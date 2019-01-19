#include "cpuset_manager.h"
#include "cpuset.h"
#include "program_IO.h"
#include "utility.h"

#include <sstream>
#include <ostream>

namespace cpuset_manager_name
{
    const std::string NAME("cpuset manager");
}

namespace
{
    enum
    {
        MAX_CPU_STRING = 100
    };
}

#define CSM_NAME cpuset_manager_name::NAME
#define CSM_CPRINT(fmt, args...)  CPRINT_WITH_NAME(CSM_NAME, fmt, ##args)
#define CSM_VPRINT(fmt, args...)  VPRINT_WITH_NAME(CSM_NAME, fmt, ##args)
#define CSM_WARNING(fmt, args...) WARNING_WITH_NAME(CSM_NAME, fmt, ##args)
#define CSM_ERROR(fmt, args...) ERROR_WITH_NAME(CSM_NAME, fmt, ##args)
#define CSM_RUNTIME(fmt, args...) RUNTIME_WITH_NAME(CSM_NAME, fmt, ##args)
#define CSM_REPORT(fmt, args...) REPORT_WITH_NAME(CSM_NAME, fmt, ##args);
#define CSM_DP(level, fmt, args...) DP(level, CSM_NAME, fmt, ##args)

////////////////////////////////////////////////////////////////////////////////
// Constructor and destructor
////////////////////////////////////////////////////////////////////////////////

/**
    The important thing is to get the root cpuset up and going.
*/

cpuset_manager::cpuset_manager(void):
    root_(new cpuset()),
    set_map_()
{
    set_map_[root_->name()] = root_;
}

/**
    Root recurses to delete all children.
*/

cpuset_manager::~cpuset_manager(void)
{
    delete root_;
}

////////////////////////////////////////////////////////////////////////////////
// Interface
////////////////////////////////////////////////////////////////////////////////

/**

*/

void
cpuset_manager::new_set
(
    const std::string &name,
    const std::string &cpus,
    const std::string &parent_cpuset_name,
    bool cpu_is_exclusive,
    bool mem_is_exclusive,
    bool migrate_memory,
    bool notify_on_release
)
{
    if (set_map_.count(name) != 0)
        CSM_RUNTIME("Already managing a cpuset with name '%s': "
                    "must have unique names", C(name));

    cpuset_map_t::iterator parent = set_map_.find(parent_cpuset_name);
    if (parent == set_map_.end())
        CSM_RUNTIME("For cpuset '%s', cannot find parent set with name '%s'",
                    C(name), C(parent_cpuset_name));

    cpuset *set = new cpuset(name, cpus, parent->second, cpu_is_exclusive,
                           mem_is_exclusive, migrate_memory, notify_on_release);

    set_map_[name] = set;
}

/**
    Add some actual processes to your shiny cpuset.
*/

void
cpuset_manager::add_task_to_set(const std::string &name, pid_t process)
{
    cpuset_map_t::iterator s = set_map_.find(name);
    if (s == set_map_.end())
        CSM_RUNTIME("Cannot get cpuset '%s': not found", C(name));

    s->second->add_task(process);
}

/**
    Note that when we remove a set, all the children go too.

    I am not going to allow deleting the root set: I think that just breaks
    too much stuff: there's certainly no way to go about producing another
    one: mounting a cpuset filesystem, etc.
*/

void
cpuset_manager::remove_set(const std::string &cpuset_name)
{
    if (cpuset_name == root_->name())
        CSM_RUNTIME("Cannot delete root cpuset.  Sorry.");

    cpuset_map_t::iterator s = set_map_.find(cpuset_name);
    if (s == set_map_.end())
        CSM_RUNTIME("Cannot remove cpuset '%s': not found",C(cpuset_name));

    delete s->second;
    set_map_.erase(s);
}

/**
    Return a /const/ reference to a cpuset within the larger map.  Can use
    to query cpuset properties.
*/

const cpuset &
cpuset_manager::get_set(const std::string &cpuset_name) const
{
    cpuset_map_t::const_iterator s = set_map_.find(cpuset_name);
    if (s == set_map_.end())
        CSM_RUNTIME("Cannot get cpuset '%s': not found",C(cpuset_name));

    return *(s->second);
}

/**
    More emphatic: non-const reference.

    I think this might be useful once you've verified that a cpuset is setup
    correctly.  Certainly adding processes will be less work direct through
    a reference rather than having to look up the task via name over and
    over.  However, I imagine adding tasks is relatively rare, so I gave the
    routine this name to emphasize that its breaking abstraction.
*/

cpuset &
cpuset_manager::gimme_the_damn_set(const std::string &cpuset_name)
{
    cpuset_map_t::iterator s = set_map_.find(cpuset_name);
    if (s == set_map_.end())
        CSM_RUNTIME("Cannot get cpuset '%s': not found", C(cpuset_name));

    return *(s->second);
}

/**
    Standard 'what ya got' routine.
*/

std::string
cpuset_manager::print(void) const
{
    std::ostringstream o;
    o << "CPUset for " << cpu_count_ << " CPU system\n"
      << "# of children in total is " << set_map_.size() << "\n"
      << "And a list starting at the root:\n" << *root_;
      
    return o.str(); 
}

////////////////////////////////////////////////////////////////////////////////
// Not in Class
////////////////////////////////////////////////////////////////////////////////

std::ostream &
operator <<(std::ostream &o, const cpuset_manager &s)
{
    o << s.print();
    return o;
}

#undef CSM_NAME
#undef CSM_CPRINT
#undef CSM_VPRINT
#undef CSM_WARNING
#undef CSM_ERROR
#undef CSM_RUNTIME
#undef CSM_REPORT
#undef CSM_DP

