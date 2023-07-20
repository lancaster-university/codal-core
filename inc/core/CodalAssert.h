#include "CodalConfig.h"
#include "CodalDmesg.h"

/**
 * Intended for use in test suites - should not be used in production code!
 * 
 * (although they will compile to nothing if assertions are disabled...)
 */

#ifndef CODAL_ASSERT_H
#define CODAL_ASSERT_H

#ifndef __FILENAME__
#define __FILENAME__ (__builtin_strrchr("/" __FILE__, '/') + 1)
#endif

#if CONFIG_ENABLED(CODAL_ENABLE_ASSERT)
    void __codal_assert__( const char * file, const int line, bool condition, const char * expr, const char * message );
    void __codal_assert_pass__( const char * file, const int line, const char * message );
    void __codal_assert_fail__( const char * file, const int line, const char * message );
    #define assert(condition, message) __codal_assert__( __FILENAME__, __LINE__, (condition) == true, #condition, message )
    #define assert_pass(message) __codal_assert_pass__( __FILENAME__, __LINE__, message )
    #define assert_fail(message) __codal_assert_fail__( __FILENAME__, __LINE__, message )
#else
    #define assert(x,y)
    #define assert_pass(message)
    #define assert_fail(message)
#endif

#endif