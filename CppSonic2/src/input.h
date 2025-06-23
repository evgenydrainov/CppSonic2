#pragma once

#include "common.h"
#include "input_bindings.h"

struct Input {
	static constexpr int NUM_KEYBOARD_KEYS = SDL_SCANCODE_RGUI + 1;
	static constexpr int NUM_CONTROLLER_BUTTONS = SDL_CONTROLLER_BUTTON_MAX;

	SDL_GameController* controller;

	u32 state;
	u32 state_press;
	u32 state_release;
	u32 state_repeat;

	u32 keyboard_state        [(NUM_KEYBOARD_KEYS + 31) / 32];
	u32 keyboard_state_press  [(NUM_KEYBOARD_KEYS + 31) / 32];
	u32 keyboard_state_release[(NUM_KEYBOARD_KEYS + 31) / 32];
	u32 keyboard_state_repeat [(NUM_KEYBOARD_KEYS + 31) / 32];

	u32 controller_state        [(NUM_CONTROLLER_BUTTONS + 31) / 32];
	u32 controller_state_press  [(NUM_CONTROLLER_BUTTONS + 31) / 32];
	u32 controller_state_release[(NUM_CONTROLLER_BUTTONS + 31) / 32];

	u32 mouse_state;
	u32 mouse_state_press;
	u32 mouse_state_release;

	vec2 mouse_world_pos;

	void init();
	void deinit();

	bool handle_event(const SDL_Event& ev);
	void update(float delta, Rect game_texture_rect, int game_width, int game_height);
	void clear();
};

struct InputBinding {
	SDL_Scancode scancode0;
	SDL_Scancode scancode1;
	SDL_GameControllerButton controller_button0;
	SDL_GameControllerButton controller_button1;
};

extern Input input;

extern InputBinding g_InputBindings[NUM_INPUTS];

bool is_input_held(InputKey key);
bool is_input_pressed(InputKey key, bool repeat = false);
bool is_input_released(InputKey key);

bool is_key_held(SDL_Scancode key);
bool is_key_pressed(SDL_Scancode key, bool repeat = false);
bool is_key_released(SDL_Scancode key);

bool is_controller_button_held(SDL_GameControllerButton button);
bool is_controller_button_pressed(SDL_GameControllerButton button);
bool is_controller_button_released(SDL_GameControllerButton button);
float controller_get_axis(SDL_GameControllerAxis axis);

bool is_mouse_button_held(u32 button);
bool is_mouse_button_pressed(u32 button);
bool is_mouse_button_released(u32 button);
