#include "scheduler_utils.h"

#include <string>

#include <sys/resource.h>   // setrlimit(), getrlimit()

#include "program_IO.h"

namespace scheduler_name
{
    const std::string NAME("scheduler utils"); // this name is magic
}

namespace
{
    enum
    {
        MEGA_TO_BYTES = 20
    };
}

#define SU_NAME scheduler_name::NAME
#define SU_CPRINT(fmt, args...) CPRINT_WITH_NAME(SU_NAME, fmt, ## args)
#define SU_VPRINT(fmt, args...) VPRINT_WITH_NAME(SU_NAME, fmt, ## args)
#define SU_WARNING(fmt, args...) WARNING_WITH_NAME(SU_NAME, fmt, ## args)
#define SU_ERROR(fmt, args...) ERROR_WITH_NAME(SU_NAME, fmt, ## args)
#define SU_RUNTIME(fmt, args...) RUNTIME_WITH_NAME(SU_NAME, fmt, ## args)
#define SU_REPORT(fmt, args...) REPORT_WITH_NAME(SU_NAME, fmt, ## args)

////////////////////////////////////////////////////////////////////////////////

/**
    The default policy is SCHED_RR (round robin), but FIFO may be supplied
    via 'sched_to_use'.  I'm not allowing NORMAL or BATCH here because I
    don't think you can supply priorities other than 0, so they'd break
    anyway.  The sched_setscheduler() manpage is illuminating.
*/

void
set_realtime_priority
(
    int desired_priority,
    pid_t pid,
    unsigned sched_to_use
)
{
    int priority = desired_priority;
    int ret;

    if ((sched_to_use != SCHED_FIFO) && (sched_to_use != SCHED_RR))
        SU_RUNTIME("Illegal scheduler type %u not FIFO or round robin",
                   sched_to_use);

    int max_priority = sched_get_priority_max(sched_to_use);
    int min_priority = sched_get_priority_min(sched_to_use);

    if ((max_priority == -1) || (min_priority == -1))
        SU_ERROR("Bad priorities returned by sched_get_priority_*.");

    // Do we have to adjust 'priority' from desired_priority based on limits?
    if ((max_priority < priority) && (min_priority < priority))
    {
        SU_CPRINT("Priority cap at %d, using instead of %d\n",
               max_priority, desired_priority);
        priority = max_priority;
    } else if ((min_priority > priority) && (max_priority > priority))
    {
        // pretty unlikely, but who knows?
        SU_CPRINT("Priority raised from %d to minimum priority of %d\n",
               desired_priority, min_priority);
        priority = min_priority;
    }

    struct sched_param priority_params;
    priority_params.sched_priority = priority;

    ret = sched_setscheduler (pid, sched_to_use, &priority_params);
    if (ret == -1)
        SU_ERROR("sched_setscheduler failed for pid %d prio %d",
                    pid, priority);
}

/**
    Change the Resident Set Size.  I think this is supposed to be
    per-process, but I am not positive that it works that way.  Probably
    it does, and I am just confused.

    Now uses exceptions for errors, rather than boolean values.
*/

void
set_RSS_limit(const uint64_t megabytes, const bool arg_in_bytes)
{
    uint64_t bytes = megabytes << (arg_in_bytes ? 0 : MEGA_TO_BYTES);
    struct rlimit64 mem_limit = { bytes, bytes };

    int status = setrlimit64(RLIMIT_RSS, &mem_limit);
    if (status != 0)
        SU_ERROR("setrlimit64 failed for RSS: "
                    "soft limit %llu bytes hard limit %llu bytes",
                    (unsigned long long)mem_limit.rlim_cur,
                    (unsigned long long)mem_limit.rlim_max);

    SU_CPRINT("Resident Set Size limited to %llu megabytes\n",
              (unsigned long long)bytes);
}

void
set_core_limit(uint64_t megabytes)
{
    struct rlimit core_limit = { megabytes << MEGA_TO_BYTES,
                                 megabytes << MEGA_TO_BYTES };
    int status = setrlimit(RLIMIT_CORE, &core_limit);
    if (status != 0)
        SU_ERROR("failed setting max core size: "
                 "soft limit %llu bytes hard limit %llu bytes",
                 (unsigned long long)core_limit.rlim_cur,
                 (unsigned long long)core_limit.rlim_max);        

    if (!megabytes)
        SU_CPRINT("Core file writing DISABLED\n");
    else
        SU_CPRINT("Core file limited to %llu megabytes \n",
                  (unsigned long long)megabytes);
}

#undef SU_NAME
#undef SU_CPRINT
#undef SU_VPRINT
#undef SU_WARNING
#undef SU_ERROR
#undef SU_RUNTIME
#undef SU_REPORT

