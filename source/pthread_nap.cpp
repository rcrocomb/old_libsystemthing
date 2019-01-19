#include "pthread_nap.h"
#include "program_IO.h"
#include "utility.h"

namespace pthread_nap_name
{
    const std::string NAME("pthread_nap");
}

#define PN_NAME pthread_nap_name::NAME
#define PN_CPRINT(fmt, args...)  CPRINT_WITH_NAME(PN_NAME, fmt, ## args)
#define PN_VPRINT(fmt, args...)  VPRINT_WITH_NAME(PN_NAME, fmt, ## args)
#define PN_WARNING(fmt, args...) WARNING_WITH_NAME(PN_NAME, fmt, ## args)
#define PN_ERROR(fmt, args...) ERROR_WITH_NAME(PN_NAME, fmt, ## args)
#define PN_RUNTIME(fmt, args...) RUNTIME_WITH_NAME(PN_NAME, fmt, ## args)
#define PN_REPORT(fmt, args...) REPORT_WITH_NAME(PN_NAME, fmt, ## args)
#define PN_DP(level, fmt, args...) DP(level, PN_NAME, fmt, ## args)

#define PN_LOCK(mutex) LOCK(mutex,PN_ERROR)
#define PN_UNLOCK(mutex) UNLOCK(mutex,PN_ERROR)


pthread_nap::pthread_nap(const std::string &name):
    name_(name),
    cond_(new pthread_cond_t()),
    cond_mutex_(new pthread_mutex_t()),
    wake_up_(0)
{
    int ret;

    ret = pthread_mutex_init(cond_mutex_, 0);
    if (ret)
        PN_ERROR("Error creating cond var mutex '%s'", C(name_));

    ret = pthread_cond_init(cond_, 0);
    if (ret)
        PN_ERROR("Error creating cond variable '%s'", C(name_));
}

pthread_nap::~pthread_nap(void)
{
    int ret;

    ret = pthread_mutex_destroy(cond_mutex_);
    if (ret)
        PN_REPORT("Cannot destroy cond var mutex '%s'", C(name_));

    ret = pthread_cond_destroy(cond_);
    if (ret)
        PN_REPORT("Cannot destroy cond var '%s'", C(name_));

    delete cond_;
    delete cond_mutex_;
}

void
pthread_nap::block(void)
{
//  PN_CPRINT("going to sleep\n");

    PN_LOCK(cond_mutex_);
    while (!wake_up_)
    {
        int ret = pthread_cond_wait(cond_, cond_mutex_);
        if (ret)
            PN_ERROR("pthread_cond_wait for '%s'", C(name_));
    }
    PN_UNLOCK(cond_mutex_);

//  PN_CPRINT("'%s': woke up\n", C(name_));
    wake_up_ = 0;
}

void
pthread_nap::wake_up(void)
{
    int ret;

    PN_LOCK(cond_mutex_);
    wake_up_ = 1;
    ret = pthread_cond_signal(cond_);
    if (ret)
    {
        PN_UNLOCK(cond_mutex_);
        PN_ERROR("pthread_cond_signal");
    }
    PN_UNLOCK(cond_mutex_);

//  PN_CPRINT("'%s': signaled for wakeup\n", C(name_));
}

#undef PN_NAME
#undef PN_CPRINT
#undef PN_VPRINT
#undef PN_WARNING
#undef PN_ERROR
#undef PN_RUNTIME
#undef PN_REPORT
#undef PN_DP
#undef PN_LOG
#undef PN_LOCK 
#undef PN_UNLOCK

