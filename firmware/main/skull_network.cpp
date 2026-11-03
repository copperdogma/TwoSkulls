#include "skull_network.h"
#include "skull_network_config.h"
#include "skull_actuators.h"
#include "skull_startup.h"
#include "skull_jaw_control.h"
#include "skull_ha_model.h"
#include "skull_show.h"
#include "skull_storage.h"
#include "skull_radio.h"
#include "skull_secondary.h"
#include "skull_fault_log.h"
#include "skull_fault_archive.h"
#include "skull_log_export.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "mqtt_client.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include <atomic>
#include <new>
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <sys/stat.h>
namespace skull::network {
namespace {
constexpr char tag[]="skull_net";
struct Broker { uint32_t version=1; char uri[192]{},user[64]{},password[128]{}; };
struct Command { bool play=false,logs=false,jaw=false; logexport::Request log_request; jawcommand::Request jaw_request; uint32_t epoch=0; int64_t received=0; char option[catalog::path_capacity]{}; };
struct State {
    Config config;
    ha::Model model;
    catalog::Catalog *assets=nullptr;
    esp_mqtt_client_handle_t mqtt=nullptr;
    bool primary=false, wifi=false;
    std::atomic<bool> connected{false};
    std::atomic<uint32_t> discovery_generation{0};
    uint32_t queued_discovery_generation=0,published_discovery_generation=0;
    bool discovery_queued=false;
    std::atomic<unsigned> dropped{0};
    std::atomic<uint32_t> epoch{0};
    std::atomic<unsigned> subscriptions{0};
    std::atomic<int> select_subscription{-1},play_subscription{-1},log_subscription{-1},jaw_subscription{-1};
    Command command;
    bool pending=false;
    logexport::Transfer log_transfer;
    uint32_t log_epoch=0;
    int log_inflight=-1;
    int64_t log_deadline=0,next_log_frame=0;
    int published[16]{};
    unsigned published_head=0,published_count=0;
    char selected[catalog::path_capacity]{}, result[64]="none", jaw_result[64]="none";
    unsigned draft_closed=544,draft_open=1368;
    uint32_t jaw_epoch=0;
    actuators::JawMode last_jaw_mode=actuators::JawMode::disabled;
    char jaw_topic[112]{},jaw_available_topic[112]{};
    char state_topic[96]{},availability_topic[96]{},play_available_topic[112]{};
    char select_topic[112]{},play_topic[112]{},discovery_topic[128]{},log_topic[112]{},logs_available_topic[112]{};
    int64_t next_state=0,next_wifi_retry=0,next_discovery=0;
    unsigned report_count=0,report_slots=0;
    uint64_t counted_report=~uint64_t(0);
    bool counted_pending=false;
    std::atomic<int64_t> next_subscription{0};
};
State *state=nullptr;
portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
void broker_load(Config &config){
    if(*config.mqtt_uri)return;
    nvs_handle_t handle{};if(nvs_open("skulls_net",NVS_READONLY,&handle)!=ESP_OK)return;
    Broker saved{};size_t size=sizeof(saved);
    const auto result=nvs_get_blob(handle,"broker",&saved,&size);nvs_close(handle);
    if(result!=ESP_OK||size!=sizeof(saved)||saved.version!=1||
       !std::memchr(saved.uri,0,sizeof(saved.uri))||!std::memchr(saved.user,0,sizeof(saved.user))||
       !std::memchr(saved.password,0,sizeof(saved.password)))return;
    std::memcpy(config.mqtt_uri,saved.uri,sizeof(saved.uri));
    std::memcpy(config.mqtt_username,saved.user,sizeof(saved.user));
    std::memcpy(config.mqtt_password,saved.password,sizeof(saved.password));
}
bool topic_is(const esp_mqtt_event_t *event,const char *topic){
    return event->topic&&event->topic_len==int(std::strlen(topic))&&
           !std::memcmp(event->topic,topic,event->topic_len);
}
void mqtt_event(void *,esp_event_base_t,int32_t code,void *data){
    if(!state)return;
    auto &s=*state;auto *event=static_cast<esp_mqtt_event_t*>(data);
    if(code==MQTT_EVENT_CONNECTED){
        ++s.epoch;s.subscriptions=0;s.next_subscription=esp_timer_get_time()+5000000;
        s.connected=true;++s.discovery_generation;
        portENTER_CRITICAL(&mux);s.published_count=s.published_head=0;portEXIT_CRITICAL(&mux);
        s.select_subscription=esp_mqtt_client_subscribe(s.mqtt,s.select_topic,0);
        s.play_subscription=esp_mqtt_client_subscribe(s.mqtt,s.play_topic,0);
        s.log_subscription=esp_mqtt_client_subscribe(s.mqtt,s.log_topic,0);
        s.jaw_subscription=esp_mqtt_client_subscribe(s.mqtt,s.jaw_topic,0);
        esp_mqtt_client_subscribe(s.mqtt,"homeassistant/status",0);
        ESP_LOGI(tag,"MQTT connected; discovery requested");
    }else if(code==MQTT_EVENT_DISCONNECTED){
        s.connected=false;++s.epoch;s.subscriptions=0;
        portENTER_CRITICAL(&mux);s.pending=false;portEXIT_CRITICAL(&mux);
        ESP_LOGI(tag,"MQTT disconnected; command slot cleared");
    }else if(code==MQTT_USER_EVENT){
        // SDK event loop owns subscribe calls/IDs, so SUBACKs cannot race their
        // returned-ID assignment on another task. Retry only missing topics.
        if(!s.connected.load())return;
        const unsigned ready=s.subscriptions.load();
        if(!(ready&1))s.select_subscription=esp_mqtt_client_subscribe(s.mqtt,s.select_topic,0);
        if(!(ready&2))s.play_subscription=esp_mqtt_client_subscribe(s.mqtt,s.play_topic,0);
        if(!(ready&4))s.log_subscription=esp_mqtt_client_subscribe(s.mqtt,s.log_topic,0);
        if(!(ready&8))s.jaw_subscription=esp_mqtt_client_subscribe(s.mqtt,s.jaw_topic,0);
    }else if(code==MQTT_EVENT_SUBSCRIBED){
        const bool ok=event->data&&event->data_len>0&&static_cast<unsigned char>(event->data[0])<0x80;
        if(ok){
            if(event->msg_id==s.select_subscription.load())s.subscriptions.fetch_or(1);
            if(event->msg_id==s.play_subscription.load())s.subscriptions.fetch_or(2);
            if(event->msg_id==s.log_subscription.load())s.subscriptions.fetch_or(4);
            if(event->msg_id==s.jaw_subscription.load())s.subscriptions.fetch_or(8);
        }else ESP_LOGW(tag,"MQTT command subscription rejected; retry will follow");
    }else if(code==MQTT_EVENT_PUBLISHED){
        // Cache a bounded ACK window so a fast PUBACK cannot race the owner's
        // returned message-ID assignment. One export frame is outstanding.
        portENTER_CRITICAL(&mux);
        s.published[s.published_head]=event->msg_id;s.published_head=(s.published_head+1)%16;
        if(s.published_count<16)++s.published_count;
        portEXIT_CRITICAL(&mux);
    }else if(code==MQTT_EVENT_DATA){
        if(event->data_len<0||event->current_data_offset||event->data_len!=event->total_data_len)return;
        if(topic_is(event,"homeassistant/status")){
            if(event->data_len==6&&!std::memcmp(event->data,"online",6))++s.discovery_generation;
            return;
        }
        if(event->retain||event->dup||!s.connected.load()||s.subscriptions.load()!=15)return;
        Command command;command.received=esp_timer_get_time();command.epoch=s.epoch.load();
        if(topic_is(event,s.jaw_topic)){
            if(!jawcommand::parse(event->data,size_t(event->data_len),command.jaw_request))return;
            command.jaw=true;
        }else if(topic_is(event,s.log_topic)){
            if(!logexport::parse(event->data,size_t(event->data_len),command.log_request))return;
            command.logs=true;
        }else if(topic_is(event,s.play_topic)){
            if(event->data_len!=4||std::memcmp(event->data,"PLAY",4))return;
            command.play=true;
        }else if(topic_is(event,s.select_topic)){
            if(!event->data_len||size_t(event->data_len)>=sizeof(command.option)||
               std::memchr(event->data,0,event->data_len))return;
            std::memcpy(command.option,event->data,event->data_len);
        }else return;
        portENTER_CRITICAL(&mux);
        // STOP replaces queued work: disabling pulses must not wait behind a
        // select, save, step or log request. Still reject retained/duplicate STOP.
        if(!s.pending||(command.jaw&&command.jaw_request.action==jawcommand::Action::stop)){
            s.command=command;s.pending=true;
        }else ++s.dropped;
        portEXIT_CRITICAL(&mux);
    }
}
void wifi_event(void *,esp_event_base_t,int32_t code,void *data){
    if(!state)return;
    if(code==WIFI_EVENT_STA_DISCONNECTED){
        const auto *event=static_cast<wifi_event_sta_disconnected_t*>(data);
        ESP_LOGI(tag,"Wi-Fi disconnected reason=%u",unsigned(event->reason));
    }
}
bool start_wifi(State &s){
    if(s.primary){s.wifi=true;return true;}
    if(!*s.config.ssid){ESP_LOGW(tag,"No SD Wi-Fi settings; network disabled");return false;}
    const auto netif=esp_netif_init();if(netif!=ESP_OK&&netif!=ESP_ERR_INVALID_STATE)return false;
    const auto loop=esp_event_loop_create_default();if(loop!=ESP_OK&&loop!=ESP_ERR_INVALID_STATE)return false;
    if(!esp_netif_get_handle_from_ifkey("WIFI_STA_DEF")&&!esp_netif_create_default_wifi_sta())return false;
    wifi_init_config_t initial=WIFI_INIT_CONFIG_DEFAULT();
    const auto initialized=esp_wifi_init(&initial);if(initialized!=ESP_OK)return false;
    wifi_config_t station{};
    std::memcpy(station.sta.ssid,s.config.ssid,std::strlen(s.config.ssid));
    std::memcpy(station.sta.password,s.config.password,std::strlen(s.config.password));
    station.sta.pmf_cfg.capable=true;station.sta.pmf_cfg.required=false;
    if(esp_wifi_set_storage(WIFI_STORAGE_RAM)!=ESP_OK||esp_wifi_set_mode(WIFI_MODE_STA)!=ESP_OK||
       esp_wifi_set_config(WIFI_IF_STA,&station)!=ESP_OK)return false;
    if(esp_event_handler_register(WIFI_EVENT,WIFI_EVENT_STA_DISCONNECTED,wifi_event,nullptr)!=ESP_OK)return false;
    if(esp_wifi_start()!=ESP_OK)return false;
    s.wifi=true;ESP_LOGI(tag,"Secondary Wi-Fi started from SD; settings redacted");
    return true;
}
int enqueue(State &s,const char *topic,const char *payload,int qos,bool retain){
    if(!s.connected.load()||esp_mqtt_client_get_outbox_size(s.mqtt)>12000)return -1;
    return esp_mqtt_client_enqueue(s.mqtt,topic,payload,0,qos,retain,true);
}
int enqueue_log(State &s,const char *topic,const uint8_t *data,size_t length){
    if(!s.connected.load()||length>1024||esp_mqtt_client_get_outbox_size(s.mqtt)>12000)return -1;
    return esp_mqtt_client_enqueue(s.mqtt,topic,reinterpret_cast<const char*>(data),int(length),1,false,true);
}
void log_error(State &s,const logexport::Request &request,const char *error){
    char topic[160],body[80];
    std::snprintf(topic,sizeof(topic),"%s/logs/%s/error",s.model.base_topic(),request.nonce);
    const int n=std::snprintf(body,sizeof(body),"{\"error\":\"%s\"}",error);
    if(n>0&&size_t(n)<sizeof(body))enqueue_log(s,topic,reinterpret_cast<const uint8_t*>(body),size_t(n));
}
void cancel_log(State &s){s.log_transfer.cancel();s.log_inflight=-1;}
void start_log(State &s,const Command &command,bool idle,int64_t now){
    const auto &request=command.log_request;
    if(s.log_transfer.active()){log_error(s,request,"busy");return;}
    if(!idle){log_error(s,request,"busy");return;}
    if(!storage::mounted()){log_error(s,request,"sd_unavailable");return;}
    s.log_transfer.start(request);
    const auto copy=[](const uint8_t *data,size_t size,void *context)->bool{
        return static_cast<logexport::Transfer*>(context)->append(data,size);
    };
    errno=0;const bool read=faultlog::read_archive("/sdcard/diagnostics",request.slot,copy,&s.log_transfer);
    if(!read){
        const int error=errno;cancel_log(s);
        log_error(s,request,error==ENOENT?"missing":error==EFBIG?"oversized":"read_failed");return;
    }
    s.log_transfer.seal();s.log_epoch=command.epoch;s.log_deadline=now+20000000;
    s.next_log_frame=0;s.log_inflight=-1;
}
void stream_log(State &s,int64_t now){
    if(!s.log_transfer.active())return;
    if(!s.connected.load()||s.log_epoch!=s.epoch.load()){cancel_log(s);return;}
    if(now>=s.log_deadline){log_error(s,s.log_transfer.request(),"timeout");cancel_log(s);return;}
    if(s.log_inflight>=0){
        bool acknowledged=false;
        portENTER_CRITICAL(&mux);
        for(unsigned i=0;i<s.published_count;++i)if(s.published[i]==s.log_inflight)acknowledged=true;
        portEXIT_CRITICAL(&mux);
        if(!acknowledged)return;
        s.log_transfer.sent();s.log_inflight=-1;
    }
    if(!s.log_transfer.active()||now<s.next_log_frame)return;
    const char *suffix;const uint8_t *data;size_t length;
    if(!s.log_transfer.frame(suffix,data,length)){cancel_log(s);return;}
    char topic[160];std::snprintf(topic,sizeof(topic),"%s/logs/%s/%s",s.model.base_topic(),s.log_transfer.request().nonce,suffix);
    const int queued=enqueue_log(s,topic,data,length);
    if(queued>=0){s.log_inflight=queued;s.next_log_frame=now+20000;}
}
struct JawBackend {
    State &owner;
    jawcontrol::View state(){
        const auto jaw=actuators::jaw_state();
        return {jaw.mode==actuators::JawMode::manual,jaw.enabled,jaw.requested_us,jaw.applied_us,jaw.closed_us,jaw.open_us};
    }
    void disable(){actuators::jaw_disable();}
    bool enter(){
        if(!actuators::jaw_manual_enter())return false;
        owner.jaw_epoch=owner.epoch.load();return true;
    }
    bool resume(){return actuators::jaw_resume();}
    bool target(unsigned pulse){return actuators::jaw_manual_target(pulse);}
    bool step(int delta){return actuators::jaw_manual_step(delta);}
    bool save(unsigned closed,unsigned open){return storage::set_jaw_limits(closed,open)==ESP_OK;}
    bool configure(unsigned closed,unsigned open){return actuators::configure_jaw(closed,open);}
};
const char *jaw_command(State &s,const jawcommand::Request &request,bool idle){
    JawBackend backend{s};jawcontrol::Draft draft{s.draft_closed,s.draft_open};
    const jawcontrol::Context context{idle,startup::finished(),s.log_transfer.active()};
    const char *result=jawcontrol::apply(request,context,draft,backend);
    s.draft_closed=draft.closed_us;s.draft_open=draft.open_us;
    return result;
}
void count_reports(State &s){
    s.report_count=s.report_slots=0;
    if(!storage::mounted())return;
    for(unsigned i=0;i<8;++i){
        char path[64];std::snprintf(path,sizeof(path),"/sdcard/diagnostics/fault-%u.log",i);
        struct stat info{};if(stat(path,&info)==0&&S_ISREG(info.st_mode)){++s.report_count;s.report_slots|=1u<<i;}
    }
}
}
void init(bool primary,catalog::Catalog *assets){
    if(state||!assets)return;
    void *memory=heap_caps_malloc(sizeof(State),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!memory){ESP_LOGW(tag,"Network allocation unavailable; local operation continues");return;}
    state=new(memory)State;auto &s=*state;s.primary=primary;s.assets=assets;
    FILE *file=std::fopen("/sdcard/config.txt","rb");
    const bool parsed=read_config(file,s.config);if(file)std::fclose(file);
    if(!parsed){ESP_LOGW(tag,"Network config invalid; credentials redacted; local operation continues");return;}
    broker_load(s.config);
    uint8_t mac[6]{};if(esp_read_mac(mac,ESP_MAC_WIFI_STA)!=ESP_OK)return;
    char id[48];std::snprintf(id,sizeof(id),"twoskulls_%02x%02x%02x%02x%02x%02x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    const auto *app=esp_app_get_description();char firmware[65];
    std::snprintf(firmware,sizeof(firmware),"%.32s+",app->version);const size_t label_length=std::strlen(firmware);
    for(size_t i=0;i<6;++i)std::snprintf(firmware+label_length+i*2,3,"%02x",app->app_elf_sha256[i]);
    if(!s.model.init(id,primary,firmware))return;
    const char *base=s.model.base_topic();
    std::snprintf(s.state_topic,sizeof(s.state_topic),"%s/state",base);
    std::snprintf(s.availability_topic,sizeof(s.availability_topic),"%s/availability",base);
    std::snprintf(s.play_available_topic,sizeof(s.play_available_topic),"%s/play_available",base);
    std::snprintf(s.select_topic,sizeof(s.select_topic),"%s/select/set",base);
    std::snprintf(s.play_topic,sizeof(s.play_topic),"%s/play",base);
    std::snprintf(s.log_topic,sizeof(s.log_topic),"%s/logs/get",base);
    std::snprintf(s.jaw_topic,sizeof(s.jaw_topic),"%s/jaw/set",base);
    std::snprintf(s.jaw_available_topic,sizeof(s.jaw_available_topic),"%s/jaw_available",base);
    const auto jaw=actuators::jaw_state();s.draft_closed=jaw.closed_us;s.draft_open=jaw.open_us;
    s.last_jaw_mode=jaw.mode;
    std::snprintf(s.logs_available_topic,sizeof(s.logs_available_topic),"%s/logs_available",base);
    std::snprintf(s.discovery_topic,sizeof(s.discovery_topic),"homeassistant/device/%s/config",id);
    for(size_t i=0;i<assets->count;++i){const char *name=ha::skit_option(assets->assets[i]);
        if(name&&ha::skit_path(*assets,name)){std::snprintf(s.selected,sizeof(s.selected),"%s",name);break;}}
    count_reports(s);
    if(!start_wifi(s)){ESP_LOGW(tag,"Wi-Fi unavailable; local operation continues");return;}
    if(!*s.config.mqtt_uri){ESP_LOGW(tag,"Broker not configured; use idle USB mqtt-config then reboot");return;}
    esp_mqtt_client_config_t config{};
    config.broker.address.uri=s.config.mqtt_uri;
    if(!std::strncmp(s.config.mqtt_uri,"mqtts://",8))config.broker.verification.crt_bundle_attach=esp_crt_bundle_attach;
    config.credentials.client_id=s.model.id();
    config.credentials.username=*s.config.mqtt_username?s.config.mqtt_username:nullptr;
    config.credentials.authentication.password=*s.config.mqtt_password?s.config.mqtt_password:nullptr;
    config.session.last_will.topic=s.availability_topic;config.session.last_will.msg="offline";
    config.session.last_will.qos=1;config.session.last_will.retain=1;config.session.keepalive=15;
    config.network.reconnect_timeout_ms=5000;config.task.stack_size=6144;
    config.buffer.size=1024;config.buffer.out_size=1024;config.outbox.limit=32768;
    s.mqtt=esp_mqtt_client_init(&config);
    if(!s.mqtt||esp_mqtt_client_register_event(s.mqtt,MQTT_EVENT_ANY,mqtt_event,nullptr)!=ESP_OK||
       esp_mqtt_client_start(s.mqtt)!=ESP_OK){ESP_LOGW(tag,"MQTT start unavailable; local operation continues");return;}
    ESP_LOGI(tag,"HA MQTT initialized role=%s skits catalog=%u; credentials redacted",primary?"primary":"secondary",unsigned(assets->count));
}
void poll(bool idle){
    if(!state)return;
    auto &s=*state;const int64_t now=esp_timer_get_time();
    if(s.wifi&&!s.primary&&now>=s.next_wifi_retry){
        s.next_wifi_retry=now+10000000;wifi_ap_record_t ap{};
        if(esp_wifi_sta_get_ap_info(&ap)!=ESP_OK)esp_wifi_connect();
    }
    // MQTT callbacks never touch the actuator. The control owner observes both
    // a disconnect and even a complete reconnect between polls via its epoch.
    auto jaw=actuators::jaw_state();
    wifi_ap_record_t manual_ap{};
    const bool manual_wifi_lost=jaw.mode==actuators::JawMode::manual&&esp_wifi_sta_get_ap_info(&manual_ap)!=ESP_OK;
    if(jaw.mode==actuators::JawMode::manual&&
       (!s.mqtt||!s.connected.load()||manual_wifi_lost||s.jaw_epoch!=s.epoch.load())){
        actuators::jaw_disable();std::snprintf(s.jaw_result,sizeof(s.jaw_result),"link_lost_disable_requested");
        s.next_state=0;jaw=actuators::jaw_state();
    }
    if(s.last_jaw_mode==actuators::JawMode::manual&&jaw.mode==actuators::JawMode::disabled&&
       !std::strcmp(s.jaw_result,"link_lost_disable_requested"))s.next_state=0;
    else if(s.last_jaw_mode==actuators::JawMode::manual&&jaw.mode==actuators::JawMode::disabled){
        std::snprintf(s.jaw_result,sizeof(s.jaw_result),"manual_timeout_disable_requested");s.next_state=0;
    }
    if(jaw.faulted){
        const char *result=jaw.enabled?"actuator_fault_disable_pending":"actuator_fault_pulses_disabled";
        if(std::strcmp(s.jaw_result,result)){
            std::snprintf(s.jaw_result,sizeof(s.jaw_result),"%s",result);s.next_state=0;
        }
    }
    s.last_jaw_mode=jaw.mode;
    show::set_jaw_inhibited(jaw.mode!=actuators::JawMode::automatic);
    if(!s.mqtt||!s.connected.load()){cancel_log(s);return;}
    if(s.subscriptions.load()!=15&&now>=s.next_subscription.load()){
        s.next_subscription=now+5000000;
        esp_mqtt_event_t retry{};esp_mqtt_dispatch_custom_event(s.mqtt,&retry);
    }
    const auto faults=faultlog::snapshot();
    if(idle&&(faults.id!=s.counted_report||faults.pending!=s.counted_pending)){
        count_reports(s);s.counted_report=faults.id;s.counted_pending=faults.pending;
    }
    // Finish an acknowledged END before accepting the next slot request.
    stream_log(s,now);
    Command command;bool pending;
    portENTER_CRITICAL(&mux);pending=s.pending;if(pending){command=s.command;s.pending=false;}portEXIT_CRITICAL(&mux);
    if(pending&&command.jaw){
        const char *result="expired";
        if(s.connected.load()&&command.epoch==s.epoch.load()&&now-command.received<=2000000)
            result=jaw_command(s,command.jaw_request,idle);
        std::snprintf(s.jaw_result,sizeof(s.jaw_result),"%s",result);s.next_state=0;
        s.last_jaw_mode=actuators::jaw_state().mode;
        show::set_jaw_inhibited(s.last_jaw_mode!=actuators::JawMode::automatic);
        ESP_LOGI(tag,"HA jaw command result=%s",result);
    }else if(pending&&command.logs){
        if(s.connected.load()&&command.epoch==s.epoch.load()){
            if(now-command.received<=2000000)start_log(s,command,idle,now);
            else log_error(s,command.log_request,"expired");
        }
    }else if(pending){
        const char *result="expired";
        if(s.connected.load()&&command.epoch==s.epoch.load()&&now-command.received<=2000000){
            if(command.play){
                const char *path=ha::skit_path(*s.assets,s.selected);
                result=path&&show::play_skit(path)?"accepted":"not_ready";
            }else{
                const char *path=ha::skit_path(*s.assets,command.option);
                if(path){std::snprintf(s.selected,sizeof(s.selected),"%s",command.option);result="selected";}
                else result="invalid_skit";
            }
        }
        std::snprintf(s.result,sizeof(s.result),"%s",result);s.next_state=0;
        ESP_LOGI(tag,"HA command %s result=%s",command.play?"play":"select",result);
    }
    const uint32_t discovery_generation=s.discovery_generation.load();
    if(discovery_generation!=s.queued_discovery_generation){
        s.queued_discovery_generation=discovery_generation;s.discovery_queued=false;s.next_discovery=0;
    }
    if(discovery_generation!=s.published_discovery_generation&&s.subscriptions.load()==15&&now>=s.next_discovery){
        s.next_discovery=now+5000000;
        if(!s.discovery_queued&&s.model.discovery(*s.assets)&&enqueue(s,s.discovery_topic,s.model.json(),1,true)>=0)
            s.discovery_queued=true;
        // A maximum-size discovery can temporarily exceed the conservative
        // outbox threshold. Retry only online after it drains, not the large
        // configuration; reconnect/birth generations restart this owner stage.
        if(s.discovery_queued&&enqueue(s,s.availability_topic,"online",1,true)>=0){
            s.published_discovery_generation=discovery_generation;s.next_state=0;
        }
    }
    if(now<s.next_state)return;
    jaw=actuators::jaw_state();
    s.next_state=now+(jaw.mode==actuators::JawMode::manual?250000:5000000);
    ha::Snapshot snapshot;
    snapshot.show_state=show::state_name();const char *path=show::current_path();
    snapshot.current_skit=std::strncmp(path,"/audio/",7)?path:path+7;
    snapshot.selected_skit=s.selected;snapshot.command_result=s.result;
    snapshot.jaw_mode=jaw.mode==actuators::JawMode::automatic?"automatic":jaw.mode==actuators::JawMode::manual?"manual":"disabled";
    snapshot.jaw_result=s.jaw_result;snapshot.jaw_command_us=jaw.requested_us;snapshot.jaw_applied_us=jaw.applied_us;
    snapshot.jaw_closed_us=jaw.closed_us;snapshot.jaw_open_us=jaw.open_us;
    snapshot.jaw_draft_closed_us=s.draft_closed;snapshot.jaw_draft_open_us=s.draft_open;
    snapshot.jaw_dirty=s.draft_closed!=jaw.closed_us||s.draft_open!=jaw.open_us;
    snapshot.speaker=radio::speaker_connected();snapshot.stream=radio::stream_ready();
    snapshot.peer=s.primary?radio::peer_ready():secondary::connected();snapshot.sd=storage::mounted();
    snapshot.error_pending=faults.pending;snapshot.report_id=faults.id;snapshot.report_count=s.report_count;snapshot.report_slots=s.report_slots;
    const auto metrics=faultlog::metrics();
    snapshot.control_ready_ms=metrics.control_ready_ms;snapshot.sd_mount_ms=metrics.sd_mount_ms;
    snapshot.sd_config_ms=metrics.sd_config_ms;snapshot.speaker_connected_ms=metrics.speaker_connected_ms;
    snapshot.bluetooth_rssi_delta=radio::speaker_connected()?metrics.bluetooth_rssi_delta:-999;
    snapshot.sd_attempts=metrics.sd_attempts;snapshot.sd_failures=metrics.sd_failures;
    snapshot.speaker_disconnects=metrics.speaker_disconnects;
    snapshot.uptime_s=uint64_t(now/1000000);
    snapshot.free_heap=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    snapshot.min_free_heap=heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    wifi_ap_record_t ap{};snapshot.wifi_rssi_valid=esp_wifi_sta_get_ap_info(&ap)==ESP_OK;snapshot.wifi_rssi_dbm=ap.rssi;
    if(s.model.state(snapshot))enqueue(s,s.state_topic,s.model.json(),0,false);
    enqueue(s,s.play_available_topic,(s.subscriptions.load()==15&&jaw.mode==actuators::JawMode::automatic&&show::command_ready())?"online":"offline",0,true);
    enqueue(s,s.jaw_available_topic,(s.subscriptions.load()==15&&jaw.mode==actuators::JawMode::manual)?"online":"offline",0,true);
    enqueue(s,s.logs_available_topic,(s.subscriptions.load()==15&&idle&&!show::busy()&&storage::mounted()&&!s.log_transfer.active())?"online":"offline",0,true);
}
bool wifi_started(){return state&&state->wifi;}
void status(){
    if(!state){ESP_LOGI(tag,"Network disabled/uninitialized");return;}
    ESP_LOGI(tag,"HA network wifi_started=%d broker_configured=%d mqtt_connected=%d dropped_commands=%u reports=%u; credentials redacted",
             state->wifi,!!*state->config.mqtt_uri,state->connected.load(),state->dropped.load(),state->report_count);
}
bool configure_mqtt(const char *arguments){
    if(!arguments||std::strlen(arguments)>=256)return false;
    char uri[192]{},user[64]{},password[128]{},extra[2]{};
    if(std::sscanf(arguments,"%191s %63s %127s %1s",uri,user,password,extra)!=3)return false;
    // Validate in a read-only memory stream; never create a temporary SD file.
    char text[224];const int length=std::snprintf(text,sizeof(text),"mqtt_uri=%s\n",uri);
    if(length<=0||size_t(length)>=sizeof(text))return false;
    FILE *check=fmemopen(text,size_t(length),"r");if(!check)return false;
    Config candidate;const bool valid=read_config(check,candidate);std::fclose(check);
    if(!valid)return false;
    Broker saved;std::snprintf(saved.uri,sizeof(saved.uri),"%s",uri);
    if(std::strcmp(user,"-"))std::snprintf(saved.user,sizeof(saved.user),"%s",user);
    if(std::strcmp(password,"-"))std::snprintf(saved.password,sizeof(saved.password),"%s",password);
    nvs_handle_t handle{};if(nvs_open("skulls_net",NVS_READWRITE,&handle)!=ESP_OK)return false;
    auto result=nvs_set_blob(handle,"broker",&saved,sizeof(saved));if(result==ESP_OK)result=nvs_commit(handle);nvs_close(handle);
    return result==ESP_OK;
}
}
