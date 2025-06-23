#pragma once

#include "common.h"

// Has to match `g_InputBindings`.
enum InputKey : u32 {
	INPUT_MOVE_RIGHT    = 1 << 0,
	INPUT_MOVE_UP       = 1 << 1,
	INPUT_MOVE_LEFT     = 1 << 2,
	INPUT_MOVE_DOWN     = 1 << 3,

	INPUT_A             = 1 << 4,
	INPUT_B             = 1 << 5,

	INPUT_PAUSE         = 1 << 6,

	INPUT_UI_RIGHT      = 1 << 7,
	INPUT_UI_UP         = 1 << 8,
	INPUT_UI_LEFT       = 1 << 9,
	INPUT_UI_DOWN       = 1 << 10,

	INPUT_UI_CONFIRM    = 1 << 11,
	INPUT_UI_CANCEL     = 1 << 12,

	NUM_INPUTS          = 13,

	INPUT_JUMP          = INPUT_A | INPUT_B,
};
