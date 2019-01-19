#include "cpuset.h"
#include "utility.h"

#include <sys/stat.h>                       // mkdir()
#include <sys/mount.h>                      // mount(), umount()
#include <unistd.h>                         // rmdir(), chdir()
#include <fcntl.h>                          // open()
#include <errno.h>

#include <sstream>
#include <ostream>

#include "program_IO.h"

namespace cpuset_name
{
    const std::string NAME("cpuset");
}

namespace
{
    enum
    {
        LINE_LENGTH             = 80,
        LINES_OF_TEXT_PER_CPU   = 20,   // /proc/cpuinfo: 20 if 1, 40 if 2, etc.
        MAX_CPUS                = 16,
        MAX_PATH_LENGTH         = 1024,
        TRIES                   = 3
    };

    const std::string DELIMITER("----------------------------------------------------------------------");

    // cpuset_thingie is an arbitrary label.  I use 'thingie' so people will
    // understand it's a person-derived label and not some weird kernel thing.
    const std::string ROOT_LABEL("cpuset_thingie");

    const std::string CPUSET_PATH("/dev/cpuset/");

    const std::string DIR_EXISTS_COMMAND("test -d ");

    const std::string ECHO("/bin/echo ");
    const std::string ECHO_ON(ECHO + "1 > ");
    const std::string ECHO_OFF(ECHO + "0 > ");
}

#define X(text) std::string(text)

// Path must have trailing "/"
#define CPU_EXCLUSIVE_ON(path)  std::string(ECHO_ON + (path) + "cpu_exclusive")
#define CPU_EXCLUSIVE_OFF(path) std::string(ECHO_OFF + (path) + "cpu_exclusive")

#define MEM_EXCLUSIVE_ON(path)  std::string(ECHO_ON + (path) + "mem_exclusive")
#define MEM_EXCLUSIVE_OFF(path) std::string(ECHO_OFF + (path) + "mem_exclusive")

#define MEM_MIGRATE_ON(path)    std::string(ECHO_ON + (path) + "memory_migrate")
#define MEM_MIGRATE_OFF(path)   std::string(ECHO_OFF + (path) + "memory_migrate")

#define RELEASE_NOTIFY_ON(path) std::string(ECHO_ON + (path) + "notify_on_release")
#define RELEASE_NOTIFY_OFF(path) std::string(ECHO_OFF + (path) + "notify_on_release")

#define CS_NAME cpuset_name::NAME
#define CS_CPRINT(fmt, args...)  CPRINT_WITH_NAME(CS_NAME, fmt, ##args)
#define CS_VPRINT(fmt, args...)  VPRINT_WITH_NAME(CS_NAME, fmt, ##args)
#define CS_WARNING(fmt, args...) WARNING_WITH_NAME(CS_NAME, fmt, ##args)
#define CS_ERROR(fmt, args...) ERROR_WITH_NAME(CS_NAME, fmt, ##args)
#define CS_RUNTIME(fmt, args...) RUNTIME_WITH_NAME(CS_NAME, fmt, ##args)
#define CS_REPORT(fmt, args...) REPORT_WITH_NAME(CS_NAME, fmt, ##args);
#define CS_DP(level, fmt, args...) DP(level, CS_NAME, fmt, ##args)

////////////////////////////////////////////////////////////////////////////////
// Static 
////////////////////////////////////////////////////////////////////////////////

unsigned int cpuset::number_cpus_ = 0;

////////////////////////////////////////////////////////////////////////////////
// Constructors and Destructor
////////////////////////////////////////////////////////////////////////////////

/**
    Only the root cpuset should do this mount.  The mere act of mkdir within
    this parent will do the proper thing for children.

    These values for exclusivity, etc. are the ones Linux uses when a root
    cpuset is mounted.  You can't alter the attributes of the root cpuset,
    so I'll just stick these values here so they're likely to be right
    (untrue if Linux defaults change: *highly* unlikely).

    The root cpuset gets an extra filename, 'memory_pressure_enabled', which
    is disabled by default.  I'm leaving it alone.  Note that there's no
    capability included to support it.

    Must new the name_ and path_ variables because of delete in destructor:
    otherwise deleting statically allocated stuff.
*/

cpuset::cpuset(void):
    name_(new std::string(cpuset_constants::ROOT_NAME)),
    path_(new std::string(CPUSET_PATH)),
    CPUs_(new cpu_vector_t()),
    pids_(new pid_vector_t()),
    cpu_is_exclusive_(true),
    mem_is_exclusive_(true),
    migrate_memory_(false),
    notify_on_release_(false),
    parent_(0),
    children_()
{
    int ret;
    unsigned tries = 0;

    if (number_cpus_)
        CS_RUNTIME("To be used only once to create root cpuset!");

    number_cpus_ = utility::how_many_cpus();

try_again:
    ret = mkdir(CP(path_), 0755);
    if (ret)
    {
        if (errno == EEXIST)
        {
            if (!tries)
                CS_CPRINT("Root set exists.  Failed to cleanup?  "
                          "Trying to purge\n");

            ++tries;
            if (tries < TRIES)
            {
                remove_root_cpuset();
                goto try_again;
            }

            // Screw it, we'll just use it
            CS_WARNING("Root cpuset exists and we couldn't remove it: "
                       "there may be existing child sets.\n"
                       "Attempting to continue.\n");
            return;
        }

        CS_ERROR("%s: failed to create path at '%s'", CP(name_), CP(path_));
    }

    ret = mount(C(ROOT_LABEL), CP(path_), "cpuset", 0, 0);
    if (ret)
        CS_ERROR("Couldn't create root cpuset at '%s'", CP(path_));

    CPUs_->reserve(number_cpus_);
    for (unsigned int i = 0; i < number_cpus_; ++i)
        CPUs_->push_back(i);

    // leave pids_ empty for lack of anything better
}

/**
    Should I worry about chmod'ing any created directories?
*/

cpuset::cpuset
(
    const std::string &name,
    const std::string &cpus,
    cpuset *parent_cpuset,
    bool cpu_is_exclusive,
    bool mem_is_exclusive,
    bool migrate_memory,
    bool notify_on_release
):
    name_(new std::string(name)),
    path_(),
    CPUs_(new cpu_vector_t()),
    pids_(new pid_vector_t()),
    cpu_is_exclusive_(cpu_is_exclusive),
    mem_is_exclusive_(mem_is_exclusive),
    migrate_memory_(migrate_memory),
    notify_on_release_(notify_on_release),
    parent_(parent_cpuset),
    children_()
{
    if (!number_cpus_)
        CS_RUNTIME("Don't know how many CPUs are in system: "
                   "call set_cpu_count() before using!");

    int ret;
    std::string cpulist_parsed;
    std::string cmd;
    std::string base_path((parent_ ? parent_->path() : CPUSET_PATH));
    path_ = new std::string(base_path + name + "/");

    // Check for parent's directory
    cmd = DIR_EXISTS_COMMAND + base_path;
    ret = system(C(cmd));
    if (ret)        // test of parent directory failed: didn't return 0
        CS_ERROR("%s: couldn't create cpuset: directory of parent '%s' "
                 "not found?", C(name), C(parent_->name()));

    // See if list of CPUs is valid before creating subdirectory: less to
    // clean up if it isn't.
    parse_cpulist(cpus, &cpulist_parsed);

    // make the child directory
    ret = mkdir(CP(path_), 0755);
    if (ret)
        CS_ERROR("%s: failed to create path at '%s'", CP(name_), CP(path_));
#if 0
    // go to that directory
    ret = chdir(CP(path_));
    if (ret)
        CS_ERROR("Couldn't move to cpuset dir for cpuset '%s': '%s'",
                 CP(name_), CP(path_));
#endif

    try
    {
        add_cpulist_to_cpuset(cpulist_parsed);
    } catch (std::exception &e)
    {
        CS_CPRINT("Failed setting cpus for '%s': trying to clean up",CP(name_));
        ret = rmdir(CP(path_));
        if (ret)
            CS_REPORT("%s: failed to remove CPUset: rmdir() failed", CP(name_));
        delete name_;
        delete path_;
        delete CPUs_;
        delete pids_;
        // Can't have children yet: this is the constructor.
        throw;
    }

    // set the various variables
    ret = system(cpu_is_exclusive
                 ? C(CPU_EXCLUSIVE_ON(*path_))
                 : C(CPU_EXCLUSIVE_OFF(*path_)));
    if (ret)
        CS_ERROR("configuring cpu_exclusive for cpuset '%s'", CP(name_));

    ret = system(mem_is_exclusive
                 ? C(MEM_EXCLUSIVE_ON(*path_))
                 : C(MEM_EXCLUSIVE_OFF(*path_)));
    if (ret)
        CS_ERROR("configuring mem_exclusive for cpuset '%s'", CP(name_));

    ret = system(migrate_memory
                 ? C(MEM_MIGRATE_ON(*path_))
                 : C(MEM_MIGRATE_OFF(*path_)));
    if (ret)
        CS_ERROR("configuring memory_migrate for cpuset '%s'", CP(name_));

    ret = system(notify_on_release
                 ? C(RELEASE_NOTIFY_ON(*path_))
                 : C(RELEASE_NOTIFY_OFF(*path_)));
    if (ret)
        CS_ERROR("configuring notify on release for cpuset '%s'", CP(name_));

    parent_->children_.push_back(this);
}

/**
    You only have to unmount the root cpuset before doing an rmdir.
*/

void
cpuset::remove_root_cpuset(void)
{
    int ret = umount(CP(path_));
    if (ret)
        CS_REPORT("%s: failed to unmount cpuset", CP(name_));

    ret = rmdir(CP(path_));
    if (ret)
        CS_REPORT("%s: failed to remove CPUset: rmdir() failed", CP(name_));
}

/**
    A cpuset cannot be removed if the cpuset is in use: has cpusets inside,
    or has processes attached.
*/

cpuset::~cpuset(void)
{
    int ret;

    CS_CPRINT("Deleting CPU set with name '%s'\n", CP(name_));

    // delete children
    cpuset_vector_t::iterator i = children_.begin();
    cpuset_vector_t::iterator e = children_.end();
    for ( ; i != e; ++i)
    {
        CS_CPRINT("Deleting child '%s'\n", C((*i)->name()));
        delete *i;
    }

    ret = chdir("/");
    if (ret)
        CS_REPORT("%s: unable to \"cd /\" in order to unmount set", CP(name_));

    if (parent_)
    {
        ret = rmdir(CP(path_));
        if (ret)
            CS_REPORT("%s: failed to remove CPUset: rmdir() failed", CP(name_));
    } else
        remove_root_cpuset();

    delete name_;
    delete path_;
    delete CPUs_;
    delete pids_;
}

/**
    Is 'cpu_num' valid?
*/

bool
cpuset::bad_cpu(cpuid_t cpu_num) const
{
    return cpu_num >= number_cpus_;
}

/**
    Misleading name: inserts a CPU onto the list of CPUs, not onto the
    cpuset itself, that's add_cpu_to_cpuset().  This takes an ASCII
    representation of processor like '42' and does all the necessary work to
    get that CPU value into the list (if a valid CPU).
*/

cpuid_t
cpuset::insert_cpu(const std::string &cpu_string)
{
    if (cpu_string.length() == 0)
        CS_RUNTIME("%s: 0 length CPU list", CP(name_));

    cpuid_t cpu = static_cast<cpuid_t>(atoi(C(cpu_string)));
    if (cpu >= number_cpus_)
        CS_RUNTIME("%s: Illegal CPU # %u", CP(name_), cpu);

    CPUs_->push_back(cpu);
    return cpu;
}

/**
    Breaks down provided string into a big long list of individual CPUs.
    This is done so that we can keep a list of which CPUs are in a cpuset.
    I figure this might come in handy at some point, though it's more of a
    gigantic pain in the ass right now.  Oh well, should catch syntax errors
    when specifying cpuset info, at least.

    What's legal is:

    single CPU: 1
    a range:    1-2
    a list:     1,2
    a combination of lists and ranges, e.g:
        1-2,4
        4,1-2
        1-2,3,4-5

    NO spaces are allowed in the list!

    Not allowed:
        open ranges         1-
        incomplete lists    1,2,
        screwed up stuff    1-,
        spaces              1 - 2

    Each CPU is checked against the valid maximum for the machine.
*/

void
cpuset::parse_cpulist(const std::string &cpus, std::string *cpu_list)
{
# if 0
    #define X(text) std::string(text)

    // errno doesn't get set correctly for system
    std::string cmd(X("/bin/echo ") + cpus + X(" > cpus"));
    int ret = system(C(cmd));
    if (ret)
        CS_RUNTIME("%s: Failed adding cpus", CP(name_));

    #undef X
#else
    // For each character in string
    cpuid_t cpu = 0;
    std::string cpu_string;
    bool numeric = false;

    enum cpu_state { NONE, IN_VALUE, IN_RANGE };
    cpu_state state = NONE;

    std::string::const_iterator i = cpus.begin();
    const std::string::const_iterator e = cpus.end();

    for ( ; i != e; ++i)
    {
        char c = *i;
        numeric = ((c >= '0') && (c <= '9'));     // numeric data?
        switch (state)
        {

        case IN_VALUE:
            if (numeric)
            {
                cpu_string += c;                   // append digit to string
            } else
            {
                if (c == '-')                      // 1st CPU in range
                {
                    cpu = insert_cpu(cpu_string);
                    cpu_string.clear();
                    state = IN_RANGE;
                } else if (c == ',')               // singleton
                {
                    cpu = insert_cpu(cpu_string);
                    cpu_string.clear();
                    state = NONE;
                } else
                {
                    CS_RUNTIME("%s: CPU list for cpuset: '%s' -> '%s'",
                               CP(name_), C(cpus), C(cpu_string));
                }
            }
            break;

        case IN_RANGE:
            if (numeric)            // upper value on range
            {
                cpu_string += c;
            } else if (c == ',')   // end range, begin new range, list || value
            {
                // could happen with, e.g. "1-,"
                if (cpu_string.length() == 0)
                    CS_RUNTIME("bad CPU range for cpuset '%s': '%s'",
                               CP(name_), C(cpus));

                cpuid_t low_cpu = cpu;
                cpu = atoi(C(cpu_string)); // last CPU in range
                if (bad_cpu(cpu))
                    CS_RUNTIME("%s: Illegal upper range CPU %u > max %u: %s",
                               CP(name_), cpu, number_cpus_, C(cpus));
                cpu_string.clear();
                state = NONE;

                // add all CPUs in range to CPUset (inclusive range)
                for ( ; low_cpu <= cpu; ++low_cpu)
                    CPUs_->push_back(low_cpu);

            } else
                CS_RUNTIME("malformatted CPU list for cpuset '%s': '%s'",
                           CP(name_), C(cpus));
            break;

        case NONE:
            if (numeric)
            {
                cpu_string += c;           // append digit to string
                state = IN_VALUE;
            } else
                CS_RUNTIME("%s: Non-numerical while not parsing CPU: %s",
                           CP(name_), C(cpus));

            break;
        default:
            CS_RUNTIME("malformatted CPU list for cpuset '%s': '%s'",
                       CP(name_), C(cpus));
        }
    }

    // What we were doing when string ended?
    switch (state)
    {

    case IN_VALUE:  // working on a singleton
        insert_cpu(cpu_string);
        cpu_string.clear();
        break;

    case IN_RANGE:
        if (numeric)
        {
            cpuid_t low_cpu = cpu + 1;
            cpu = atoi(C(cpu_string)); // last CPU in range
            if (bad_cpu(cpu))
                CS_RUNTIME("Illegal upper range CPU %u in cpuset %s: %s",
                           cpu, CP(name_), C(cpus));

            // add all CPUs in range to CPUset (inclusive range)
            for ( ; low_cpu <= cpu; ++low_cpu)
                CPUs_->push_back(low_cpu);

        } else  // ended after a '-' with no last range value
            CS_RUNTIME("end of range with no value for cpuset '%s': '%s'",
                       CP(name_), C(cpus));
        break;

    case NONE:  // fall-through
    default:
        CS_RUNTIME("malformatted CPU list for cpuset '%s': '%s'",
                   CP(name_), C(cpus));
    }

    cpu_list->clear();
    const unsigned int size = CPUs_->size();
    for (unsigned int i = 0; i < size; ++i)
    {
        cpu = (*CPUs_)[i];
        char digits[cpu / 10 + 1 + 1];  // + 1 for comma, + 1 for '\0'
        if ((i + 1) < size)
            sprintf(digits, "%u,", cpu);// has comma
        else
            sprintf(digits, "%u", cpu); // No tailing comma
        (*cpu_list) += digits;
    }
    CS_CPRINT("String is '%s'\n",  CP(cpu_list));
#endif
}

/**
    Add the processor 'cpu' to those available to the cpuset.

    XXX: and 'mems', which is new?
*/

void
cpuset::add_cpulist_to_cpuset(const std::string &cpulist)
{

    std::string cmd(ECHO +  cpulist + X(" > ") + *path_ + "cpus");
    std::string mem_cmd(ECHO + cpulist + X(" > ") + *path_ + "mems");

    CS_CPRINT("Trying to execute '%s'\n", C(cmd));
    int ret = system(C(cmd));
    if (ret)
        CS_ERROR("%s: failed adding CPUs '%s'", CP(name_), C(cmd));

    CS_CPRINT("Trying to execute '%s'\n", C(mem_cmd));
    ret = system(C(mem_cmd));
    if (ret)
        CS_ERROR("%s: failed adding mems '%s'", CP(name_), C(mem_cmd));

}

////////////////////////////////////////////////////////////////////////////////
// Public
////////////////////////////////////////////////////////////////////////////////

/**
    Add the process 'pid' to the cpuset.
*/ 

void
cpuset::add_task(pid_t pid)
{
    int length = 0;
    for (pid_t x = pid ; x ; x /= 10)
        ++length; 
    
    char digits[length + 1];
    sprintf(digits, "%u", pid);
    std::string cmd(ECHO + X(digits) + X(" > tasks"));

    int ret = system(C(cmd));
    if (ret)
        CS_ERROR("%s: Failed adding task %u: %s", CP(name_), pid, C(cmd));

    pids_->push_back(pid);
}

/**
    Puts out everything we know.
*/

std::string
cpuset::print(void) const
{
    std::ostringstream o;
    o << "name: " << *name_
      << "\npath: " << *path_;

    o << "\n#CPUs in set == " << CPUs_->size() << "\n";
    for (unsigned int i = 0; i < CPUs_->size(); ++i)
        o << (*CPUs_)[i] << " ";

    o << "\nProcesses in set == " << pids_->size() << "\n";
    for (unsigned int i = 0; i < pids_->size(); ++i)
        o << (*pids_)[i] << " ";

    o << "\nCPU exclusive: " << (cpu_is_exclusive_ ? "Yes" : "No")
      << "\nmem exclusive: " << (mem_is_exclusive_ ? "Yes" : "No")
      << "\nmigrate memory: " << (migrate_memory_ ? "Yes" : "No")
      << "\nnotify on release: " << (notify_on_release_ ? "Yes" : "No") << "\n";

    if (parent_)
        o << "Child CPUset of '" << parent_->name() << "'\n";
    else
        o << "This is the root cpuset: no one's child\n";

    o << "Num child cpusets: " << children_.size() << "\n";
    for (unsigned int i = 0; i < children_.size(); ++i)
    {
        o << "[" << i << "]: \n"
          << DELIMITER << "\n"
          << *(children_[i]) << "\n"
          <<  DELIMITER << "\n";
    }

    return o.str();
}


////////////////////////////////////////////////////////////////////////////////
// Not in class
////////////////////////////////////////////////////////////////////////////////

/**
    Standard output operator.
*/

std::ostream &
operator <<(std::ostream &o, const cpuset &s)
{
    o << s.print();
    return o;
}

#undef X
#undef CS_NAME
#undef CS_CPRINT
#undef CS_VPRINT
#undef CS_RUNTIME
#undef CS_DP
#undef S
#undef SP

