#ifndef JD_CLASSES_H
#define JD_CLASSES_H

#define STATIC_CLASS_START                              0
#define STATIC_CLASS_END                                0x00FFFFFF
#define DYNAMIC_CLASS_START                             STATIC_ADDRESS_END
#define DYNAMIC_CLASS_END                               0xFFFFFFFF

#define JD_SERVICE_IDENTIFIER_CODAL_START                    0
#define JD_SERVICE_IDENTIFIER_CODAL_END                      2000
#define JD_SERVICE_IDENTIFIER_MAKECODE_START                 2000
#define JD_SERVICE_IDENTIFIER_MAKECODE_END                   4000

#define JD_SERVICE_IDENTIFIER_CONTROL                        0
#define JD_SERVICE_IDENTIFIER_CONTROL_RNG                    1
#define JD_SERVICE_IDENTIFIER_CONTROL_CONFIGURATION          2
#define JD_SERVICE_IDENTIFIER_CONTROL_TEST                   3

#define JD_SERVICE_IDENTIFIER_JOYSTICK                       4
#define JD_SERVICE_IDENTIFIER_MESSAGE_BUS                    5
#define JD_SERVICE_IDENTIFIER_BRIDGE                         6
#define JD_SERVICE_IDENTIFIER_BUTTON                         7
#define JD_SERVICE_IDENTIFIER_ACCELEROMETER                  8
#define JD_SERVICE_IDENTIFIER_CONSOLE                        9

#endif