#ifndef UTILITY_H
#define UTILITY_H

#include <sys/types.h>          // pid_t
#include <unistd.h>             // getpid

#define LOCK(mutex,ERROR_MACRO) do { \
                                    int ret = pthread_mutex_lock(mutex); \
                                    if (ret) \
                                        ERROR_MACRO("Unable to lock mutex."); \
                                } while (0)

#define UNLOCK(mutex,ERROR_MACRO) do { \
                                    int ret = pthread_mutex_unlock(mutex); \
                                    if (ret) \
                                        ERROR_MACRO("Unable to unlock mutex.");\
                                  } while (0)

namespace utility
{

    unsigned int how_many_cpus(void);
    void run_on_cpu(unsigned cpu, pid_t pid = getpid());
}

#endif  /* UTILITY_H */

