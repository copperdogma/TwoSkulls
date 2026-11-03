#pragma once
#include <cstddef>
#include <cstring>

namespace skull::jawcommand {
enum class Action { enter, stop, resume, step, target, closed, open, save, discard };
struct Request { Action action = Action::stop; int value = 0; bool relative_open = false; };
// Strict, allocation-free MQTT grammar. Never accepts embedded NULs, whitespace,
// signs on absolute targets, exponents, partial numbers or unbounded integers.
inline bool parse(const char *data, size_t size, Request &request) {
    if (!data || !size || size > 16 || std::memchr(data, 0, size)) return false;
    const struct { const char *text; Action action; int value; bool relative; } commands[] = {
        {"ENTER",Action::enter,0,false}, {"STOP",Action::stop,0,false},
        {"RESUME",Action::resume,0,false}, {"CLOSED",Action::closed,0,false},
        {"OPEN",Action::open,0,false}, {"SAVE",Action::save,0,false},
        {"DISCARD",Action::discard,0,false},
        {"STEP:+1",Action::step,1,false}, {"STEP:-1",Action::step,-1,false},
        {"STEP:+10",Action::step,10,false}, {"STEP:-10",Action::step,-10,false},
        {"STEP:OPEN1",Action::step,1,true}, {"STEP:CLOSE1",Action::step,-1,true},
        {"STEP:OPEN10",Action::step,10,true}, {"STEP:CLOSE10",Action::step,-10,true}
    };
    for (const auto &command : commands) {
        if (size == std::strlen(command.text) && !std::memcmp(data,command.text,size)) {
            request = {command.action,command.value,command.relative}; return true;
        }
    }
    if (size < 8 || std::memcmp(data,"TARGET:",7)) return false;
    unsigned value = 0;
    for (size_t i=7;i<size;++i) {
        if (data[i]<'0'||data[i]>'9') return false;
        value=value*10+unsigned(data[i]-'0');
        if(value>2400) return false;
    }
    if(value<544) return false;
    request={Action::target,int(value),false}; return true;
}
}
