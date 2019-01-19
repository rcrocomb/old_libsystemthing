#ifndef PROGRAM_IO_H
#define PROGRAM_IO_H

/*!
    Author: Robert Crocombe
    Classification: Unclassified
    Initial Release Date: 2004-06-14

    I very much to prefer to use printf-like output statements.  They are
    more easily wrapped in macros that can be compiled away, etc.  This file
    proides the means by which this can be accomplished and yet still
    eventually push the output to std::cout.  I don't know of this is better
    than sync_with_stdio(), but since that routine is supposedly Performance
    Death, I thought I'd try.

    Uses more stack than I'd like :(.

    This code is gcc-specific due to use of '##args'.  See C-99 and
    google and so forth for details.
*/

#include <iostream>
#include <string>               // std::string
#include <stdexcept>            // std::runtime_error

#include <errno.h>              // errno

enum
{
    DEFAULT_BUFFER_SIZE = 2048 + 1,

    DEBUG_0 =   0,
    DEBUG_1 =   1,
    DEBUG_2 =   2,
    DEBUG_3 =   3,
    DEBUG_4 =   4,
    DEBUG_5 =   5
};

extern int debug_level;

/*!
    Transforms printf-like output into a single 'char *' of maximum length
    DEFAULT_BUFFER_SIZE (truncates if input data is longer) and outputs it
    via std::cout.
*/

// 'c' for compact
#define cprint(format, args...) do { \
                            char Q[DEFAULT_BUFFER_SIZE]; \
                            snprintf(Q, sizeof(Q), format, ##args); \
                            std::cout << Q << std::flush; \
                          } while (0)

// 'v' for verbose
#define vprint(format, args...) do { \
                            char Z[DEFAULT_BUFFER_SIZE]; \
                            char Q[DEFAULT_BUFFER_SIZE]; \
                            snprintf(Z, sizeof(Z), format, ##args); \
                            snprintf(Q, sizeof(Q), "%s:%s:%d: %s",\
                                     __FILE__, __func__, __LINE__, Z); \
                            std::cout << Q << std::flush; \
                          } while (0)

#define warning(format, args...) do { \
                            char Z[DEFAULT_BUFFER_SIZE]; \
                            char Q[DEFAULT_BUFFER_SIZE]; \
                            snprintf(Z, sizeof(Z), format, ##args); \
                            snprintf(Q, sizeof(Q), "%s:%s:%d: WARNING: %s",\
                                     __FILE__, __func__, __LINE__, Z); \
                            std::cout << Q << std::flush; \
                          } while (0)


/*!
    Two buffers are needed: one to hold the result of crushing any
    error-related printf-like() bestrewn text into a single string, and
    then a second to smash this string and the return string from
    strerror() into yet another string to be output.

    Note that despite the fact that both buffers are
    DEFAULT_BUFFER_SIZE in size, that the overall string must still be
    < DEFAULT_BUFFER_SIZE in length, not 2 * DEFAULT_BUFFER_SIZE.
   
    Use this for OS-related errors like failures to allocate memory or
    something.
*/

#define error(format, args...) do { \
                            char Z[DEFAULT_BUFFER_SIZE]; \
                            char Q[DEFAULT_BUFFER_SIZE]; \
                            snprintf(Z, sizeof(Z), format, ##args); \
                            snprintf(Q, sizeof(Q), "%s:%s:%d:\nERROR: %s -- %s\n",\
                                     __FILE__, __func__, __LINE__, Z, strerror(errno)); \
                            std::cout << Q << std::flush; \
                            throw std::runtime_error(Q); \
                          } while (0)

/*!
    As above, but use this for things that we have screwed up: assertions
    we've failed or the failure of data to arrive as we expected, etc.  By
    far the more likely case.

    The distinction is because we don't need a strerror() for non-OS-related
    junk.
*/

#define runtime(format, args...) do { \
                            char Z[DEFAULT_BUFFER_SIZE]; \
                            char Q[DEFAULT_BUFFER_SIZE]; \
                            snprintf(Z, sizeof(Z), format, ##args); \
                            snprintf(Q, sizeof(Q), "%s:%s:%d:\nRUNTIME error: %s\n",\
                                     __FILE__, __func__, __LINE__, Z); \
                            std::cout << Q << std::flush; \
                            throw std::runtime_error(Q); \
                          } while (0)

// This is like error(), above, but doesn't throw an exception: it just
// reports what error is thinking.
#define report_error(format, args...) do { \
                            char Z[DEFAULT_BUFFER_SIZE]; \
                            char Q[DEFAULT_BUFFER_SIZE]; \
                            snprintf(Z, sizeof(Z), format, ##args); \
                            snprintf(Q, sizeof(Q), "%s:%s:%d:\nBADNESS: %s -- %s\n",\
                                     __FILE__, __func__, __LINE__, Z, strerror(errno)); \
                            std::cout << Q << std::flush; \
                          } while (0)

/**
    Takes a variable name 'field', and print out both the field name and the
    value of the field in hexidecial: intended for numeric values.  May not
    work on uint8_t since that may get treated as a char.

    Use like:

    cout << HEX_THIS(some_variable);
*/
#include <iomanip>
#define HEX_THIS(field) \
    "\n" << #field << ": 0x" \
         << std::setw(2 * sizeof(field)) << std::setfill('0') << std::hex \
         << (field)


// I get tired of writing BLAH.c_str() or BLAH->c_str() over and over.
#define C(string) (string).c_str()
#define CP(string_p) (string_p)->c_str()

////////////////////////////////////////////////////////////////////////////////

/*!
    According to the 2005 GCC Summit Proceedings, it's relatively important
    to fully remove debugging code and not just have comparisons keep from
    outputting any data.  They weren't specific, but were annoyed at the
    libc folks.  I would think branch prediction here would be very easy,
    but I dunno.  Maybe code just gets to be too branchy.
*/
#if !DEBUG_ON
    #define CPRINT_WITH_NAME(name, format, args...) do {} while(0)
    #define VPRINT_WITH_NAME(name, format, args...) do {} while(0) 
    #define DP(level, name, format, args...)        do {} while(0)
    #define DEBUG_DECLARE(x)
#else

    #define CPRINT_WITH_NAME(name, format, args...) \
        cprint("%s: "format, name.c_str(), ##args)

    #define VPRINT_WITH_NAME(name, format, args...) \
        vprint("%s: "format, name.c_str(), ##args)

    /*
        Only print the info in 'format, args...' if priority attached to
        debugging info (specified by 'debug') is of a higher priority (lower
        numeric value) than the debugging print level (as specified by
        debug_level).  So a DP(2, args...) statement prints if debug_level
        is >= 2, while a DP(5, args...) would not.
    */
    #define DP(debug, format, args...) \
    do { \
        if ((debug) <= debug_level) \
            cprint(format, ##args); \
    } while (0)

    #define DEBUG_DECLARE(x)                        x

#endif  // DEBUG_ON

/*
    These seem important enough to keep at all times.
*/

#define ALWAYS_WITH_NAME(name, format, args...) \
    cprint("%s: "format, name.c_str(), ##args)

#define WARNING_WITH_NAME(name, format, args...) \
    warning("%s: "format, name.c_str(), ##args)

#define ERROR_WITH_NAME(name, format, args...) \
    error("%s: "format, name.c_str(), ##args)

#define RUNTIME_WITH_NAME(name, format, args...) \
    runtime("%s: "format, name.c_str(), ##args)

#define REPORT_WITH_NAME(name, format, args...) \
    report_error("%s: "format, name.c_str(), ##args)

#endif  // PROGRAM_IO_H

