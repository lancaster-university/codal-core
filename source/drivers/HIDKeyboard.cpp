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

#include "HIDKeyboard.h"
#include "HID.h"

#if CONFIG_ENABLED(DEVICE_USB)

using namespace codal;

#define HID_KEYBOARD_KEY_OFF 0x00

//this descriptor must be stored in RAM
static char hidKeyboardDescriptor[] = {
    0x05, 0x01,                         // Usage Page (Generic Desktop)
    0x09, 0x06,                         // Usage (Keyboard)
    0xA1, 0x01,                         // Collection (Application)
    0x85, HID_KEYBOARD_REPORT_GENERIC,  //   Report ID (1)
    0x05, 0x07,                         //     Usage Page (Key Codes)
    0x19, 0xe0,                         //     Usage Minimum (224)
    0x29, 0xe7,                         //     Usage Maximum (231)
    0x15, 0x00,                         //     Logical Minimum (0)
    0x25, 0x01,                         //     Logical Maximum (1)
    0x75, 0x01,                         //     Report Size (1)
    0x95, 0x08,                         //     Report Count (8)
    0x81, 0x02,                         //     Input (Data, Variable, Absolute)

    0x95, 0x01,                         //     Report Count (1)
    0x75, 0x08,                         //     Report Size (8)
    0x81, 0x01,                         //     Input (Constant) reserved byte(1)

    0x95, 0x05,                         //     Report Count (5)
    0x75, 0x01,                         //     Report Size (1)
    0x05, 0x08,                         //     Usage Page (Page# for LEDs)
    0x19, 0x01,                         //     Usage Minimum (1)
    0x29, 0x05,                         //     Usage Maximum (5)
    0x91, 0x02,                         //     Output (Data, Variable, Absolute), Led report
    0x95, 0x01,                         //     Report Count (1)
    0x75, 0x03,                         //     Report Size (3)
    0x91, 0x01,                         //     Output (Data, Variable, Absolute), Led report padding

    0x95, 0x06,                         //     Report Count (6)
    0x75, 0x08,                         //     Report Size (8)
    0x15, 0x00,                         //     Logical Minimum (0)
    0x25, 0x65,                         //     Logical Maximum (101)
    0x05, 0x07,                         //     Usage Page (Key codes)
    0x19, 0x00,                         //     Usage Minimum (0)
    0x29, 0x65,                         //     Usage Maximum (101)
    0x81, 0x00,                         //     Input (Data, Array) Key array(6 bytes)


    0x09, 0x05,                         //     Usage (Vendor Defined)
    0x15, 0x00,                         //     Logical Minimum (0)
    0x26, 0xFF, 0x00,                   //     Logical Maximum (255)
    0x75, 0x08,                         //     Report Count (2)
    0x95, 0x02,                         //     Report Size (8 bit)
    0xB1, 0x02,                         //     Feature (Data, Variable, Absolute)

    0xC0,                                // End Collection (Application)

    0x05, 0x0c,                         // Usage Page (Consumer Devices)
    0x09, 0x01,                         // Usage (Consumer Control)
    0xa1, 0x01,                         // Collection (Application)
    0x85, HID_KEYBOARD_REPORT_CONSUMER, // Report ID (2)
    0x15, 0x00,                         // Logical Minimum (0)
    0x25, 0x01,                         // Logical Maximum (1)
    0x75, 0x01,                         // Report Size (1)
    0x95, 0x10,                         // Report Count (16)
    0x09, 0xe2,                         // Usage (Mute) 0x01
    0x09, 0xe9,                         // Usage (Volume Up) 0x02
    0x09, 0xea,                         // Usage (Volume Down) 0x03
    0x09, 0xcd,                         // Usage (Play/Pause) 0x04
    0x09, 0xb7,                         // Usage (Stop) 0x05
    0x09, 0xb6,                         // Usage (Scan Previous Track) 0x06
    0x09, 0xb5,                         // Usage (Scan Next Track) 0x07
    0x0a, 0x8a, 0x01,                   // Usage (Mail) 0x08
    0x0a, 0x92, 0x01,                   // Usage (Calculator) 0x09
    0x0a, 0x21, 0x02,                   // Usage (www search) 0x0a
    0x0a, 0x23, 0x02,                   // Usage (www home) 0x0b
    0x0a, 0x2a, 0x02,                   // Usage (www favorites) 0x0c
    0x0a, 0x27, 0x02,                   // Usage (www refresh) 0x0d
    0x0a, 0x26, 0x02,                   // Usage (www stop) 0x0e
    0x0a, 0x25, 0x02,                   // Usage (www forward) 0x0f
    0x0a, 0x24, 0x02,                   // Usage (www back) 0x10
    0x81, 0x62,                         // Input (Data,Var,Abs,NPrf,Null)
    0xc0,                               // End Collection

};

static const HIDReportDescriptor reportDesc = {
    9,
    0x21,                  // HID
    0x101,                 // hidbcd 1.01
    0x00,                  // country code
    0x01,                  // num desc
    0x22,                  // report desc type
    sizeof(hidKeyboardDescriptor),
};

static const InterfaceInfo ifaceInfo = {
    &reportDesc,
    sizeof(reportDesc),
    1,
    {
        1,    // numEndpoints
        0x03, /// class code - HID
        0x01, // subclass (boot interface)
        0x01, // protocol (keyboard)
        0x00, //
        0x01, //
    },
    {USB_EP_TYPE_INTERRUPT, 1},
    {USB_EP_TYPE_INTERRUPT, 1},
};

USBHIDKeyboard::USBHIDKeyboard(const keySequence *m, uint16_t mapLen, void (*delayfn)(int)) : USBHID()
{
    for(int i=0; i<HID_KEYBOARD_KEYSTATE_SIZE_GENERIC; i++)
        keyStateGeneric[i] = HID_KEYBOARD_KEY_OFF;
    
    for(int i=0; i<HID_KEYBOARD_KEYSTATE_SIZE_CONSUMER; i++)
        keyStateConsumer[i] = HID_KEYBOARD_KEY_OFF;

    _map = m;
    _mapLen = mapLen;
    _delay = delayfn;
    keyPressedCountGeneric = 0;
    keyPressedCountConsumer = 0;
}

int USBHIDKeyboard::stdRequest(UsbEndpointIn &ctrl, USBSetup &setup)
{
    if (setup.bRequest == GET_DESCRIPTOR)
    {
        if (setup.wValueH == 0x21)
        {
            InterfaceDescriptor tmp;
            fillInterfaceInfo(&tmp);
            return ctrl.write(&tmp, sizeof(tmp));
        }
        else if (setup.wValueH == 0x22)
        {
            return ctrl.write(hidKeyboardDescriptor, sizeof(hidKeyboardDescriptor));
        }
    }
    return DEVICE_NOT_SUPPORTED;
}

const InterfaceInfo *USBHIDKeyboard::getInterfaceInfo()
{
    return &ifaceInfo;
}

int USBHIDKeyboard::keyDown(uint8_t key, uint8_t reportID)
{
    int status, newIndex;

    switch(reportID){
        case HID_KEYBOARD_REPORT_GENERIC:{
            if(keyPressedCountGeneric == HID_KEYBOARD_KEYSTATE_SIZE_GENERIC)
                return DEVICE_NO_RESOURCES;

            for(int i=HID_KEYBOARD_MODIFIER_OFFSET; i<HID_KEYBOARD_KEYSTATE_SIZE_GENERIC; i++){
                if(keyStateGeneric[i] == HID_KEYBOARD_KEY_OFF){
                    keyStateGeneric[i] = key;
                    newIndex = i;
                    break;
                }
            }

            uint8_t report[HID_KEYBOARD_KEYSTATE_SIZE_GENERIC + 1] = {reportID};
            memcpy(report + 1, keyStateGeneric, HID_KEYBOARD_KEYSTATE_SIZE_GENERIC);
            status = in->write(report, sizeof(report));

            if(status == DEVICE_OK)
                keyPressedCountGeneric++;
            else
                keyStateGeneric[newIndex] = HID_KEYBOARD_KEY_OFF; //we could not make the change

            break;
        }
        case HID_KEYBOARD_REPORT_CONSUMER:{
            if(keyPressedCountConsumer == HID_KEYBOARD_KEYSTATE_SIZE_CONSUMER)
                return DEVICE_NO_RESOURCES;

            for(int i=HID_KEYBOARD_MODIFIER_OFFSET; i<HID_KEYBOARD_KEYSTATE_SIZE_CONSUMER; i++){
                if(keyStateConsumer[i] == HID_KEYBOARD_KEY_OFF){
                    keyStateConsumer[i] = key;
                    newIndex = i;
                    break;
                }
            }

            uint8_t report[HID_KEYBOARD_KEYSTATE_SIZE_CONSUMER + 1] = {reportID};
            memcpy(report + 1, keyStateConsumer, HID_KEYBOARD_KEYSTATE_SIZE_CONSUMER);
            status = in->write(report, sizeof(report));

            if(status == DEVICE_OK)
                keyPressedCountConsumer++;
            else
                keyStateConsumer[newIndex] = HID_KEYBOARD_KEY_OFF; //we could not make the change

            break;

        }
        default:
            //unknown report
            status = DEVICE_INVALID_PARAMETER;
    }

    return status;
}

int USBHIDKeyboard::keyUp(uint8_t key, uint8_t reportID)
{
    int status;
    int newIndex = -1;

    switch(reportID){
        case HID_KEYBOARD_REPORT_GENERIC: {

            // ?? hmm no keys are pressed ??
            if(keyPressedCountGeneric == 0)
                return DEVICE_INVALID_PARAMETER;

            // try to find the passed key
            for(int i=HID_KEYBOARD_MODIFIER_OFFSET; i<HID_KEYBOARD_KEYSTATE_SIZE_GENERIC; i++){
                if(keyStateGeneric[i] == key){
                    keyStateGeneric[i] = HID_KEYBOARD_KEY_OFF;
                    newIndex = i;
                    break;
                }
            }

            // the passed key is not pressed
            if(newIndex == -1)
                return DEVICE_INVALID_PARAMETER;

            //write to the host
            uint8_t report[HID_KEYBOARD_KEYSTATE_SIZE_GENERIC + 1] = {reportID};
            memcpy(report + 1, keyStateGeneric, HID_KEYBOARD_KEYSTATE_SIZE_GENERIC);
            status = in->write(report, sizeof(report));

            if(status == DEVICE_OK)
                keyPressedCountGeneric--;
            else
                keyStateGeneric[newIndex] = key; //we could not make the change. Revert
            break;
        }
        case HID_KEYBOARD_REPORT_CONSUMER: {

            // ?? hmm no keys are pressed ??
            if(keyPressedCountConsumer == 0)
                return DEVICE_INVALID_PARAMETER;

            // try to find the passed key
            for(int i=HID_KEYBOARD_MODIFIER_OFFSET; i<HID_KEYBOARD_KEYSTATE_SIZE_CONSUMER; i++){
                if(keyStateConsumer[i] == key){
                    keyStateConsumer[i] = HID_KEYBOARD_KEY_OFF;
                    newIndex = i;
                    break;
                }
            }

            // the passed key is not pressed
            if(newIndex == -1)
                return DEVICE_INVALID_PARAMETER;

            //write to the host
            uint8_t report[HID_KEYBOARD_KEYSTATE_SIZE_CONSUMER + 1] = {reportID};
            memcpy(report + 1, keyStateConsumer, HID_KEYBOARD_KEYSTATE_SIZE_CONSUMER);
            status = in->write(report, sizeof(report));

            if(status == DEVICE_OK)
                keyPressedCountConsumer--;
            else
                keyStateConsumer[newIndex] = key; //we could not make the change. Revert
            break;
        }
        default:
            //unknown report
            status = DEVICE_INVALID_PARAMETER;

    }

    return status;
}

int USBHIDKeyboard::modifierKeyDown(uint8_t key, uint8_t reportID)
{
    int status;
    uint8_t currentModifier;

    switch(reportID){
        case HID_KEYBOARD_REPORT_GENERIC:{
            currentModifier = *keyStateGeneric;

            if(currentModifier & key) 
                return DEVICE_INVALID_PARAMETER; // the passed modifier flag is already set

            *keyStateGeneric = currentModifier | key;

            //write to the host
            uint8_t report[HID_KEYBOARD_KEYSTATE_SIZE_GENERIC + 1] = {reportID};
            memcpy(report + 1, keyStateGeneric, HID_KEYBOARD_KEYSTATE_SIZE_GENERIC);
            status = in->write(report, sizeof(report));

            if(status != DEVICE_OK)
                *keyStateGeneric = currentModifier; //we could not make the change. Revert

            break;
        }
        case HID_KEYBOARD_REPORT_CONSUMER:{
            currentModifier = *keyStateConsumer;
            if(currentModifier & key) 
                return DEVICE_INVALID_PARAMETER; // the passed modifier flag is already set

            *keyStateConsumer = currentModifier | key;

            //write to the host
            uint8_t report[HID_KEYBOARD_KEYSTATE_SIZE_CONSUMER + 1] = {reportID};
            memcpy(report + 1, keyStateConsumer, HID_KEYBOARD_KEYSTATE_SIZE_CONSUMER);
            status = in->write(report, sizeof(report));

            if(status != DEVICE_OK)
                *keyStateConsumer = currentModifier; //we could not make the change. Revert

            break;

        }
        default:
            //unknown report
            status = DEVICE_INVALID_PARAMETER;
    }

    return status;
}

int USBHIDKeyboard::modifierKeyUp(uint8_t key, uint8_t reportID)
{
    int status;
    uint8_t currentModifier;

    switch(reportID){
        case HID_KEYBOARD_REPORT_GENERIC:{
            currentModifier = *keyStateGeneric;

            if( !(currentModifier & key) ) 
                return DEVICE_INVALID_PARAMETER; //The passed modifier key is not pressed

            //Clear the flag of the passed modifier
            *keyStateGeneric = currentModifier & ~(key);

            //write to the host
            uint8_t report[HID_KEYBOARD_KEYSTATE_SIZE_GENERIC + 1] = {reportID};
            memcpy(report + 1, keyStateGeneric, HID_KEYBOARD_KEYSTATE_SIZE_GENERIC);
            status = in->write(report, sizeof(report));

            if(status != DEVICE_OK)
                *keyStateGeneric = currentModifier; //we could not make the change. Revert

            break;
        }
        case HID_KEYBOARD_REPORT_CONSUMER:{
            currentModifier = *keyStateConsumer;
            if( !(currentModifier & key) ) 
                return DEVICE_INVALID_PARAMETER; //The passed modifier key is not pressed

            //Clear the flag of the passed modifier
            *keyStateConsumer = currentModifier & ~(key);

            //write to the host
            uint8_t report[HID_KEYBOARD_KEYSTATE_SIZE_CONSUMER + 1] = {reportID};
            memcpy(report + 1, keyStateConsumer, HID_KEYBOARD_KEYSTATE_SIZE_CONSUMER);
            status = in->write(report, sizeof(report));

            if(status != DEVICE_OK)
                *keyStateConsumer = currentModifier; //we could not make the change. Revert

            break;

        }
        default:
            //unknown report
            status = DEVICE_INVALID_PARAMETER;
    }

    return status;
}

int USBHIDKeyboard::type(const char str[], uint8_t reportID)
{
    char c = *str++;
    keySequence *seq;;
    while(c){
        if((uint8_t)c < _mapLen){
            seq = (keySequence *)&_map[(uint8_t)c];
            for(int i=0; i<seq->length; i++){
                key k = seq->seq[i];
                
                if(k.bit.isModifier){
                    if(k.bit.isKeyDown) modifierKeyDown(k.bit.code, reportID);
                    else modifierKeyUp(k.bit.code, reportID);
                }
                else{
                    if(k.bit.isKeyDown) keyDown(k.bit.code, reportID);
                    else keyUp(k.bit.code, reportID);
                }
                _delay(HID_KEYBOARD_DELAY_DEFAULT);
            }
            c = *str++;
        }
        else return DEVICE_INVALID_PARAMETER;
    }
}

#endif