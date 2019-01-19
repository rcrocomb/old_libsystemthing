#include "utility.h"
#include "program_IO.h"
#include <unistd.h>                 // sysconf

#include <string>

namespace cpuset_name
{
    const std::string NAME("cpuset");
}

#define UTIL_NAME cpuset_name::NAME
#define UTIL_CPRINT(fmt, ...)  CPRINT_WITH_NAME(UTIL_NAME, fmt, ##__VA_ARGS__)
#define UTIL_VPRINT(fmt, ...)  VPRINT_WITH_NAME(UTIL_NAME, fmt, ##__VA_ARGS__)
#define UTIL_WARNING(fmt, ...) WARNING_WITH_NAME(UTIL_NAME, fmt, ##__VA_ARGS__)
#define UTIL_ERROR(fmt, ...) ERROR_WITH_NAME(UTIL_NAME, fmt, ##__VA_ARGS__)
#define UTIL_RUNTIME(fmt, ...) RUNTIME_WITH_NAME(UTIL_NAME, fmt, ##__VA_ARGS__)
#define UTIL_REPORT(fmt, ...) REPORT_WITH_NAME(UTIL_NAME, fmt, ##__VA_ARGS__);
#define UTIL_DP(level, fmt, ...) DP(level, UTIL_NAME, fmt, ##__VA_ARGS__)

namespace utility
{
/**
    How many CPUs are available to us.  

    _SC_NPROCESSORS_ONLN gives us a count of "online" processors (if they
    are hotpluggable).

    The other option is: _SC_NPROCESSORS_CONF, which is how many processors
    the machine could possibly have.

    Note that if you are running within a cpuset, that this value may return
    a value that includes CPUs that you cannot access.

    There's a discussion of the various CPU counts and how to get them and
    suchlike beginning at:

        http://marc.info/?t=117561936700003&r=2&w=2

    which begins with someone complaining about how long it takes to get a
    certain number, but which might not be the proper number because of CPU
    hotplugging and cpusets, etc.
*/

unsigned int
how_many_cpus(void)
{
    long count = sysconf(_SC_NPROCESSORS_ONLN);
    if (count == -1)
        UTIL_ERROR("getting CPU count via sysconf");

    return static_cast<unsigned>(count);
}

void
run_on_cpu(unsigned cpu, pid_t pid)
{
    static unsigned CPUs = utility::how_many_cpus();
    cpu_set_t CPU_mask;
    int retcode;

    if (cpu >= CPUs)
        UTIL_RUNTIME("Illegal CPU value %d\n", cpu);

    UTIL_CPRINT("Using CPU %d as CPU to run on\n", cpu);

    retcode = sched_getaffinity(pid, sizeof(CPU_mask), &CPU_mask);
    if (retcode)
        UTIL_ERROR("sched_getaffinity failed");

    if (!__CPU_ISSET(cpu, &CPU_mask))
        UTIL_RUNTIME("not allowed to use CPU %u", cpu);

    __CPU_ZERO(&CPU_mask);
    __CPU_SET(cpu, &CPU_mask);

    retcode = sched_setaffinity(pid, sizeof(CPU_mask), &CPU_mask);
    if (retcode)
        UTIL_ERROR("Failed to set processor affinity for CPU %u", cpu);

    UTIL_CPRINT("Okay: assigned to CPU %u\n", cpu);
}

}       // end utility namespace

////////////////////////////////////////////////////////////////////////////////

#undef UTIL_NAME
#undef UTIL_CPRINT
#undef UTIL_VPRINT
#undef UTIL_ERROR
#undef UTIL_WARNING
#undef UTIL_RUNTIME
#undef UTIL_REPORT
#undef UTIL_DP

