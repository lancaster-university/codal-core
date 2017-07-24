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

/**
  * Compile time configuration options for the codal device runtime.
  */

#ifndef CODAL_CONFIG_H
#define CODAL_CONFIG_H

#include "TargetConfig.h"

// Enables or disables the DeviceHeapllocator. Note that if disabled, no reuse of the SRAM normally
// reserved for SoftDevice is possible, and out of memory condition will no longer be trapped...
// i.e. panic() will no longer be triggered on memory full conditions.
#ifndef DEVICE_HEAP_ALLOCATOR
#define DEVICE_HEAP_ALLOCATOR                 0
#endif

// Block size used by the allocator in bytes.
// n.b. Currently only 32 bits (4 bytes) is supported.
#ifndef DEVICE_HEAP_BLOCK_SIZE
#define DEVICE_HEAP_BLOCK_SIZE                4
#endif

// The proportion of SRAM available on the mbed heap to reserve for the codal device heap.
#ifndef DEVICE_NESTED_HEAP_SIZE
#define DEVICE_NESTED_HEAP_SIZE               0.75
#endif

// If enabled, RefCounted objects include a constant tag at the beginning.
// Set '1' to enable.
#ifndef DEVICE_TAG
#define DEVICE_TAG                            0
#endif

//
// Fiber scheduler configuration
//

// Scheduling quantum (milliseconds)
// Also used to drive the codal device runtime system ticker.
#ifndef SCHEDULER_TICK_PERIOD_US
#define SCHEDULER_TICK_PERIOD_US                   6000
#endif

//
// Message Bus:
// Default behaviour for event handlers, if not specified in the listen() call
//
// Permissable values are:
//   MESSAGE_BUS_LISTENER_REENTRANT
//   MESSAGE_BUS_LISTENER_QUEUE_IF_BUSY
//   MESSAGE_BUS_LISTENER_DROP_IF_BUSY
//   MESSAGE_BUS_LISTENER_IMMEDIATE

#ifndef EVENT_LISTENER_DEFAULT_FLAGS
#define EVENT_LISTENER_DEFAULT_FLAGS            MESSAGE_BUS_LISTENER_QUEUE_IF_BUSY
#endif

//
// Maximum event queue depth. If a queue exceeds this depth, further events will be dropped.
// Used to prevent message queues growing uncontrollably due to badly behaved user code and causing panic conditions.
//
#ifndef MESSAGE_BUS_LISTENER_MAX_QUEUE_DEPTH
#define MESSAGE_BUS_LISTENER_MAX_QUEUE_DEPTH    10
#endif

//Configures the default serial mode used by serial read and send calls.
#ifndef DEVICE_DEFAULT_SERIAL_MODE
#define DEVICE_DEFAULT_SERIAL_MODE            SYNC_SLEEP
#endif

//
// I/O Options
//
#ifndef DEVICE_COMPONENT_COUNT
#define DEVICE_COMPONENT_COUNT               30
#endif
//
// Define the default mode in which the digital input pins are configured.
// valid options are PullDown, PullUp and PullNone.
//
#ifndef DEVICE_DEFAULT_PULLMODE
#define DEVICE_DEFAULT_PULLMODE                PullNone
#endif

//
// Panic options
//

// Enable this to invoke a panic on out of memory conditions.
// Set '1' to enable.
#ifndef DEVICE_PANIC_HEAP_FULL
#define DEVICE_PANIC_HEAP_FULL                1
#endif

//
// Debug options
//
#ifndef DEVICE_DMESG
#define DEVICE_DMESG                          0
#endif

// When non-zero internal debug messages (DMESG() macro) go to a in-memory buffer of this size (in bytes).
// It can be inspected from GDB (with 'print codalLogStore'), or accessed by the application.
// Typical size range between 512 and 4096. Set to 0 to disable.
#ifndef DEVICE_DMESG_BUFFER_SIZE
#define DEVICE_DMESG_BUFFER_SIZE              1024
#endif

// Enable this to route debug messages through the USB serial interface.
// n.b. This also disables the user serial port 'uBit.serial'.
// Set '1' to enable.
#ifndef DEVICE_DBG
#define DEVICE_DBG                            0
#endif

// Enable this to receive diagnostic messages from the heap allocator via the USB serial interface.
// n.b. This requires DEVICE_DBG to be defined.
// Set '1' to enable.
#ifndef DEVICE_HEAP_DBG
#define DEVICE_HEAP_DBG                       0
#endif

// Versioning options.
// We use semantic versioning (http://semver.org/) to identify differnet versions of the codal device runtime.
// Where possible we use yotta (an ARM mbed build tool) to help us track versions.
// if this isn't available, it can be defined manually as a configuration option.
//
#ifndef DEVICE_DAL_VERSION
#define DEVICE_DAL_VERSION                    "unknown"
#endif

#ifndef DEVICE_USB
#define DEVICE_USB                            0
#endif

//
// Helper macro used by the codal device runtime to determine if a boolean configuration option is set.
//
#define CONFIG_ENABLED(X) (X == 1)
#define CONFIG_DISABLED(X) (X != 1)

#if CONFIG_ENABLED(DEVICE_DBG)
extern TARGET_DEBUG_CLASS* SERIAL_DEBUG;
#endif

#endif
