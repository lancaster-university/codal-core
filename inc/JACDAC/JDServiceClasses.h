#ifndef JD_CLASSES_H
#define JD_CLASSES_H

#define STATIC_CLASS_START                              0
#define STATIC_CLASS_END                                0x00FFFFFF
#define DYNAMIC_CLASS_START                             STATIC_ADDRESS_END
#define DYNAMIC_CLASS_END                               0xFFFFFFFF

#define JD_SERVICE_CLASS_CODAL_START                     0
#define JD_SERVICE_CLASS_CODAL_END                       2000
#define JD_SERVICE_CLASS_MAKECODE_START                  2000
#define JD_SERVICE_CLASS_MAKECODE_END                    4000

#define JD_SERVICE_CLASS_CONTROL                         0
#define JD_SERVICE_CLASS_CONTROL_RNG                     1
#define JD_SERVICE_CLASS_CONTROL_NAMING                  2

#define JD_SERVICE_CLASS_JOYSTICK                        3
#define JD_SERVICE_CLASS_MESSAGE_BUS                     4
#define JD_SERVICE_CLASS_BRIDGE                          5
#define JD_SERVICE_CLASS_BUTTON                          6
#define JD_SERVICE_CLASS_PIN                             7
#define JD_SERVICE_CLASS_RELIABILITY_TESTER              8
#define JD_SERVICE_CLASS_ACCELEROMETER                   9
#define JD_SERVICE_CLASS_CAPTOUCH_BUTTON                 10

#endif