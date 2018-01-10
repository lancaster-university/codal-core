#ifndef DEVICE_KEYMAP_H
#define DEVICE_KEYMAP_H

#include <stdint.h>

#define KEYMAP_ALL_KEYS_UP_Val 1
#define KEYMAP_ALL_KEYS_UP_POS 13
#define KEYMAP_ALL_KEYS_UP_MASK(x) ((uint16_t)x << KEYMAP_ALL_KEYS_UP_POS)
#define KEYMAP_ALL_KEYS_UP KEYMAP_ALL_KEYS_UP_MASK(KEYMAP_ALL_KEYS_UP_Val)

#define KEYMAP_NORMAL_KEY_Val 0
#define KEYMAP_MODIFIER_KEY_Val 1
#define KEYMAP_MODIFIER_POS 14
#define KEYMAP_MODIFIER_MASK(x) ((uint16_t)x << KEYMAP_MODIFIER_POS)
#define KEYMAP_MODIFIER_KEY KEYMAP_MODIFIER_MASK(KEYMAP_MODIFIER_KEY_Val)

#define KEYMAP_KEY_UP_Val 0
#define KEYMAP_KEY_DOWN_Val 1
#define KEYMAP_KEY_DOWN_POS 15
#define KEYMAP_KEY_DOWN_MASK(x) ((uint16_t)x << KEYMAP_KEY_DOWN_POS)
#define KEYMAP_KEY_DOWN KEYMAP_KEY_DOWN_MASK(KEYMAP_KEY_DOWN_Val)
#define KEYMAP_KEY_UP KEYMAP_KEY_DOWN_MASK(KEYMAP_KEY_UP_Val)

#define KEYMAP_REGISTER(x) { .seq = x, .length = sizeof(x)/sizeof(key) }

typedef union {
    struct {
        uint8_t code:8;
        uint8_t reserved:5;
        bool allKeysUp:1;
        bool isModifier:1;
        bool isKeyDown:1;
    } bit;
    uint16_t reg;
} __attribute__((packed)) key;

typedef struct {
	const key *seq;
	uint8_t length;
} keySequence;

#define KEYMAP_ASCII_LENGTH
extern const keySequence keymap_ascii[];

#endif