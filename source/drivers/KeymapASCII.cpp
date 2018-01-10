#include "Keymap.h"
#include "USB_HID_Keys.h"

//define all key sequences to be used
static const key seq_backspace[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_BACKSPACE },
};

static const key seq_tab[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_TAB },
};

static const key seq_newline[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_ENTER },
};

static const key seq_space[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_SPACE },
};

static const key seq_exclamation_point[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_1 },
};

static const key seq_quote[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_1 },
};

static const key seq_pound[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_3 },
};

static const key seq_dollar[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_4 },
};

static const key seq_percent[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_5 },
};

static const key seq_amp[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_7 },
};

static const key seq_apostrophe[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_APOSTROPHE },
};

static const key seq_left_paren[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_9 },
};

static const key seq_right_paren[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_0 },
};

static const key seq_ast[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_8 },
};

static const key seq_plus[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_EQUAL },
};

static const key seq_comma[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_COMMA },
};

static const key seq_minus[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_MINUS },
};

static const key seq_dot[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_DOT },
};

static const key seq_slash[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_SLASH },
};

static const key seq_0[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_0 },
};

static const key seq_1[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_1 },
};

static const key seq_2[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_2 },
};

static const key seq_3[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_3 },
};

static const key seq_4[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_4 },
};

static const key seq_5[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_5 },
};

static const key seq_6[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_6 },
};

static const key seq_7[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_7 },
};

static const key seq_8[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_8 },
};

static const key seq_9[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_9 },
};

static const key seq_colon[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_SEMICOLON },
};

static const key seq_semicolon[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_SEMICOLON },
};

static const key seq_angle_left[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_COMMA },
};

static const key seq_equal[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_EQUAL },
};

static const key seq_angle_right[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_DOT },
};

static const key seq_question[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_SLASH },
};

static const key seq_at[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_2 },
};

static const key seq_A[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_A },
};

static const key seq_B[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_B },
};

static const key seq_C[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_C },
};

static const key seq_D[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_D },
};

static const key seq_E[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_E },
};

static const key seq_F[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_F },
};

static const key seq_G[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_G },
};

static const key seq_H[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_H },
};

static const key seq_I[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_I },
};

static const key seq_J[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_J },
};

static const key seq_K[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_K },
};

static const key seq_L[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_L },
};

static const key seq_M[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_M },
};

static const key seq_N[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_N },
};

static const key seq_O[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_O },
};

static const key seq_P[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_P },
};

static const key seq_Q[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_Q },
};

static const key seq_R[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_R },
};

static const key seq_S[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_S },
};

static const key seq_T[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_T },
};

static const key seq_U[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_U },
};

static const key seq_V[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_V },
};

static const key seq_W[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_W },
};

static const key seq_X[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_X },
};

static const key seq_Y[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_Y },
};

static const key seq_Z[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_Z },
};

static const key seq_brace_left[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_LEFTBRACE },
};

static const key seq_backslash[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_BACKSLASH },
};

static const key seq_brace_right[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_RIGHTBRACE },
};

static const key seq_hat[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_6 },
};

static const key seq_underscore[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_MINUS },
};

static const key seq_grave[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_GRAVE },
};

static const key seq_a[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_A },
};

static const key seq_b[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_B },
};

static const key seq_c[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_C },
};

static const key seq_d[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_D },
};

static const key seq_e[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_E },
};

static const key seq_f[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_F },
};

static const key seq_g[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_G },
};

static const key seq_h[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_H },
};

static const key seq_i[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_I },
};

static const key seq_j[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_J },
};

static const key seq_k[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_K },
};

static const key seq_l[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_L },
};

static const key seq_m[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_M },
};

static const key seq_n[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_N },
};

static const key seq_o[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_O },
};

static const key seq_p[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_P },
};

static const key seq_q[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_Q },
};

static const key seq_r[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_R },
};

static const key seq_s[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_S },
};

static const key seq_t[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_T },
};

static const key seq_u[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_U },
};

static const key seq_v[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_V },
};

static const key seq_w[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_W },
};

static const key seq_x[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_X },
};

static const key seq_y[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_Y },
};

static const key seq_z[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_Z },
};

static const key seq_curly_left[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_LEFTBRACE },
};

static const key seq_pipe[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_BACKSLASH },
};

static const key seq_curly_right[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_RIGHTBRACE},
};

static const key seq_tilda[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEYMAP_MODIFIER_KEY | KEY_MOD_LSHIFT }, 
		{ .reg = KEYMAP_KEY_DOWN | KEY_GRAVE },
};

static const key seq_del[] = { 
		{ .reg = KEYMAP_KEY_DOWN | KEY_BACKSPACE },
};

//define the keymap
const keySequence keymap_ascii[] = {
	{}, {}, {}, {}, {}, {}, {}, {}, //0 - 7
	KEYMAP_REGISTER(seq_backspace), 
	KEYMAP_REGISTER(seq_tab), 
	KEYMAP_REGISTER(seq_newline), 
	{}, {}, {}, {}, {},
	{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, //16 - 31
	KEYMAP_REGISTER(seq_space),								//32 space
	KEYMAP_REGISTER(seq_exclamation_point),					//33 !
	KEYMAP_REGISTER(seq_quote),								//34 "
	KEYMAP_REGISTER(seq_pound),								//35 #
	KEYMAP_REGISTER(seq_dollar),
	KEYMAP_REGISTER(seq_percent),
	KEYMAP_REGISTER(seq_amp),
	KEYMAP_REGISTER(seq_apostrophe),
	KEYMAP_REGISTER(seq_left_paren),
	KEYMAP_REGISTER(seq_right_paren),
	KEYMAP_REGISTER(seq_ast),
	KEYMAP_REGISTER(seq_plus),
	KEYMAP_REGISTER(seq_comma),
	KEYMAP_REGISTER(seq_minus),
	KEYMAP_REGISTER(seq_dot),
	KEYMAP_REGISTER(seq_slash),
	KEYMAP_REGISTER(seq_0),
	KEYMAP_REGISTER(seq_1),
	KEYMAP_REGISTER(seq_2),
	KEYMAP_REGISTER(seq_3),
	KEYMAP_REGISTER(seq_4),
	KEYMAP_REGISTER(seq_5),
	KEYMAP_REGISTER(seq_6),
	KEYMAP_REGISTER(seq_7),
	KEYMAP_REGISTER(seq_8),
	KEYMAP_REGISTER(seq_9),
	KEYMAP_REGISTER(seq_colon),
	KEYMAP_REGISTER(seq_semicolon),
	KEYMAP_REGISTER(seq_angle_left),
	KEYMAP_REGISTER(seq_equal),
	KEYMAP_REGISTER(seq_angle_right),
	KEYMAP_REGISTER(seq_question),
	KEYMAP_REGISTER(seq_at),
	KEYMAP_REGISTER(seq_A),
	KEYMAP_REGISTER(seq_B),
	KEYMAP_REGISTER(seq_C),
	KEYMAP_REGISTER(seq_D),
	KEYMAP_REGISTER(seq_E),
	KEYMAP_REGISTER(seq_F),
	KEYMAP_REGISTER(seq_G),
	KEYMAP_REGISTER(seq_H),
	KEYMAP_REGISTER(seq_I),
	KEYMAP_REGISTER(seq_J),
	KEYMAP_REGISTER(seq_K),
	KEYMAP_REGISTER(seq_L),
	KEYMAP_REGISTER(seq_M),
	KEYMAP_REGISTER(seq_N),
	KEYMAP_REGISTER(seq_O),
	KEYMAP_REGISTER(seq_P),
	KEYMAP_REGISTER(seq_Q),
	KEYMAP_REGISTER(seq_R),
	KEYMAP_REGISTER(seq_S),
	KEYMAP_REGISTER(seq_T),
	KEYMAP_REGISTER(seq_U),
	KEYMAP_REGISTER(seq_V),
	KEYMAP_REGISTER(seq_W),
	KEYMAP_REGISTER(seq_X),
	KEYMAP_REGISTER(seq_Y),
	KEYMAP_REGISTER(seq_Z),
	KEYMAP_REGISTER(seq_brace_left),
	KEYMAP_REGISTER(seq_backslash),
	KEYMAP_REGISTER(seq_brace_right),
	KEYMAP_REGISTER(seq_hat),
	KEYMAP_REGISTER(seq_underscore),
	KEYMAP_REGISTER(seq_grave),
	KEYMAP_REGISTER(seq_a),
	KEYMAP_REGISTER(seq_b),
	KEYMAP_REGISTER(seq_c),
	KEYMAP_REGISTER(seq_d),
	KEYMAP_REGISTER(seq_e),
	KEYMAP_REGISTER(seq_f),
	KEYMAP_REGISTER(seq_g),
	KEYMAP_REGISTER(seq_h),
	KEYMAP_REGISTER(seq_i),
	KEYMAP_REGISTER(seq_j),
	KEYMAP_REGISTER(seq_k),
	KEYMAP_REGISTER(seq_l),
	KEYMAP_REGISTER(seq_m),
	KEYMAP_REGISTER(seq_n),
	KEYMAP_REGISTER(seq_o),
	KEYMAP_REGISTER(seq_p),
	KEYMAP_REGISTER(seq_q),
	KEYMAP_REGISTER(seq_r),
	KEYMAP_REGISTER(seq_s),
	KEYMAP_REGISTER(seq_t),
	KEYMAP_REGISTER(seq_u),
	KEYMAP_REGISTER(seq_v),
	KEYMAP_REGISTER(seq_w),
	KEYMAP_REGISTER(seq_x),
	KEYMAP_REGISTER(seq_y),
	KEYMAP_REGISTER(seq_z),
	KEYMAP_REGISTER(seq_curly_left),
	KEYMAP_REGISTER(seq_pipe),
	KEYMAP_REGISTER(seq_curly_right),
	KEYMAP_REGISTER(seq_tilda),
	KEYMAP_REGISTER(seq_del),
};