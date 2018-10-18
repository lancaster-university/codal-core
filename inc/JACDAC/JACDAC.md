# JACDAC: Joint Asynchronous Communications, Device Agnostic Control.

## What is JACDAC?

JACDAC is a single wire serial protocol for the plug and play of accessories for embedded computers.

## Why do we need _another_ protocol?

Imagine a scenario you'd like to connect two of the same board (with on-board peripherals) to talk one another over a wire. Your options are:

### 1. Use I2C or SPI

Both of these protocols are very low-level and use static addresses to configure registers. They each rely on there being a single _central_ device that controls all connected _peripherals_ on the bus; multi-master scenarios are barely supported. Connecting two of the same boards together will result in addressing conflicts due to static addresses and competition over peripherals. Sidestepping this issue, how would you design an elegant peer-to-peer networking interface using I2c or SPI?

### 2. Create a Custom Serial Protocol

Due to the static nature of SPI and I2C, the next best option is to create a custom serial protocol for peer-to-peer networks of devices over a wire. In implementing a custom serial protocol, design of bus arbitration and resolution, and reliability will have to be carefully considered. Realising a custom protocol will require a lot of work and result in potentially non-reusable, application specific code.

### 3. Go wireless

Simply adding a dongle to your board will allow you to network using higher level abstractions like http, or tcp sockets. However, this will add cost and complexity to your project.

