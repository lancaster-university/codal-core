import sys

PHYS_STATE_SIZE = 128

log = [5140, 5140, 5396, 4372, 4372, 4372, 4116, 5140, 5396, 4372, 4116, 5140, 5140, 5140, 5140, 5140, 5140, 5140, 5140, 5140, 5140, 5140, 5396, 4372, 4116, 5140, 5396, 4372, 4372, 4116, 5140, 5140, 5140, 5140, 5140, 5140, 5140, 5396, 4372, 4372, 4372, 4372, 4372, 4116, 5140, 5140, 5140, 5396, 4372, 4116, 5140, 5396, 4372, 4372, 5140, 5140, 5140, 5140, 5140, 5140, 5396, 4372, 4116, 5140, 5396, 4372, 4372, 4116, 5140, 5140, 5140, 5140, 5140, 5140, 5140, 5396, 4372, 4372, 4372, 4372, 4372, 4116, 5140, 5140, 5140, 5396, 4372, 4116, 5140, 5396, 4372, 4372, 4116, 5140, 5396, 4372, 4372, 4116, 5140, 5140, 5140, 5396, 4372, 4372, 4372, 4116, 5140, 5396, 4372, 4372, 4116, 5140, 5140, 5140, 5140, 5140, 5140, 5140, 5396, 4372, 4372, 4116, 5140, 5396, 4372, 4372, 4116, 5140]
log_start = 54

def process_flags(num):
    flags = {
        "JD_SERIAL_RECEIVING": 0x0002,
        "JD_SERIAL_RECEIVING_HEADER": 0x0004,
        "JD_SERIAL_TRANSMITTING": 0x0008,
        "JD_SERIAL_TX_DRAIN_ENABLE": 0x0010,

        "JD_SERIAL_BUS_LO_ERROR": 0x0020,
        "JD_SERIAL_BUS_TIMEOUT_ERROR": 0x0040,
        "JD_SERIAL_BUS_UART_ERROR": 0x0080,
        "JD_SERIAL_ERR_MSK": 0x00E0,

        "JD_SERIAL_BUS_STATE": 0x0100,
        "JD_SERIAL_BUS_TOGGLED": 0x0200,
        "JD_SERIAL_LO_PULSE_START": 0x0400,

        "DEVICE_COMPONENT_RUNNING": 0x1000
    }

    flag_str = str(num) + " "
    for flag in flags.keys():
        if flags[flag] & num:
            flag_str += flag + " "

    print(flag_str)

iterator = log_start
log_end = iterator - 1

while iterator != log_end:
    process_flags(log[iterator])
    iterator = (iterator + 1) % PHYS_STATE_SIZE




