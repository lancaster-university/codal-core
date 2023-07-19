#include "codal_target_hal.h"
#include "CodalAssert.h"
#include "CodalDmesg.h"

uint32_t failures = 0;

void __codal_assert__( const char * file, const int line, bool condition, const char * expr, const char * expect, const char * message ) {
    if( condition )
        return;
    DMESGF( "Assertion failure in %s line %d", file, line );
    DMESGF( "\tThe expression (%s) did not evaluate to (%s)", expr, expect );
    DMESGF( "\t%s", message );
    #if CONFIG_ENABLED(CODAL_ASSERT_PANIC)
        target_panic( 999 );
    #else
        DMESGF( "\tFailed %d tests", ++failures );
    #endif
}