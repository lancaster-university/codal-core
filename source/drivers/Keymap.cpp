#include "Keymap.h"
#include "USB_HID_Keys.h"

static const key seq_space[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_SPACE }, 
		{ .reg = KEYMAP_KEY_UP | KEY_SPACE } 
};

static const key seq_exclamation_point[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_1 },
		{ .reg = KEYMAP_KEY_UP | KEY_1 },
		{ .reg = KEYMAP_KEY_UP | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }
};

static const key seq_quote[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_1 },
		{ .reg = KEYMAP_KEY_UP | KEY_1 },
		{ .reg = KEYMAP_KEY_UP | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }
};

const keySequence US_ASCII[] = {
	{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, //0 - 15
	{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, //16 - 31
	{ .seq = seq_space, .length = 2 },
	{ .seq = seq_exclamation_point, .length = 4 },
	{ .seq = seq_quote, .length = 4 },
};