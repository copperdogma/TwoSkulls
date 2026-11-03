#pragma once
#include "skull_jaw_command.h"

namespace skull::jawcontrol {
struct View {
    bool manual = false, enabled = false;
    unsigned requested_us = 0, applied_us = 0, closed_us = 544, open_us = 1368;
};
struct Draft { unsigned closed_us = 544, open_us = 1368; };
struct Context { bool idle = false, startup_finished = false, transfer_active = false; };
inline void reset(Draft &draft, const View &state) {
    draft.closed_us=state.closed_us;draft.open_us=state.open_us;
}
// The actual network/control owner uses this policy, with a small actuator/SD
// adapter. Fake backends exercise the same enter/capture/save decision paths.
// No callback invokes it: callers must serialize backend motion and SD access.
template <typename Backend>
const char *apply(const jawcommand::Request &request,const Context &context,Draft &draft,Backend &backend) {
    using jawcommand::Action;
    const auto jaw=backend.state();
    if(request.action==Action::stop){
        backend.disable();
        return backend.state().enabled ? "disable_pending" : "pulses_disabled";
    }
    if(request.action==Action::enter){
        if(!context.idle||!context.startup_finished||context.transfer_active)return "not_idle";
        if(jaw.manual)return "already_manual";
        if(!backend.enter())return "entry_rejected";
        reset(draft,backend.state());return "manual_entered";
    }
    if(request.action==Action::resume){
        if(!context.idle||!context.startup_finished)return "not_idle";
        if(!backend.resume())return "resume_rejected";
        reset(draft,backend.state());return "automatic_resumed";
    }
    if(!jaw.manual)return "not_manual";
    if(request.action==Action::target)
        return backend.target(unsigned(request.value))?"target_accepted":"target_rejected_bounds_or_step";
    if(request.action==Action::step){
        int delta=request.value;
        if(request.relative_open&&jaw.open_us<jaw.closed_us)delta=-delta;
        return backend.step(delta)?"step_accepted":"step_rejected_bounds_or_pending";
    }
    // Do not capture an unapplied target or save after pulse disable/lease
    // expiry. A no-motion target renews/checks the lease through the actuator.
    if(!jaw.enabled||!jaw.applied_us||jaw.requested_us!=jaw.applied_us)return "position_pending_or_disabled";
    if(!backend.target(jaw.requested_us))return "manual_lease_rejected";
    if(request.action==Action::closed){draft.closed_us=jaw.applied_us;return "closed_captured";}
    if(request.action==Action::open){draft.open_us=jaw.applied_us;return "open_captured";}
    if(request.action==Action::discard){reset(draft,backend.state());return "draft_discarded";}
    if(request.action==Action::save){
        if(!context.idle||context.transfer_active)return "not_idle";
        if(draft.closed_us==draft.open_us)return "invalid_equal_endpoints";
        if(!backend.save(draft.closed_us,draft.open_us))return "sd_save_failed";
        if(!backend.configure(draft.closed_us,draft.open_us))return "saved_activation_failed";
        return "saved_to_sd";
    }
    return "invalid_command";
}
}
