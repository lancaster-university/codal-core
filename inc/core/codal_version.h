#ifndef CODAL_VERSION_H
#define CODAL_VERSION_H

    #define STR_HELPER(x) #x
    #define STR(x) STR_HELPER(x)

    #ifndef CODAL_VERSION_MAJOR
        #define CODAL_VERSION_MAJOR 0
    #endif

    #ifndef CODAL_VERSION_MINOR
        #define CODAL_VERSION_MINOR 0
    #endif

    #ifndef CODAL_VERSION_PATCH
        #define CODAL_VERSION_PATCH 0
    #endif

    #ifndef CODAL_VERSION_STRING
        #define CODAL_VERSION_STRING  "v" STR(CODAL_VERSION_MAJOR) "." STR(CODAL_VERSION_MINOR) "." STR(CODAL_VERSION_PATCH)
    #endif

#endif