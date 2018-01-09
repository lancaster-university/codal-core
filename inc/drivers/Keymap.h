#ifndef DEVICE_KEYMAP_H
#define DEVICE_KEYMAP_H

#include <stdint.h>

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

typedef union {
    struct {
        uint8_t code:8;
        uint8_t reserved:6;
        bool isModifier:1;
        bool isKeyDown:1;
    } bit;
    uint16_t reg;
} __attribute__((packed)) key;

typedef struct {
	const key *seq;
	uint8_t length;
} keySequence;

class Keymap {
public:
	Keymap(const keySequence *seq, uint16_t length);
	int lookup(uint16_t c, keySequence *seq);

private:
	const keySequence *table;
	uint16_t tableLen;
};

extern const keySequence US_ASCII[];

#endif