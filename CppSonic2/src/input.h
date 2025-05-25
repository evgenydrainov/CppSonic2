#pragma once

#include "common.h"

struct Input {
	u32 state;
	u32 state_press;
	u32 state_release;
	u32 state_repeat;

	void init();
	void deinit();

	bool handle_event(const SDL_Event& ev);
	void update(float delta);
	void clear();
};

struct InputBinding {
	SDL_Scancode scancode0;
	SDL_Scancode scancode1;
	SDL_GameControllerButton controller_button0;
	SDL_GameControllerButton controller_button1;
};

extern Input input;

#include "input_bindings.h"

bool is_input_held(InputKey key);
bool is_input_pressed(InputKey key, bool repeat = false);
bool is_input_released(InputKey key);
