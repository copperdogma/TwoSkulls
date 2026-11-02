#pragma once
#include "skull_fault_log_core.h"
namespace skull::faultlog {
// Fixed eight slots plus one bounded temporary file; never touches other paths.
// Uses checked flush/fsync/close and readback before replacing an older slot.
bool archive_report(const char *directory,unsigned slot,const Report &report);
// Read a fixed saved slot only, in <=64-byte chunks. Reject files exceeding the
// report cap before emitting content. Owner must exclude concurrent archive I/O.
// A false consumer result aborts reading. Never writes/deletes card contents.
bool read_archive(const char *directory,unsigned slot,
                  bool (*consume)(const uint8_t *,size_t,void *),void *context);
}
