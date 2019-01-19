#ifndef SCHEDULER_UTILS_H
#define SCHEDULER_UTILS_H

/**
    Author: Robert Crocombe
    Classification: Unclassified
    Initial Release Date: Sometime in 2001 or 2002, I think.

    Originally released for IRIX.  This is the Linux version, which removes
    some sysmp() stuff for which Linux provides an alternate interface,
    usually something within cpusets.
*/

#include <unistd.h>
#include <stdint.h>
#include <sched.h>

void set_realtime_priority(int priority,
                           pid_t pid = getpid(),
                           unsigned sched_to_use = SCHED_RR);

void set_RSS_limit(uint64_t megabytes, bool arg_in_bytes = false);
void set_core_limit(uint64_t megabytes);

#endif  // SCHEDULER_UTILS_H

