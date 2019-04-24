import sys

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

        "JD_SERIAL_BUS_ACTIVE_LO": 0x0800,
        "JD_SERIAL_COMM_ACTIVE_LO": 0x0100
    }

    for flag in flags.keys():
        if flags[flag] & num:
            print(flag + "\r\n")

if len(sys.argv) == 2:
    process_flags(int(sys.argv[1]))
else:
    print("Invalid args, this script expects on number")
    exit(1)



