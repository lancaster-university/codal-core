/*
The MIT License (MIT)

Copyright (c) 2017 Lancaster University.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

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
    void __codal_fault__( const char * file, const int line, const char * message  );
    void __codal_assert_pass__( const char * file, const int line, const char * message );
    void __codal_assert_fail__( const char * file, const int line, const char * message );
    #define assert(condition, message) __codal_assert__( __FILENAME__, __LINE__, (condition) == true, #condition, message )
    #define assert_fault(message) __codal_fault__( __FILENAME__, __LINE__, message )
    #define assert_pass(message) __codal_assert_pass__( __FILENAME__, __LINE__, message )
    #define assert_fail(message) __codal_assert_fail__( __FILENAME__, __LINE__, message )
#else
    #define assert(x,y)
    #define assert_fault(message)
    #define assert_pass(message)
    #define assert_fail(message)
#endif

#endif