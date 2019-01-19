#include "timing.h"

#include <stdint.h>                                 // uint64_t
#include <stdlib.h>                                 // drand48()
#include <sys/select.h>                             // select()
#include <sys/time.h>                               // gettimeofday
#include <time.h>
#include <asm-i486/msr.h>                           // rdtscll()

#include "program_IO.h"

#include <string>

namespace timing_name
{
    const std::string NAME("timing");
}

#define TIME_NAME timing_name::NAME
#define TIME_CPRINT(fmt, args...)  CPRINT_WITH_NAME(TIME_NAME, fmt, ## args)
#define TIME_VPRINT(fmt, args...)  VPRINT_WITH_NAME(TIME_NAME, fmt, ## args)
#define TIME_WARNING(fmt, args...) WARNING_WITH_NAME(TIME_NAME, fmt, ## args)
#define TIME_ERROR(fmt, args...) ERROR_WITH_NAME(TIME_NAME, fmt, ## args)
#define TIME_RUNTIME(fmt, args...) RUNTIME_WITH_NAME(TIME_NAME, fmt, ## args)
#define TIME_REPORT(fmt, args...) REPORT_WITH_NAME(TIME_NAME, fmt, ## args)
#define TIME_DP(level, fmt, args...) DP(level, TIME_NAME, fmt, ## args)

////////////////////////////////////////////////////////////////////////////////
// Globals
////////////////////////////////////////////////////////////////////////////////

static const double SECS_PER_NANO = 1E-6;
static const double GIGS_PER_HZ   = 1E-9;
static double seconds_per_tick;

////////////////////////////////////////////////////////////////////////////////
// Prototypes
////////////////////////////////////////////////////////////////////////////////

double gimme_timeofday(void);
void selectsleep(unsigned us);

////////////////////////////////////////////////////////////////////////////////
// Definitions
////////////////////////////////////////////////////////////////////////////////

double
gimme_timeofday(void)
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv.tv_sec + SECS_PER_NANO * tv.tv_usec;
}


double
get_time(void)
{
    uint64_t t;
    rdtscll(t);
    return t * seconds_per_tick;
}


/**
    A good way to simply hang around doing nothing for awhile.
*/

void
selectsleep(unsigned us)
{
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = us;
	select(0, 0, 0, 0, &tv);
}

/**
    Figure out how fast rdtscll() ticks.  This should be equal to the
    frequency of the clock on the processor.  Here's the bad news: I don't
    know if rdtscll() always uses the same processor so it may very well be
    necessary to set a processor affinity to get really good results over
    time.

    This piece of code by Mark Hahn.
*/

void
init_timer(void)
{
	double sumx = 0;
	double sumy = 0;
	double sumxx = 0;
	double sumxy = 0;
	double slope;

	// least squares linear regression of ticks onto real time
	// as returned by gettimeofday.

	const unsigned n = 30;

        TIME_CPRINT("Calibrating cycle counter vs. clock\n");

	for (unsigned int i = 0; i < n; ++i)
        {
		double breal,real,ticks;
		uint64_t aticks, bticks;
	
		breal = gimme_timeofday();
		rdtscll(bticks);

		selectsleep((unsigned)(10000 + drand48() * 200000));

                rdtscll(aticks);
		ticks = aticks - bticks;
		real = gimme_timeofday() - breal;

		sumx += real;
		sumxx += real * real;
		sumxy += real * ticks;
		sumy += ticks;
	}

	slope = ((sumxy - (sumx * sumy) / n) / (sumxx - (sumx * sumx) / n));
	seconds_per_tick = 1.0 / slope;

        TIME_CPRINT("Calibrated timer as %.6f GHz\n\n", slope * GIGS_PER_HZ);
}

void
busy_delay(double seconds)
{
    if (seconds < 0)
        return;

    if (seconds == 0)
        return;

    double start = get_time();
    for ( ; ; )
    {
        double now = get_time();
        double diff = now - start;
        if (diff > seconds)
            break;
    }
}

#undef TIME_NAME
#undef TIME_CPRINT
#undef TIME_VPRINT
#undef TIME_WARNING
#undef TIME_ERROR
#undef TIME_RUNTIME
#undef TIME_REPORT
#undef TIME_DP
#undef TIME_LOG
