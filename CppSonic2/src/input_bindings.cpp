#include "input.h"

InputBinding g_InputBindings[NUM_INPUTS] = {
	/* INPUT_MOVE_RIGHT    */ { SDL_SCANCODE_RIGHT, (SDL_Scancode)0, SDL_CONTROLLER_BUTTON_DPAD_RIGHT },
	/* INPUT_MOVE_UP       */ { SDL_SCANCODE_UP,    (SDL_Scancode)0, SDL_CONTROLLER_BUTTON_DPAD_UP },
	/* INPUT_MOVE_LEFT     */ { SDL_SCANCODE_LEFT,  (SDL_Scancode)0, SDL_CONTROLLER_BUTTON_DPAD_LEFT },
	/* INPUT_MOVE_DOWN     */ { SDL_SCANCODE_DOWN,  (SDL_Scancode)0, SDL_CONTROLLER_BUTTON_DPAD_DOWN },

	/* INPUT_A             */ { SDL_SCANCODE_Z, (SDL_Scancode)0, SDL_CONTROLLER_BUTTON_A },
	/* INPUT_B             */ { SDL_SCANCODE_X, (SDL_Scancode)0, SDL_CONTROLLER_BUTTON_B },

	/* INPUT_PAUSE         */ { SDL_SCANCODE_ESCAPE, (SDL_Scancode)0, SDL_CONTROLLER_BUTTON_START },

	/* INPUT_UI_RIGHT      */ { SDL_SCANCODE_RIGHT, (SDL_Scancode)0, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, (SDL_GameControllerButton)0 },
	/* INPUT_UI_UP         */ { SDL_SCANCODE_UP,    (SDL_Scancode)0, SDL_CONTROLLER_BUTTON_DPAD_UP,    (SDL_GameControllerButton)0 },
	/* INPUT_UI_LEFT       */ { SDL_SCANCODE_LEFT,  (SDL_Scancode)0, SDL_CONTROLLER_BUTTON_DPAD_LEFT,  (SDL_GameControllerButton)0 },
	/* INPUT_UI_DOWN       */ { SDL_SCANCODE_DOWN,  (SDL_Scancode)0, SDL_CONTROLLER_BUTTON_DPAD_DOWN,  (SDL_GameControllerButton)0 },

	/* INPUT_UI_CONFIRM    */ { SDL_SCANCODE_Z, SDL_SCANCODE_RETURN, SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_START },
	/* INPUT_UI_CANCEL     */ { SDL_SCANCODE_X, SDL_SCANCODE_ESCAPE, SDL_CONTROLLER_BUTTON_B },
};
