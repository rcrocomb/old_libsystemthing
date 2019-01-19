#include "random_utilities.h"

#include <sstream>
#include <ostream>

#include <limits.h>

#include "program_IO.h"

#include <iomanip>
using std::hex;
using std::dec;
using std::setw;
using std::setfill;

////////////////////////////////////////////////////////////////////////////////
// Global Vars && Macros
////////////////////////////////////////////////////////////////////////////////

namespace random_utilities
{
    // global variables for maintaining random number state
    seed_t erand_state;
    seed_t nrand_state;
    seed_t jrand_state;
}

namespace random_name
{
    const std::string NAME("random");
}

#define RAND_NAME random_name::NAME
#define RAND_CPRINT(fmt, args...)  CPRINT_WITH_NAME(RAND_NAME, fmt, ##args)
#define RAND_VPRINT(fmt, args...)  VPRINT_WITH_NAME(RAND_NAME, fmt, ##args)
#define RAND_WARNING(fmt, args...) WARNING_WITH_NAME(RAND_NAME, fmt, ##args)
#define RAND_ERROR(fmt, args...)   ERROR_WITH_NAME(RAND_NAME, fmt, ##args)
#define RAND_RUNTIME(fmt, args...) RUNTIME_WITH_NAME(RAND_NAME, fmt, ##args)
#define RAND_DP(level, fmt, args...) DP(level, RAND_NAME, fmt, ##args)

////////////////////////////////////////////////////////////////////////////////
// Internal, not in namespace
////////////////////////////////////////////////////////////////////////////////

double spread_random(void);

////////////////////////////////////////////////////////////////////////////////
// Seeding
////////////////////////////////////////////////////////////////////////////////

void
random_utilities::seed_erand48(const seed_t &p)
{
    erand_state =  p;
}

void
random_utilities::seed_nrand48(const seed_t &p)
{
    nrand_state =  p;
}

void
random_utilities::seed_jrand48(const seed_t &p)
{
    jrand_state = p;
}

/*!
    All seeded with same variable.
*/

void
random_utilities::seed_all(const seed_t &p)
{
    seed_erand48(p);    // double [0.0, 1.0) -- uniform distribution
    seed_nrand48(p);    // unsigned long between 0 and 2^31 -- uniform
    seed_jrand48(p);    // signed long between -2^31 and 2^31 -- uniform
}

////////////////////////////////////////////////////////////////////////////////
// Template Specialization
//
// Okay, from the Templates book, you can only put the declaration for
// a specialization in the header: body *must* be in .cpp file, or you
// will get "multiple declaration" errors from the linker (which I
// got).
//
// And they have to be within the "namespace random {}" junk because
// if you try and qualify the template with random:: you get an error
// about "non-templates used as templates".
////////////////////////////////////////////////////////////////////////////////

namespace random_utilities
{

/**
    If it's unspecialized, then you get erand48()'s result multiplied by
    what should (I think) be the max value of your type: ~0.  This should
    give you a result in the range [0, max_value_that_fits_in_T).

    Dunno, though.
*/

template <typename T>
T
get_random(void)
{
    return static_cast<T>(erand48(erand_state.s) * static_cast<T>(~0));
}

////////////////////////////////////////////////////////////////////////////////
// char && uchar: 8 bits
////////////////////////////////////////////////////////////////////////////////

/**
    X = get_random() * UCHAR_MAX: [0, 255)
    Y = X + SCHAR_MIN: [0,255) + (-128): [-128,127).

    Therefore covers all valid values that fit in a signed byte.

    Note that 'char' by itself is not necessarily signed or unsigned, so
    I've got with the explicit 'signed char' here.  Presumably one or the
    other of this or its companion functions will be properly called.
*/

template<>
signed char
get_random(void)
{
    return static_cast<signed char>(get_random() * UCHAR_MAX + SCHAR_MIN);
}

/**
    [0.0, 1.0) * 255 = [0, 255): everything that fits in an unsigned byte.
*/

template<>
unsigned char
get_random(void)
{
    return static_cast<unsigned char>(get_random() * UCHAR_MAX);
}


////////////////////////////////////////////////////////////////////////////////
// short && ushort: 16 bits
////////////////////////////////////////////////////////////////////////////////

/**
    Signed 16 bits.
*/

template<>
short
get_random(void)
{
    return static_cast<short>(get_random() * USHRT_MAX + SHRT_MAX);
}

/**
    Unsigned 16 bits.
*/

template<>
unsigned short
get_random(void)
{
    return static_cast<unsigned short>(get_random() * USHRT_MAX);
}

////////////////////////////////////////////////////////////////////////////////
// int && uint: 32 bits
////////////////////////////////////////////////////////////////////////////////

/**
    get_random<int>: -2^31 to 2^31
*/

template<>
int
get_random(void)
{
    return static_cast<int>(jrand48(jrand_state.s));
}

/**
    get_random<long>: -2^31 to 2^31.
*/

template<>
long
get_random(void)
{
    return jrand48(jrand_state.s);
}

/**
    get_random<unsigned int>: 0 to 2^31.
*/

template<>
unsigned int
get_random(void)
{
    return static_cast<unsigned int>(jrand48(jrand_state.s));
}

/**
    get_random<unsigned long>: 0 to 2^31.
*/

template<>
unsigned long
get_random(void)
{
    return static_cast<unsigned long>(nrand48(nrand_state.s));
}

/**
    Value in the range [0, max)
*/

unsigned int
get_random(unsigned int max)
{
    return static_cast<int>(max * get_random());
}

}   // end "random_utilities" namespace


////////////////////////////////////////////////////////////////////////////////

std::ostream &
random_utilities::operator <<(std::ostream &o, const seed_t &t)
{
    o << "\n"
      << "1st == " << t[0] << " "
      << "2nd == " << t[1] << " "
      << "3rd == " << t[2]
      << "\n";
    return o;
}

////////////////////////////////////////////////////////////////////////////////
// Comparison
////////////////////////////////////////////////////////////////////////////////

/*!
    Returns true if a random number generated between [0,1) is greater
    than the  parameter 'odds'.

    Both numbers must be in the range [0,1)
*/

bool
random_utilities::random_gt(const double odds)
{
    return erand48(erand_state.s) > odds;
}

/*!
    Returns true if a randomly generated number is greater than or
    equal to 'odds'.

    Both numbers must be in the range [0,1)
*/

bool
random_utilities::random_gte(const double odds)
{
    return erand48(erand_state.s) >= odds;
}

/*!
    Returns true if a randomly generated number is less than 'odds'.

    Both numbers must be in the range [0,1)
*/

bool
random_utilities::random_lt(const double odds)
{
    return erand48(erand_state.s) < odds;
}

/*!
    Return true if a randomly generated number is less than or equal
    to 'odds'.

    Both numbers must be in the range [0,1)
*/

bool
random_utilities::random_lte(const double odds)
{
    return  erand48(erand_state.s) <= odds;
}

////////////////////////////////////////////////////////////////////////////////

/**
    X = get_random(): [0.0, 1.0).
    Y = 2 * X: [0.0, 2.0).
    Z = Y - 1: [-1.0, 1.0).

    Note: not exactly uniform since bottom range is closed, top is open.  Is
    evenly distributed within that range.

    "* 2" is a 1 bit bump in the exponent with no mantissa manipulation, and
    thus should not affect the randomness at all.
*/

double
spread_random(void)
{
    return (2.0 * random_utilities::get_random()) - 1.0;
}

/**
    Range of double is [0.0, 1.0)
*/

double
random_utilities::get_random(void)
{
    return erand48(erand_state.s);
}

#undef RAND_NAME
#undef RAND_CPRINT
#undef RAND_VPRINT
#undef RAND_WARNING
#undef RAND_ERROR
#undef RAND_RUNTIME
#undef RAND_DP

