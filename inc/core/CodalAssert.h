#include "CodalConfig.h"
#include "CodalDmesg.h"

#ifndef CODAL_ASSERT_H
#define CODAL_ASSERT_H

#ifndef __FILENAME__
#define __FILENAME__ (__builtin_strrchr("/"__FILE__, '/') + 1)
#endif

#if CONFIG_ENABLED(CODAL_ENABLE_ASSERT)
    void __codal_assert__( const char * file, const int line, bool condition, const char * expr, const char * expect, const char * message );
    #define assert_true(condition, message) __codal_assert__( __FILENAME__, __LINE__, (condition) == true, #condition, "true", message )
    #define assert_false(condition, message) __codal_assert__( __FILENAME__, __LINE__, (condition) == false, #condition, "false", message )
    #define assert_never(message) __codal_assert__( __FILENAME__, __LINE__, false, "assert", "not executed", message )
#else
    #define assert_true(x,y)
    #define assert_false(x,y)
    #define assert_never(x)
#endif

#endif