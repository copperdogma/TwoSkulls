#include "skull_show.h"
#include "skull_audio.h"
#include "skull_catalog.h"
#include "skull_radio.h"
#include "skull_actuators.h"
#include "skull_secondary.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"
#include <atomic>
#include <cstring>

namespace skull::show {
namespace {
constexpr char tag[] = "skull_show";
enum class State { idle, preparing, awaiting_peer, playing, cancelling };
State state = State::idle;
catalog::Catalog *assets = nullptr;
const catalog::Asset *selected = nullptr;
bool primary = true, initialization_seen = false, coordinated = false;
bool discovery_enabled = false;
uint32_t incoming_token = 0;
std::atomic<bool> accepting{false}, requested{false};
int64_t deadline = 0, cooldown_until = 0, marco_at = 0;
int64_t now() { return esp_timer_get_time(); }
void discovery(bool enabled) {
    if (enabled != discovery_enabled) {
        discovery_enabled = enabled;
        radio::set_peer_discovery(enabled);
    }
}
const catalog::Asset *find(const char *path) {
    if (!assets || !path) return nullptr;
    for (size_t i = 0; i < assets->count; ++i)
        if (!std::strcmp(path, assets->assets[i].path)) return &assets->assets[i];
    return nullptr;
}
bool begin(const catalog::Asset *next, bool pair) {
    if (!next || state != State::idle || !radio::stream_ready() || audio::busy()) return false;
    if (!audio::prepare(next)) return false;
    selected = next; coordinated = pair;
    accepting.store(false);
    discovery(!pair && !std::strcmp(next->path, "/audio/Marco.wav"));
    deadline = now() + 5000000;
    state = State::preparing;
    ESP_LOGI(tag, "Preparing %s [%s]", selected->path, pair ? "paired show" : "local/standalone");
    return true;
}
void abort(const char *reason) {
    accepting.store(false);
    ESP_LOGW(tag, "Playback canceled: %s; no automatic replay", reason);
    if (incoming_token) { secondary::respond(incoming_token, false); incoming_token = 0; }
    audio::cancel();
    state = State::cancelling;
}
}

void init(catalog::Catalog *content, bool is_primary) {
    assets = content; primary = is_primary;
}
bool trigger() {
    if (!accepting.exchange(false)) return false;
    bool expected = false;
    return requested.compare_exchange_strong(expected, true);
}
bool local(const char *path) {
    if (!initialization_seen || requested.load()) return false;
    return begin(find(path), false);
}
bool busy() { return state != State::idle || requested.load(); }
const char *state_name() {
    switch (state) {
    case State::idle: return "idle";
    case State::preparing: return "preparing";
    case State::awaiting_peer: return "awaiting-peer";
    case State::playing: return "playing";
    case State::cancelling: return "canceling";
    }
    return "invalid";
}
void tick(bool initialization_finished, bool startup_active) {
    const int64_t time = now();
    if (!primary) {
        secondary::busy(!initialization_finished || startup_active || state != State::idle || audio::busy());
        secondary::poll();
        secondary::advertising(initialization_finished);
    }
    if (!initialization_seen && initialization_finished) {
        initialization_seen = true;
        cooldown_until = time + 10000000;
        ESP_LOGI(tag, "Initialization finished; direct peer discovery enabled while idle");
    }
    if (!initialization_seen || startup_active) {
        accepting.store(false); discovery(false); return;
    }
    if (!primary) {
        if (secondary::take_connected() && state == State::idle && radio::stream_ready() && !audio::busy())
            begin(find("/audio/Polo.wav"), false);
        secondary::Request incoming{};
        if (secondary::take_request(incoming)) {
            if (state == State::idle && radio::stream_ready() && begin(find(incoming.path), false)) {
                incoming_token = incoming.token;
                deadline = time + 2500000; // Below the server's 3 s and primary's 5 s bounds.
            } else secondary::respond(incoming.token, false);
        }
    }
    if (state != State::idle && state != State::cancelling &&
        (!radio::stream_ready() || audio::error())) abort("speaker unavailable or SD/PCM failure");
    if (state == State::preparing) {
        if (audio::ready()) {
            if (coordinated) {
                radio::clear_peer_result();
                if (!radio::peer_ready() || radio::request_file(selected->path) != ESP_OK)
                    abort("Jeff unavailable before request");
                else { state = State::awaiting_peer; deadline = time + 6000000; }
            } else if (incoming_token) {
                if (!secondary::connected() || !secondary::respond(incoming_token, true))
                    abort("secondary request expired/disconnected or ACK could not queue");
                else {
                    incoming_token = 0;
                    if (audio::start()) {
                        state = State::playing;
                        ESP_LOGI(tag, "Secondary ACK queued; playing %s", selected->path);
                    } else abort("secondary prepared audio could not start");
                }
            } else if (audio::start()) {
                state = State::playing;
                ESP_LOGI(tag, "Playing %s", selected->path);
            } else abort("prepared audio could not start");
        } else if (time >= deadline) abort("local prebuffer timeout");
    } else if (state == State::awaiting_peer) {
        const auto reply = radio::peer_result();
        if (reply == radio::PeerResult::accepted) {
            if (!radio::peer_ready()) abort("Jeff disconnected after ACK");
            else if (audio::start()) {
                state = State::playing;
                ESP_LOGI(tag, "Jeff ACK/readback accepted; local playback %s (not proof of Jeff sound)", selected->path);
            } else abort("local start failed after peer reply");
        } else if (reply != radio::PeerResult::pending || time >= deadline)
            abort("Jeff rejected/disconnected/timed out");
    } else if (state == State::playing && audio::complete()) {
        const auto stats = audio::stats();
        ESP_LOGI(tag, "Playback complete: %s frames=%lu underruns=%lu", selected->path,
                 static_cast<unsigned long>(stats.frames), static_cast<unsigned long>(stats.underruns));
        cooldown_until = time + 10000000;
        radio::clear_peer_result();
        state = State::idle;
    } else if (state == State::cancelling && !audio::busy() &&
               radio::peer_result() != radio::PeerResult::pending) {
        radio::clear_peer_result();
        cooldown_until = time + 10000000;
        state = State::idle;
    }
    bool marco_due = false;
    if (primary && radio::peer_scanning()) {
        if (!marco_at) marco_at = time + 5000000;
        if (time >= marco_at) { marco_due = true; marco_at = time + 5000000; }
    } else marco_at = 0;
    if (state == State::idle) {
        discovery(primary);
        const bool can_trigger = primary && assets && radio::stream_ready() && radio::peer_ready() &&
            !audio::busy() && time >= cooldown_until;
        accepting.store(can_trigger && !requested.load());
        if (requested.exchange(false)) {
            accepting.store(false);
            if (can_trigger) {
                const int index = catalog::select(*assets, static_cast<uint64_t>(time / 1000), esp_random());
                if (index >= 0 && begin(&assets->assets[index], true)) actuators::diagnostic(1);
                else ESP_LOGW(tag, "Trigger refused: no playable skit or prepare busy");
            } else ESP_LOGW(tag, "Trigger consumed: busy/cooldown/speaker/peer unavailable");
        } else if (marco_due && radio::stream_ready()) {
            begin(find("/audio/Marco.wav"), false);
        }
    } else {
        accepting.store(false);
        discovery(primary && selected && !coordinated && !std::strcmp(selected->path, "/audio/Marco.wav"));
    }
}
}
