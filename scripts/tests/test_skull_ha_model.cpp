#include "skull_ha_model.h"
#include "skull_jaw_command.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
using namespace skull;
namespace {
void asset(catalog::Catalog &c, size_t index, const char *path, bool skit = true) {
    assert(std::strlen(path) < sizeof(c.assets[index].path));
    std::strcpy(c.assets[index].path, path); c.assets[index].skit = skit;
    if (c.count <= index) c.count = index + 1;
}
void selection_and_discovery() {
    auto content = std::make_unique<catalog::Catalog>();
    asset(*content, 0, "/audio/Marco.wav", false);
    asset(*content, 1, "/audio/Skit - names.wav");
    asset(*content, 2, "/audio/Skit - café \"quoted\".wav");
    ha::Model model; assert(model.init("twoskulls-f42dc978ad78", false, "build-test"));
    assert(std::string(model.base_topic()) == "halloween/twoskulls-f42dc978ad78");
    assert(model.discovery(*content));
    const std::string text = model.json();
    assert(text.size() == model.size());
    assert(text.find("\"cmps\":{") != std::string::npos);
    assert(text.find("\"origin\":{") != std::string::npos);
    assert(text.find("\"expire_after\":20") != std::string::npos);
    assert(text.find("Bluetooth RSSI delta (not dBm)") != std::string::npos);
    assert(text.find("\"Skit - names.wav\"") != std::string::npos);
    assert(text.find("Skit - café \\\"quoted\\\".wav") != std::string::npos);
    assert(text.find("Marco.wav") == std::string::npos);
    assert(text.find("\"payload_press\":\"PLAY\",\"qos\":0,\"retain\":false") != std::string::npos);
    assert(text.find("/play_available") != std::string::npos);
    assert(text.find("/logs_available") != std::string::npos);
    assert(text.find("\"saved_reports\":{\"platform\":\"button\"") != std::string::npos);
    assert(text.find("\"payload_press\":\"REFRESH\",\"qos\":0,\"retain\":false") != std::string::npos);
    assert(text.find("\"platform\":\"select\"") != std::string::npos);
    assert(text.find("\"default_entity_id\":\"number.jeff_jaw_target\"") != std::string::npos);
    assert(text.find("\"default_entity_id\":\"button.jeff_jaw_stop\"") != std::string::npos);
    assert(text.find("\"default_entity_id\":\"sensor.jeff_jaw_closed_us\"") != std::string::npos);
    assert(text.find("/jaw/set") != std::string::npos);
    assert(text.find("/jaw_available") != std::string::npos);
    assert(text.find("\"command_template\":\"TARGET:{{ value | int }}\"") != std::string::npos);
    assert(text.find("\"optimistic\":false") != std::string::npos);
    for (const char *payload : {"ENTER", "STOP", "RESUME", "STEP:CLOSE1", "STEP:OPEN1", "STEP:CLOSE10", "STEP:OPEN10", "CLOSED", "OPEN", "SAVE", "DISCARD"}) {
        const std::string expected = std::string("\"payload_press\":\"") + payload + "\",\"qos\":0,\"retain\":false";
        assert(text.find(expected) != std::string::npos);
    }
    assert(std::string(ha::skit_path(*content, "Skit - names.wav")) == "/audio/Skit - names.wav");
    assert(!ha::skit_path(*content, "names")); assert(!ha::skit_path(*content, "Marco.wav"));
    assert(!ha::skit_path(*content, "/audio/Skit - names.wav"));
    asset(*content, 3, "/audio/Skit - names.wav");
    assert(!ha::skit_path(*content, "Skit - names.wav"));
    assert(!model.discovery(*content) && !model.size() && !*model.json());
    content->count = 1; assert(model.discovery(*content));
    assert(!std::strstr(model.json(), "\"platform\":\"select\""));
    assert(!std::strstr(model.json(), "Play selected skit"));
    assert(std::strstr(model.json(), "Open saved reports"));
}
void state_types_and_bounds() {
    ha::Model model; assert(model.init("jeff", false, "a\"b"));
    ha::Snapshot s;
    s.show_state = "idle\n\t\r\b\f\1"; s.current_skit = "a\\b\"c";
    s.selected_skit = "Skit - café.wav"; s.command_result = "rejected_not_ready";
    s.sd = true; s.error_pending = true; s.uptime_s = 1234567890123ULL;
    s.free_heap = 12345; s.min_free_heap = 10000; s.report_id = UINT64_MAX; s.report_count = 8; s.report_slots = 255;
    assert(model.state(s));
    assert(std::strstr(model.json(), "\"speaker\":false"));
    assert(std::strstr(model.json(), "\"sd\":true"));
    assert(std::strstr(model.json(), "\"wifi_rssi_dbm\":null"));
    for (const char *key : {"control_ready_ms", "sd_mount_ms", "sd_config_ms", "speaker_connected_ms", "bluetooth_rssi_delta"}) {
        const std::string expected = std::string("\"") + key + "\":null";
        assert(std::strstr(model.json(), expected.c_str()));
    }
    assert(std::strstr(model.json(), "\"report_id\":\"ffffffffffffffff\""));
    assert(std::strstr(model.json(), "\"report_count\":8"));
    assert(std::strstr(model.json(), "\"report_slots\":255"));
    assert(std::strstr(model.json(), "\"uptime_s\":1234567890123"));
    assert(std::strstr(model.json(), "idle\\u000a\\u0009\\u000d\\u0008\\u000c\\u0001"));
    s.wifi_rssi_valid = true; s.wifi_rssi_dbm = -71;
    s.control_ready_ms = 3409; s.sd_mount_ms = 111; s.sd_config_ms = 141;
    s.speaker_connected_ms = 60000; s.bluetooth_rssi_delta = -4;
    s.sd_attempts = 2; s.sd_failures = 1; s.speaker_disconnects = 3;
    assert(model.state(s));
    for (const char *field : {"\"control_ready_ms\":3409", "\"sd_mount_ms\":111", "\"sd_config_ms\":141",
                              "\"speaker_connected_ms\":60000", "\"bluetooth_rssi_delta\":-4", "\"sd_attempts\":2",
                              "\"sd_failures\":1", "\"speaker_disconnects\":3"}) assert(std::strstr(model.json(), field));
    assert(std::strstr(model.json(), "\"wifi_rssi_dbm\":-71"));
    const std::string giant(ha::Model::capacity * 2, 'x'); s.current_skit = giant.c_str();
    assert(!model.state(s) && !model.size() && !*model.json());
    s.current_skit = nullptr; assert(!model.state(s) && !*model.json());
    s.current_skit = "normal"; assert(model.state(s)); // failure is recoverable
    const char malformed[] = {'x',char(0xe0),char(0x80),char(0x80),0};
    s.current_skit = malformed; assert(!model.state(s));
    assert(!model.init("invalid/id", true, "v1"));
    assert(!model.state(ha::Snapshot{}));
    assert(!model.init("UpperCase", true, "v1"));
    assert(!model.init("", true, "v1"));
}
void idle_display_and_slot_mask() {
    ha::Model model; assert(model.init("jeff", false, "0.2.0-ha"));
    ha::Snapshot s; s.show_state = "idle";
    assert(model.state(s)); assert(std::strstr(model.json(), "\"current_skit\":\"Idle\""));
    for (const char *state : {"preparing", "playing", "canceling", "", "Idle"}) {
        s.show_state = state;
        assert(model.state(s)); assert(std::strstr(model.json(), "\"current_skit\":\"Unknown\""));
    }
    s.show_state = "playing"; s.current_skit = "Skit - names.wav";
    assert(model.state(s)); assert(std::strstr(model.json(), "\"current_skit\":\"Skit - names.wav\""));
    s.report_slots = 256;
    assert(!model.state(s) && !model.size() && !*model.json());
    s.report_slots = 0; s.current_skit = ""; s.show_state = nullptr;
    assert(!model.state(s) && !model.size());
    s.show_state = "idle"; assert(model.state(s));
}
void malformed_catalog_and_capacity() {
    auto content = std::make_unique<catalog::Catalog>();
    ha::Model model; assert(model.init("anthony", true, "v1"));
    asset(*content, 0, "/audio/sub/file.wav"); assert(!ha::skit_option(content->assets[0]));
    assert(!model.discovery(*content));
    asset(*content, 0, "/audio/../file.wav"); assert(!model.discovery(*content));
    asset(*content, 0, "/audio/not-wave.txt"); assert(!model.discovery(*content));
    asset(*content, 0, "/audio/Skit - upper.WAV"); assert(model.discovery(*content));
    assert(std::string(ha::skit_path(*content, "Skit - upper.WAV")) == "/audio/Skit - upper.WAV");
    std::memset(content->assets[0].path, 'a', sizeof(content->assets[0].path));
    assert(!model.discovery(*content));
    for (size_t i = 0; i < catalog::max_assets; ++i) {
        std::string path = "/audio/Skit - " + std::to_string(i) + std::string(107, 'x') + ".wav";
        asset(*content, i, path.c_str());
    }
    assert(model.discovery(*content) && model.size() < ha::Model::capacity);
    assert(std::strstr(model.json(), ha::skit_option(content->assets[catalog::max_assets - 1])));
    for (size_t i = 0; i < catalog::max_assets; ++i) {
        std::string path = "/audio/" + std::to_string(i) + std::string(100, '\1') + ".wav";
        asset(*content, i, path.c_str());
    }
    assert(!model.discovery(*content) && !model.size() && !*model.json());
    content->count = catalog::max_assets + 1; assert(!model.discovery(*content));
    assert(!ha::skit_path(*content, "anything"));
}
void jaw_protocol_and_state() {
    jawcommand::Request request;
    for (const char *text : {"ENTER", "STOP", "RESUME", "STEP:+1", "STEP:-1", "STEP:+10", "STEP:-10", "STEP:OPEN1", "STEP:CLOSE1", "STEP:OPEN10", "STEP:CLOSE10", "TARGET:544", "TARGET:2400", "CLOSED", "OPEN", "SAVE", "DISCARD"})
        assert(jawcommand::parse(text,std::strlen(text),request));
    assert(jawcommand::parse("STEP:CLOSE10",12,request));
    assert(request.action==jawcommand::Action::step&&request.value==-10&&request.relative_open);
    assert(jawcommand::parse("TARGET:1368",11,request));
    assert(request.action==jawcommand::Action::target&&request.value==1368);
    for (const char *text : {"", "enter", " ENTER", "ENTER ", "ENTER\n", "TARGET:", "TARGET:543", "TARGET:2401", "TARGET:-544", "TARGET:+544", "TARGET:544.0", "TARGET:544x", "TARGET:1e3", "TARGET:99999999999999999999999", "STEP:+25", "STEP:0", "STOP ignored"})
        assert(!jawcommand::parse(text,std::strlen(text),request));
    assert(!jawcommand::parse(nullptr,4,request));
    const char embedded[]={'S','T','O','P',0,'X'};
    assert(!jawcommand::parse(embedded,sizeof embedded,request));
    ha::Model model; assert(model.init("jeff",false,"test"));
    ha::Snapshot s;s.jaw_mode="manual";s.jaw_result="saved_to_sd";
    s.jaw_command_us=600;s.jaw_applied_us=600;s.jaw_closed_us=600;s.jaw_open_us=1400;
    s.jaw_draft_closed_us=610;s.jaw_draft_open_us=1400;s.jaw_dirty=true;
    assert(model.state(s));
    for(const char *field : {"\"jaw_mode\":\"manual\"", "\"jaw_result\":\"saved_to_sd\"", "\"jaw_command_us\":600", "\"jaw_applied_us\":600", "\"jaw_closed_us\":600", "\"jaw_open_us\":1400", "\"jaw_draft_closed_us\":610", "\"jaw_draft_open_us\":1400", "\"jaw_dirty\":true"})assert(std::strstr(model.json(),field));
    s.jaw_mode="disabled";s.jaw_applied_us=0;assert(model.state(s));
    s.jaw_mode="unrecognized";assert(!model.state(s));
    s.jaw_mode="automatic";s.jaw_command_us=543;assert(!model.state(s));
    s.jaw_command_us=600;s.jaw_applied_us=2401;assert(!model.state(s));
    s.jaw_applied_us=600;s.jaw_closed_us=1400;assert(!model.state(s));
    s.jaw_closed_us=2400;s.jaw_open_us=544;assert(model.state(s)); // reversed is valid
    s.jaw_draft_closed_us=1400;s.jaw_draft_open_us=1400;assert(model.state(s)); // unsavable draft is visible
}
void emit_fixtures() {
    auto content = std::make_unique<catalog::Catalog>();
    asset(*content, 0, "/audio/Marco.wav", false);
    asset(*content, 1, "/audio/Skit - names.wav");
    asset(*content, 2, "/audio/Skit - café \"quoted\".wav");
    ha::Model model; assert(model.init("twoskulls-f42dc978ad78", false, "v1\"test"));
    assert(model.discovery(*content)); std::puts(model.json());
    ha::Snapshot s; s.show_state = "idle\n"; s.selected_skit = "Skit - names.wav";
    s.command_result = "rejected_not_ready"; s.report_id = UINT64_MAX;
    assert(model.state(s)); std::puts(model.json());
}
}
int main(int argc, char **argv) {
    jaw_protocol_and_state(); selection_and_discovery(); state_types_and_bounds(); idle_display_and_slot_mask(); malformed_catalog_and_capacity();
    if (argc > 2 && std::string(argv[1]) == "--catalog") {
        auto content = std::make_unique<catalog::Catalog>();
        assert(argc - 2 <= static_cast<int>(catalog::max_assets));
        for (int i = 2; i < argc; ++i) asset(*content, static_cast<size_t>(i - 2), argv[i]);
        ha::Model model; assert(model.init("twoskulls-f42dc978ad78", false, "1fedf38fe36113cf367f20233ded4ca0ffaca3517a008b4203b765e597e91e7c"));
        assert(model.discovery(*content)); std::puts(model.json());
    } else if (argc > 1) emit_fixtures();
    else std::puts("HA payload bounds, discovery, exact options, types and escaping passed");
}
