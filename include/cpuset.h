#ifndef CPUSET_H
#define CPUSET_H

/**
    Author: Robert Crocombe
    Classification: Unclassified
    Initial Release Date: Mon Sep 25 10:15:29 MST 2006

    Encapsulation of Linux kernel's cpuset mechanism.

    I am not going to allow modification of flags or the CPUs in the set
    after set creation to simplify my life.

    The 'pids' thing is only for that cpuset itself: it doesn't include the
    children (?).  It's empty for the root cpuset, though really the root
    cpuset includes *all* the processes on the system.  But is that
    informative, really?.
*/

#include <vector>
#include <string>
#include <iosfwd>

#include <sys/types.h>

class cpuset;

typedef unsigned int cpuid_t;
typedef std::vector<cpuid_t> cpu_vector_t;
typedef std::vector<pid_t> pid_vector_t;
typedef std::vector<cpuset *> cpuset_vector_t;

namespace cpuset_constants
{
    const std::string ROOT_NAME("root");
}

class cpuset
{

private:

    static unsigned int number_cpus_;

    std::string *name_;
    std::string *path_;
    cpu_vector_t *CPUs_;
    pid_vector_t *pids_;

    bool cpu_is_exclusive_;
    bool mem_is_exclusive_;
    bool migrate_memory_;
    bool notify_on_release_;

    cpuset *parent_;
    cpuset_vector_t children_;

private:

    static void how_many_cpus(void);

    bool bad_cpu(cpuid_t cpu_number) const;
    cpuid_t insert_cpu(const std::string &cpu_string);
    void parse_cpulist(const std::string &cpus_in, std::string *cpus_out);
    void add_cpulist_to_cpuset(const std::string &cpulist);
    void remove_root_cpuset(void);

private:    // not possible

    cpuset(const cpuset &c);
    cpuset & operator =(const cpuset &c);

public:

    cpuset(void);

    // CPUset's have a name, and the 'cpus' string is a number or range: '1'
    // or '2-3' or '1,2,3,4'
    cpuset(const std::string &name,
           const std::string &cpus,
           cpuset *parent_cpuset,
           bool cpu_is_exclusive = true,
           bool mem_is_exclusive = true,
           bool migrate_memory = true,
           bool notify_on_release = false);
    ~cpuset(void);

    bool cpu_is_exclusive(void) const { return cpu_is_exclusive_; }
    bool mem_is_exclusive(void) const { return mem_is_exclusive_; }
    bool migrate_memory(void) const { return migrate_memory_; }
    bool notify_on_release(void) const { return notify_on_release_; }

    const std::string &path(void) const { return *path_; }
    const std::string &name(void) const { return *name_; }

    const cpu_vector_t &CPUs(void) const { return *CPUs_; }
    const pid_vector_t &pids(void) const { return *pids_; }

    void add_task(pid_t process);
    std::string print(void) const;

//  static void set_cpu_count(unsigned int count) { number_cpus_ = count; }
};

std::ostream &operator <<(std::ostream &o, const cpuset &s);

#endif  // CPUSET_H

