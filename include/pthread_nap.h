#ifndef PTHREAD_NAP_H
#define PTHREAD_NAP_H

#include <pthread.h>
#include <string>

namespace pthread_nap_constants
{
    const std::string DEFAULT_NAME("pthread nap default name");
}

namespace PNC = pthread_nap_constants;

class pthread_nap
{

private:

    std::string name_;
    pthread_cond_t *cond_;
    pthread_mutex_t *cond_mutex_;
    unsigned int wake_up_;

public:

    pthread_nap(const std::string &name = PNC::DEFAULT_NAME);
    ~pthread_nap(void);
    
    void block(void);
    void wake_up(void);
};

#endif  // PTHREAD_NAP_H

