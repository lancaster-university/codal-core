# This file is no longer auto-generated to make the repository builds with GCC
# and ARMCC no matter what.

cmake_minimum_required(VERSION 2.8.12)

enable_language(ASM)

include("${CODAL_UTILS_LOCATION}")
RECURSIVE_FIND_DIR(INCLUDE_DIRS "./inc" "*.h")

set(CODAL_CPP_FILES
    "source/core/MemberFunctionCallback.cpp"
    "source/core/CodalCompat.cpp"
    "source/core/CodalDmesg.cpp"
    "source/core/CodalDevice.cpp"
    "source/core/DeviceComponent.cpp"
    "source/core/DeviceFiber.cpp"
    "source/core/DeviceHeapAllocator.cpp"
    "source/core/DeviceListener.cpp"
    "source/core/Timer.cpp"
    "source/core/CodalUSB.cpp"

    "source/types/ManagedString.cpp"
    "source/types/Matrix4.cpp"
    "source/types/DeviceEvent.cpp"
    "source/types/DataStream.cpp"
    "source/types/ManagedBuffer.cpp"
    "source/types/RefCounted.cpp"
    "source/types/RefCountedInit.cpp"

    "source/drivers/AbstractButton.cpp"
    "source/drivers/DeviceButton.cpp"
    "source/drivers/MultiButton.cpp"
    "source/drivers/TouchButton.cpp"
    "source/drivers/TouchSensor.cpp"
    "source/drivers/MessageBus.cpp"
    "source/drivers/HID.cpp"
    "source/drivers/LIS3DH.cpp"
    "source/drivers/AnalogSensor.cpp"
    "source/drivers/LinearAnalogSensor.cpp"
    "source/drivers/NonLinearAnalogSensor.cpp"
    "source/drivers/LevelDetector.cpp"
    "source/drivers/Synthesizer.cpp"
)

execute_process(WORKING_DIRECTORY "." COMMAND "git" "log" "--pretty=format:%h" "-n" "1" OUTPUT_VARIABLE git_hash)
execute_process(WORKING_DIRECTORY "." COMMAND "git" "rev-parse" "--abbrev-ref" "HEAD" OUTPUT_VARIABLE git_branch OUTPUT_STRIP_TRAILING_WHITESPACE)

if ("${git_branch}" STREQUAL "master")
    set(CODAL_VERSION_STRING "${CODAL_VERSION_STRING}")
else()
    set(CODAL_VERSION_STRING "${CODAL_VERSION_STRING}-${git_branch}-g${git_hash}")
endif()

set(CODAL_VERSION_FLAGS "-DCODAL_VERSION=\\\"${CODAL_VERSION_STRING}\\\"")

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CODAL_VERSION_FLAGS}")

if(CMAKE_COMPILER_IS_GNUCC)
  file(REMOVE "./source/asm/CortexContextSwitch.s")
  configure_file("./source/asm/CortexContextSwitch.s.gcc" "./source/asm/CortexContextSwitch.s" COPYONLY)
else()
  file(REMOVE "./source/asm/CortexContextSwitch.s")
  configure_file("./source/asm/CortexContextSwitch.s.armcc" "./source/asm/CortexContextSwitch.s" COPYONLY)
endif()

set(CODAL_S_FILES
    "./source/asm/CortexContextSwitch.s"
)

add_library(codal-core
    ${CODAL_CPP_FILES}
    ${CODAL_S_FILES}
)

target_include_directories(codal-core PUBLIC ${INCLUDE_DIRS})

target_link_libraries(codal-core ${CODAL_TARGET_NAME})
