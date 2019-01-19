#ifndef CPUSET_MANAGER_H
#define CPUSET_MANAGER_H

/**
    Author: Robert Crocombe
    Initial Release Date: Mon Dec 18 15:30:28 MST 2006
    Classification: Unclassified

    Manages a set of cpuset class instances.  After some experimentation, it
    seems reasonable to manage the root cpuset as its own entity, because it
    has a bunch unalterable fields and its list of tasks will have very task
    in the system, etc.

    It isn't strictly necessary to maintain this list: it would be possible
    to add a search routine to cpuset to check if a particular cpuset is a
    child, and then maintaining a pointer to the root cpuset would be
    sufficient for accessing all children.  However, it could be
    time-consuming to dig through a list (though with a simple topology I
    guess it wouldn't be a bunch of work).  Anyway, I register all the
    cpusets that are created at the top level for ease of access.

    However, cpusets will be deleted through the root_ pointer, since the
    cpuset destructor does recurse its tree of children to free them, and
    therefore it will not be necessary to go through set_map_ and delete
    things that way.

    The limitation of having this top level mapping is that names must be
    globally unique, and not simply unique within a single nesting.  For
    instance, with this class the following is *not* legal, but would be if
    I maintained a tree.

    root
     |
     +---> monkey
     |      |----> kitty
     |
     +---> cow
            |----> kitty

    You cannot have two "kitty"s in my scheme (the map will clash).  I don't
    consider this a serious limitation, or I would've used a multimap.
*/

#include <string>
#include <map>
#include <vector>
#include <iosfwd>


class cpuset;

// mapped by cpuset's name
typedef std::map<std::string, cpuset *> cpuset_map_t;

class cpuset_manager
{

private:

    unsigned cpu_count_;

    cpuset *root_;          //* the base cpuset
    cpuset_map_t set_map_;  //* keeps track of all child cpusets

private:    // unimplemented

    cpuset_manager(const cpuset_manager &m);
    cpuset_manager &operator =(const cpuset_manager &m);

private:    // internal

public:

    cpuset_manager(void);
    ~cpuset_manager(void);

    void new_set(const std::string &name,
                 const std::string &cpus,
                 const std::string &parent_cpuset_name,
                 bool cpu_is_exclusive = true,
                 bool mem_is_exclusive = true,
                 bool migrate_memory = true,
                 bool notify_on_release = false);

    void add_task_to_set(const std::string &name, pid_t process);

    void remove_set(const std::string &cpuset_name);

    const cpuset &get_set(const std::string &cpuset_name) const;
    cpuset &gimme_the_damn_set(const std::string &cpuset_name);

    unsigned int how_many_cpus(void) const { return cpu_count_; }

    std::string print(void) const;
};

std::ostream &operator <<(std::ostream &o, const cpuset_manager &s);

#endif  // CPUSET_MANAGER_H

