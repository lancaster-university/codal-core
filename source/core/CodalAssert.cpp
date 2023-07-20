#include "codal_target_hal.h"
#include "CodalAssert.h"
#include "CodalDmesg.h"

uint32_t test_failures = 0;
uint32_t test_successes = 0;

void __codal_assert__( const char * file, const int line, bool condition, const char * expr, const char * message ) {
    DMESGN( "%s:%d; %s -> ", file, line, expr );

    if( condition ) {
        DMESG( "PASS" );
        test_successes++;
        return;
    }
    DMESG( "FAIL" );
    if( message != NULL )
        DMESG( "\t%s", message );
    
    #if CONFIG_ENABLED(CODAL_ASSERT_PANIC)
        target_panic( 999 );
    #else
        DMESG( "\tFailed %d tests", ++test_failures );
    #endif
}

void __codal_assert_pass__( const char * file, const int line, const char * message ) {
    test_successes++;

    DMESG( "Test SUCCESS in %s:%d (PASS: %d, FAIL: %d)", file, line, test_successes, test_failures );
    if( message != NULL )
        DMESG( "\t%s", message );
}

void __codal_assert_fail__( const char * file, const int line, const char * message ) {
    DMESG( "Test FAIL in %s:%d (PASS: %d, FAIL: %d)", file, line, test_successes, test_failures );
    
    if( message != NULL )
        DMESG( "\t%s", message );
    
    #if CONFIG_ENABLED(CODAL_ASSERT_PANIC)
        target_panic( 999 );
    #else
        DMESG( "\tFailed %d tests", ++test_failures );
    #endif
}