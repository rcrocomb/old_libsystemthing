#ifndef RANDOM_UTILITIES_H
#define RANDOM_UTILITIES_H

/**
    Author: Robert Crocombe
    Classification: Unclassified
    Initial Release Date: no later than: Sat Dec  4 19:40:20 MST 2004

    Generates random numbers using approved SuSv3 routines.

    Note that various types get fed off various chunks of state, so if you
    are looking for all your values to come from a single stream, then you
    need to be very careful.  It'd probably just be best to use get_random()
    and scale yourself.  You can seed the generators with whatever state
    you'd like, so you should presumably be able to replay if the order of
    calls and whatall are well ordered.
*/

#include <iosfwd>
#include <string>

#include <stdlib.h>         // erand(), etc.
#include <stdint.h>

////////////////////////////////////////////////////////////////////////////////
// Stuff and Junk
////////////////////////////////////////////////////////////////////////////////

namespace random_utilities
{
    // The state that needs to be saved for the SUSv3 random number
    // functions.
    enum
    {
        RANDOM_STATE_ARRAY_SIZE = 3 // 3 x uint16_t = 48 bits (required amount)
    };

    // Keep state in this
    struct seed_t
    {
        uint16_t s[RANDOM_STATE_ARRAY_SIZE];

        seed_t(void) { memset(s, 0, sizeof(s)); }
        seed_t(const uint16_t a, const uint16_t b, const uint16_t c)
        {
            s[0] = a;
            s[1] = b;
            s[2] = c;
        }
        seed_t(uint16_t t[RANDOM_STATE_ARRAY_SIZE]){ memcpy(s, t, sizeof(s)); }
        seed_t(const seed_t &t)                    { memcpy(s, t.s, sizeof(s));}
        ~seed_t(void) {}

        seed_t &operator =(const seed_t &t)
        {
            if (this != &t)
                memcpy(s, t.s, sizeof(s));
            return *this;
        }

        // Return elements of state array
        const uint16_t &operator [](const unsigned int index) const 
        {
            return s[index];
        }
    };

    std::ostream &operator <<(std::ostream &o, const seed_t &t);

    void seed_erand48(const seed_t &p);
    void seed_nrand48(const seed_t &p);
    void seed_jrand48(const seed_t &p);
    void seed_all(const seed_t &p);

    bool random_gt(const double odds);
    bool random_gte(const double odds);
    bool random_lt(const double odds);
    bool random_lte(const double odds);

    double get_random(void);

    template <typename T> T get_random(void);
    // specializations of above
    template<> int get_random(void);
    template<> long get_random(void);
    template<> unsigned long get_random(void);
    template<> unsigned int get_random(void);
    // overload
    unsigned int get_random(unsigned int max);

}   // end 'random_utilities' namespace

#endif  // RANDOM_UTILITIES_H

