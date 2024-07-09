project(codal-core C CXX)

# Set all our default version values
set( CODAL_VERSION_MAJOR  -1 )
set( CODAL_VERSION_MINOR  -1 )
set( CODAL_VERSION_PATCH  -1 )
set( CODAL_VERSION_HASH   "0000000" )

# Grab the latest tag and hash from git
execute_process( WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../${codal.target.name}" COMMAND git describe --tags --abbrev=0 OUTPUT_VARIABLE git_tag_string )
execute_process( WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../${codal.target.name}" COMMAND "git" "log" "--pretty=format:%h" "-n" "1" OUTPUT_VARIABLE CODAL_VERSION_HASH )

string( REGEX MATCHALL "[0-9]+" git_tag "${git_tag_string}" )

# If we had a valid git response, update the values
list(LENGTH git_tag len)
if( ${len} GREATER 0 )
    list(GET git_tag 0 CODAL_VERSION_MAJOR )
    list(GET git_tag 1 CODAL_VERSION_MINOR )
    list(GET git_tag 2 CODAL_VERSION_PATCH )
endif()

configure_file( "${CMAKE_CURRENT_SOURCE_DIR}/inc/core/codal_version.h.in" "${CMAKE_CURRENT_BINARY_DIR}/gen/codal_version.h" )

RECURSIVE_FIND_DIR(INCLUDE_DIRS "./inc" "*.h")
RECURSIVE_FIND_FILE(SOURCE_FILES "./source" "*.c??")

add_library(codal-core
    ${SOURCE_FILES}
)

target_include_directories(codal-core PUBLIC ${INCLUDE_DIRS} "${CMAKE_CURRENT_BINARY_DIR}/gen/" )
