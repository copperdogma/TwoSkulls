// Exercise the actual radio worker and state handlers with explicit async SDK
// events and a virtual clock; queued API acceptance never implies completion.
#include <cassert>
#include <cstring>
#include <stdexcept>
#include <vector>
#include "../../firmware/main/skull_radio.cpp"
namespace skull::secondary {
void gap_event(esp_gap_ble_cb_event_t,esp_ble_gap_cb_param_t*) {}
}
struct Tick { int64_t time; bool event; skull::radio::Event value; };
struct StopWorker {};
void ticks(std::vector<Tick> script) {
    size_t index=0;
    radio_mock::receive=[&](void* target) {
        if (index==script.size()) throw StopWorker{};
        const auto& tick=script[index++];
        radio_mock::time=tick.time;
        if (tick.event) *static_cast<skull::radio::Event*>(target)=tick.value;
        return tick.event ? pdTRUE : 0;
    };
    try { skull::radio::worker(nullptr); } catch (StopWorker&) {}
}

using namespace skull::radio;
bool primary_under_test = true;
void reset(bool cached=true) {
    radio_mock::time=1;
    radio_mock::connects=radio_mock::disconnects=radio_mock::inquiries=radio_mock::media_requests=0;
    radio_mock::connect_result=radio_mock::inquiry_result=radio_mock::rssi_result=ESP_OK;
    radio_mock::rssi_requests=0; radio_mock::logs.clear();
    speaker_attempt_us=speaker_connected_us=next_rssi_us=rssi_pending_until=0;
    speaker_attempt_count=speaker_disconnect_count=0;
    faulted=false; overflow=false; attached=true; own_iface=1;
    speaker_up=peer_up=media_ready=false;
    primary_role=primary_under_test; peer_discovery_enabled=false;
    inquiry=found_speaker=speaker_connecting=streaming=false;
    known_speaker=cached; saved_speaker=true;
    reconnect_attempts=0; reconnect_at=0;
    speaker_acl_up=await_acl_down=false;
    acl_wait_deadline=speaker_connect_deadline=0;
    close_deadline=media_retry_at=setup_deadline=0;
    media_attempts=0;
    std::strcpy(speaker_name,"Late Speaker");
    std::memset(speaker_address,0x42,6);
}
Event state(int code,int value) {
    Event e{}; e.code=code;e.a=value;
    std::memcpy(e.address,speaker_address,6);
    return e;
}
void advance(int64_t delta) { ticks({{radio_mock::time+delta,false,{}}}); }
void disconnected() { handle_a2dp(state(ESP_A2D_CONNECTION_STATE_EVT,ESP_A2D_CONNECTION_STATE_DISCONNECTED)); }
void inquiry_stopped() { handle_classic(state(ESP_BT_GAP_DISC_STATE_CHANGED_EVT,0)); }
void connected() { handle_a2dp(state(ESP_A2D_CONNECTION_STATE_EVT,ESP_A2D_CONNECTION_STATE_CONNECTED)); }
void test_late_speaker_after_many_episodes() {
    reset(); seek();
    for (unsigned episode=0;episode<20;++episode) {
        for(unsigned attempt=0;attempt<3;++attempt) {
            assert(speaker_connecting);
            const unsigned before=radio_mock::connects;
            advance(1000000); assert(radio_mock::connects==before);
            disconnected();
            advance(4999999); assert(radio_mock::connects==before);
            advance(1);
        }
        assert(inquiry && !speaker_connecting);
        const unsigned before=radio_mock::inquiries;
        inquiry_stopped();
        advance(4999999); assert(radio_mock::inquiries==before);
        advance(1);
        assert(speaker_connecting); // A new complete episode, indefinitely.
    }
    assert(radio_mock::connects==61 && radio_mock::inquiries==20);
    connected();
    Event ack{};ack.code=ESP_A2D_MEDIA_CTRL_ACK_EVT;ack.a=ESP_A2D_MEDIA_CTRL_START;ack.status=ESP_A2D_MEDIA_CTRL_ACK_SUCCESS;
    handle_a2dp(ack);
    const unsigned before=radio_mock::connects;
    advance(1000000000);
    assert(speaker_connected() && stream_ready());
    assert(radio_mock::connects==before);
}
void test_no_cached_speaker_discovery_and_failed_discovered_open() {
    reset(false); seek();
    for(int cycle=0;cycle<8;++cycle) {
        assert(inquiry); inquiry_stopped();advance(5000000);
    }
    assert(radio_mock::inquiries==9 && radio_mock::connects==0);
    auto found=state(ESP_BT_GAP_DISC_RES_EVT,0);std::strcpy(found.text,speaker_name);
    handle_classic(found);inquiry_stopped();
    assert(speaker_connecting && radio_mock::connects==1);
    disconnected();advance(5000000);
    assert(speaker_connecting && radio_mock::connects==2);
    connected();assert(speaker_connected());
}
void test_api_rejections_remain_rate_limited() {
    reset();radio_mock::connect_result=ESP_ERR_INVALID_STATE;radio_mock::inquiry_result=ESP_ERR_INVALID_STATE;
    seek();assert(radio_mock::connects==1);
    for(unsigned tick=0;tick<30;++tick) {
        const auto before=radio_mock::connects;
        advance(4999999);assert(radio_mock::connects==before);
        advance(1);assert(radio_mock::connects==before+1);
    }
    assert(radio_mock::connects==31 && radio_mock::inquiries==10);
    radio_mock::connect_result=ESP_OK;
    advance(5000000);assert(speaker_connecting);
    connected();assert(speaker_connected());
}
void test_pending_connection_past_old_timeout() {
    reset();seek();
    for(int bound=0;bound<6;++bound) advance(75000000);
    assert(speaker_connecting && radio_mock::connects==1 && !faulted.load());
    disconnected();advance(5000000);
    assert(speaker_connecting && radio_mock::connects==2);
    connected();assert(speaker_connected());
}
void test_acl_teardown_late_and_ordering() {
    reset();seek();
    auto acl=state(ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT,128);acl.status=ESP_BT_STATUS_SUCCESS;
    handle_classic(acl);disconnected();
    assert(await_acl_down);
    for(int bound=0;bound<5;++bound) advance(60000000);
    assert(radio_mock::connects==1 && await_acl_down && !faulted.load());
    auto down=state(ESP_BT_GAP_ACL_DISCONN_CMPL_STAT_EVT,128);
    handle_classic(down);advance(4999999);assert(radio_mock::connects==1);
    advance(1);assert(radio_mock::connects==2);
    // ACL completion can precede the corresponding A2DP completion. Never open
    // over that still-pending A2DP attempt.
    handle_classic(acl);handle_classic(down);advance(5000000);
    assert(radio_mock::connects==2 && speaker_connecting);
    disconnected();advance(5000000);assert(radio_mock::connects==3);
}
void test_media_retry_survives_old_budget() {
    reset();seek();connected();assert(radio_mock::media_requests==1);
    advance(2000000);advance(2000000);
    assert(radio_mock::media_requests==3);
    for(int attempt=0;attempt<20;++attempt) {
        const auto before=radio_mock::media_requests;
        advance(4999999);assert(radio_mock::media_requests==before);
        advance(1);assert(radio_mock::media_requests==before+1);
    }
    Event ack{};ack.code=ESP_A2D_MEDIA_CTRL_ACK_EVT;ack.a=ESP_A2D_MEDIA_CTRL_START;ack.status=ESP_A2D_MEDIA_CTRL_ACK_SUCCESS;
    handle_a2dp(ack);const auto before=radio_mock::media_requests;
    advance(1000000000);assert(radio_mock::media_requests==before && stream_ready());
}
void test_established_session_loss() {
    reset(); seek(); connected();
    const auto before = radio_mock::connects;
    disconnected();
    advance(4999999); assert(radio_mock::connects == before);
    advance(1); assert(radio_mock::connects == before + 1);
    connected(); assert(speaker_connected());
}

bool logged(const char *text) {
    return std::any_of(radio_mock::logs.begin(), radio_mock::logs.end(),
                       [text](const std::string &line) { return line.find(text) != std::string::npos; });
}
void test_lifecycle_timings_and_rssi_cadence() {
    reset(); seek();
    advance(2000000); connected();
    assert(logged("event=speaker_connect_attempt"));
    assert(logged("attempt_ms=2000"));
    advance(1); assert(radio_mock::rssi_requests==1 && rssi_pending_until);
    Event sample=state(ESP_BT_GAP_READ_RSSI_DELTA_EVT,-12);sample.status=ESP_BT_STATUS_SUCCESS;
    handle_classic(sample);
    assert(!rssi_pending_until && logged("rssi_delta=-12 units=controller_delta_not_dbm"));
    advance(29999999); assert(radio_mock::rssi_requests==1);
    advance(1); assert(radio_mock::rssi_requests==2);
    advance(5000000); assert(!rssi_pending_until && logged("state=timeout"));
    disconnected();
    assert(logged("event=speaker_disconnected") && logged("was_connected=1"));
    assert(speaker_disconnect_count==1);
    const auto count=radio_mock::rssi_requests;
    advance(1000000);handle_classic(sample);
    assert(radio_mock::rssi_requests==count && !rssi_pending_until);
    // A controller failure stays observable and respects the same sample rate.
    connected();radio_mock::rssi_result=ESP_ERR_INVALID_STATE;
    advance(1);assert(logged("state=request_failed"));
    advance(29999999);assert(radio_mock::rssi_requests==count+1);
    advance(1);assert(radio_mock::rssi_requests==count+2);
}

int main() {
    for (bool primary : {true, false}) {
    primary_under_test = primary;
    test_late_speaker_after_many_episodes();
    test_no_cached_speaker_discovery_and_failed_discovered_open();
    test_api_rejections_remain_rate_limited();
    test_pending_connection_past_old_timeout();
    test_acl_teardown_late_and_ordering();
    test_media_retry_survives_old_budget();
    test_established_session_loss();
    test_lifecycle_timings_and_rssi_cadence();
    }
    puts("Radio recovery: primary and secondary production-source virtual-clock scenarios passed.");
}
