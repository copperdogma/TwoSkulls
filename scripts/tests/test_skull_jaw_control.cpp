#include "skull_jaw_control.h"
#include <cassert>
#include <cstring>
#include <cstdio>
using namespace skull;
namespace {
struct Fake {
    jawcontrol::View view{false,true,600,600,544,1368};
    bool enter_ok=true,resume_ok=true,target_ok=true,save_ok=true,configure_ok=true,disable_ok=true;
    unsigned enters=0,disables=0,resumes=0,targets=0,saves=0,configures=0;
    unsigned saved_closed=0,saved_open=0,last_target=0;
    int last_step=0;
    jawcontrol::View state(){return view;}
    bool enter(){++enters;if(enter_ok)view.manual=true;return enter_ok;}
    void disable(){++disables;view.manual=false;if(disable_ok){view.enabled=false;view.applied_us=0;}}
    bool resume(){++resumes;if(resume_ok)view.manual=false;return resume_ok;}
    bool target(unsigned pulse){++targets;last_target=pulse;return target_ok;}
    bool step(int delta){last_step=delta;return target_ok;}
    bool save(unsigned closed,unsigned open){++saves;saved_closed=closed;saved_open=open;return save_ok;}
    bool configure(unsigned closed,unsigned open){++configures;if(configure_ok){view.closed_us=closed;view.open_us=open;}return configure_ok;}
};
const char *apply(const char *text,const jawcontrol::Context &context,jawcontrol::Draft &draft,Fake &fake){
    jawcommand::Request request;assert(jawcommand::parse(text,std::strlen(text),request));
    return jawcontrol::apply(request,context,draft,fake);
}
void expect(const char *actual,const char *expected){assert(!std::strcmp(actual,expected));}
void enter_stop_resume(){
    Fake fake;jawcontrol::Draft draft{610,1400};jawcontrol::Context context{false,true,false};
    expect(apply("ENTER",context,draft,fake),"not_idle");assert(!fake.enters);
    context={true,false,false};expect(apply("ENTER",context,draft,fake),"not_idle");assert(!fake.enters);
    context={true,true,true};expect(apply("ENTER",context,draft,fake),"not_idle");assert(!fake.enters);
    context.transfer_active=false;fake.enter_ok=false;
    expect(apply("ENTER",context,draft,fake),"entry_rejected");assert(draft.closed_us==610);
    fake.enter_ok=true;expect(apply("ENTER",context,draft,fake),"manual_entered");
    assert(draft.closed_us==544&&draft.open_us==1368&&!fake.targets&&!fake.saves);
    expect(apply("ENTER",context,draft,fake),"already_manual");assert(fake.enters==2);
    context.idle=false;expect(apply("STOP",context,draft,fake),"pulses_disabled");assert(fake.disables==1);
    expect(apply("TARGET:600",context,draft,fake),"not_manual");assert(!fake.targets);
    expect(apply("RESUME",context,draft,fake),"not_idle");assert(!fake.resumes);
    context.idle=true;fake.resume_ok=false;expect(apply("RESUME",context,draft,fake),"resume_rejected");
    fake.resume_ok=true;expect(apply("RESUME",context,draft,fake),"automatic_resumed");assert(!fake.view.manual);
    fake.view.enabled=true;fake.view.applied_us=600;fake.disable_ok=false;
    expect(apply("STOP",context,draft,fake),"disable_pending");
    assert(fake.view.enabled&&fake.view.applied_us==600); // Never claim zero before the driver accepts it.
}
void target_and_direction(){
    Fake fake;fake.view.manual=true;jawcontrol::Draft draft;jawcontrol::Context context{true,true,false};
    expect(apply("TARGET:610",context,draft,fake),"target_accepted");assert(fake.last_target==610);
    fake.target_ok=false;expect(apply("TARGET:2400",context,draft,fake),"target_rejected_bounds_or_step");
    expect(apply("STEP:OPEN10",context,draft,fake),"step_rejected_bounds_or_pending");assert(fake.last_step==10);
    fake.target_ok=true;expect(apply("STEP:CLOSE1",context,draft,fake),"step_accepted");assert(fake.last_step==-1);
    fake.view.closed_us=1368;fake.view.open_us=544;
    expect(apply("STEP:OPEN10",context,draft,fake),"step_accepted");assert(fake.last_step==-10);
    expect(apply("STEP:CLOSE1",context,draft,fake),"step_accepted");assert(fake.last_step==1);
    expect(apply("STEP:+10",context,draft,fake),"step_accepted");assert(fake.last_step==10);
}
void capture_save_and_failure(){
    Fake fake;fake.view.manual=true;jawcontrol::Draft draft;jawcontrol::Context context{true,true,false};
    fake.view.requested_us=610;
    expect(apply("CLOSED",context,draft,fake),"position_pending_or_disabled");assert(!fake.targets&&draft.closed_us==544);
    fake.view.requested_us=600;fake.view.enabled=false;
    expect(apply("SAVE",context,draft,fake),"position_pending_or_disabled");assert(!fake.saves);
    fake.view.enabled=true;fake.target_ok=false;
    expect(apply("CLOSED",context,draft,fake),"manual_lease_rejected");assert(draft.closed_us==544&&!fake.saves);
    expect(apply("SAVE",context,draft,fake),"manual_lease_rejected");assert(!fake.saves&&!fake.configures);
    fake.target_ok=true;expect(apply("CLOSED",context,draft,fake),"closed_captured");
    assert(draft.closed_us==600&&fake.last_target==600&&!fake.saves);
    expect(apply("OPEN",context,draft,fake),"open_captured");
    expect(apply("SAVE",context,draft,fake),"invalid_equal_endpoints");assert(!fake.saves&&!fake.configures);
    draft.open_us=1400;context.idle=false;
    expect(apply("SAVE",context,draft,fake),"not_idle");assert(!fake.saves);
    context.idle=true;context.transfer_active=true;
    expect(apply("SAVE",context,draft,fake),"not_idle");assert(!fake.saves);
    context.transfer_active=false;fake.save_ok=false;
    expect(apply("SAVE",context,draft,fake),"sd_save_failed");assert(fake.saves==1&&!fake.configures);
    assert(draft.closed_us==600&&draft.open_us==1400&&fake.view.closed_us==544);
    fake.save_ok=true;expect(apply("SAVE",context,draft,fake),"saved_to_sd");
    assert(fake.saved_closed==600&&fake.saved_open==1400&&fake.configures==1);
    assert(fake.view.closed_us==600&&fake.view.open_us==1400&&fake.view.applied_us==600);
    draft.closed_us=610;expect(apply("DISCARD",context,draft,fake),"draft_discarded");assert(draft.closed_us==600);
    fake.view.manual=false;expect(apply("SAVE",context,draft,fake),"not_manual");assert(fake.saves==2);
}
}
int main(){enter_stop_resume();target_and_direction();capture_save_and_failure();std::puts("Jaw control entry, lease gate, direction, draft and verified-save policy passed");}
