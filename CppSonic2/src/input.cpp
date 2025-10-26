#include "input.h"

Input input;

void Input::init() {}

void Input::deinit() {
	if (controller) SDL_GameControllerClose(controller);
	controller = nullptr;
}

bool Input::handle_event(const SDL_Event& ev) {
	switch (ev.type) {
		case SDL_KEYDOWN: {
			SDL_Scancode scancode = ev.key.keysym.scancode;

			u32 mask = 1;
			for (const auto& it : g_InputBindings) {
				if (it.scancode0 == scancode || it.scancode1 == scancode) {
					if (ev.key.repeat) {
						state_repeat |= mask;
					} else {
						state_press |= mask;
						state       |= mask;
					}
				}

				mask <<= 1;
			}

			if (scancode >= 0 && scancode < NUM_KEYBOARD_KEYS) {
				if (ev.key.repeat) {
					keyboard_state_repeat[scancode / 32] |= 1 << (scancode % 32);
				} else {
					keyboard_state_press[scancode / 32] |= 1 << (scancode % 32);
					keyboard_state      [scancode / 32] |= 1 << (scancode % 32);
				}
			}
			break;
		}

		case SDL_KEYUP: {
			SDL_Scancode scancode = ev.key.keysym.scancode;

			u32 mask = 1;
			for (const auto& it : g_InputBindings) {
				if (it.scancode0 == scancode || it.scancode1 == scancode) {
					state_release |=  mask;
					state         &= ~mask;
				}

				mask <<= 1;
			}

			if (scancode >= 0 && scancode < NUM_KEYBOARD_KEYS) {
				keyboard_state_release[scancode / 32] |=   1 << (scancode % 32);
				keyboard_state        [scancode / 32] &= ~(1 << (scancode % 32));
			}
			break;
		}

		case SDL_CONTROLLERDEVICEADDED: {
			if (!controller) {
				controller = SDL_GameControllerOpen(ev.cdevice.which);

				log_info("Opened controller %s.", SDL_GameControllerName(controller));
			}
			break;
		}

		case SDL_CONTROLLERDEVICEREMOVED: {
			if (controller) {
				SDL_Joystick* joystick = SDL_GameControllerGetJoystick(controller);
				if (SDL_JoystickInstanceID(joystick) == ev.cdevice.which) {
					log_info("Closing controller %s...", SDL_GameControllerName(controller));

					SDL_GameControllerClose(controller);
					controller = nullptr;
				}
			}
			break;
		}

		case SDL_CONTROLLERBUTTONDOWN: {
			if (controller) {
				SDL_Joystick* joystick = SDL_GameControllerGetJoystick(controller);
				if (SDL_JoystickInstanceID(joystick) == ev.cbutton.which) {
					SDL_GameControllerButton button = (SDL_GameControllerButton) ev.cbutton.button;

					u32 mask = 1;
					for (const auto& it : g_InputBindings) {
						if (it.controller_button0 == button || it.controller_button1 == button) {
							state_press |= mask;
							state       |= mask;
						}

						mask <<= 1;
					}

					if (button >= 0 && button < NUM_CONTROLLER_BUTTONS) {
						controller_state_press[button / 32] |= 1 << (button % 32);
						controller_state      [button / 32] |= 1 << (button % 32);
					}
				}
			}
			break;
		}

		case SDL_CONTROLLERBUTTONUP: {
			if (controller) {
				SDL_Joystick* joystick = SDL_GameControllerGetJoystick(controller);
				if (SDL_JoystickInstanceID(joystick) == ev.cbutton.which) {
					SDL_GameControllerButton button = (SDL_GameControllerButton) ev.cbutton.button;

					u32 mask = 1;
					for (const auto& it : g_InputBindings) {
						if (it.controller_button0 == button || it.controller_button1 == button) {
							state_release |=  mask;
							state         &= ~mask;
						}

						mask <<= 1;
					}

					if (button >= 0 && button < NUM_CONTROLLER_BUTTONS) {
						controller_state_release[button / 32] |=   1 << (button % 32);
						controller_state        [button / 32] &= ~(1 << (button % 32));
					}
				}
			}
			break;
		}
	}

	// don't eat events?
	return false;
}

void Input::update(float delta, Rect game_texture_rect, int game_width, int game_height) {
#if 0
	// Controller Axis
	{
		const float deadzone = 0.3f;

		float leftx = controller_get_axis(SDL_CONTROLLER_AXIS_LEFTX);
		if (leftx < -deadzone) {
			p->input |= INPUT_MOVE_LEFT;
		}
		if (leftx > deadzone) {
			p->input |= INPUT_MOVE_RIGHT;
		}

		float lefty = controller_get_axis(SDL_CONTROLLER_AXIS_LEFTY);
		if (lefty < -deadzone) {
			p->input |= INPUT_MOVE_UP;
		}
		if (lefty > deadzone) {
			p->input |= INPUT_MOVE_DOWN;
		}
	}
#endif

	// handle mouse
	{
		u32 prev = mouse_state;

		int mouse_x;
		int mouse_y;
		mouse_state = SDL_GetMouseState(&mouse_x, &mouse_y);

		mouse_state_press   =  mouse_state & ~prev;
		mouse_state_release = ~mouse_state &  prev;

		if (game_texture_rect.w != 0 && game_texture_rect.h != 0) {
			mouse_world_pos.x = (mouse_x - game_texture_rect.x) / (float)game_texture_rect.w * (float)game_width;
			mouse_world_pos.y = (mouse_y - game_texture_rect.y) / (float)game_texture_rect.h * (float)game_height;
		}
	}
}

void Input::clear() {
	// `state` is not cleared
	state_press = 0;
	state_release = 0;
	state_repeat = 0;

	// `keyboard_state` is not cleared
	memset(keyboard_state_press,   0, sizeof(keyboard_state_press));
	memset(keyboard_state_release, 0, sizeof(keyboard_state_release));
	memset(keyboard_state_repeat,  0, sizeof(keyboard_state_repeat));

	// `controller_state` is not cleared
	memset(controller_state_press,   0, sizeof(controller_state_press));
	memset(controller_state_release, 0, sizeof(controller_state_release));
}

bool is_input_held(InputKey key) {
	return (input.state & key) != 0;
}

bool is_input_pressed(InputKey key, bool repeat) {
	bool result = (input.state_press & key) != 0;

	if (repeat) {
		result |= (input.state_repeat & key) != 0;
	}

	return result;
}

bool is_input_released(InputKey key) {
	return (input.state_release & key) != 0;
}

bool is_key_pressed(SDL_Scancode key, bool repeat) {
	if (key < 0 || key >= input.NUM_KEYBOARD_KEYS) {
		return false;
	}

	bool result = (input.keyboard_state_press[key / 32] & (1 << (key % 32))) != 0;
	if (repeat) {
		result |= (input.keyboard_state_repeat[key / 32] & (1 << (key % 32))) != 0;
	}
	return result;
}

bool is_key_held(SDL_Scancode key) {
	if (key < 0 || key >= input.NUM_KEYBOARD_KEYS) {
		return false;
	}

	return (input.keyboard_state[key / 32] & (1 << (key % 32))) != 0;
}

bool is_key_released(SDL_Scancode key) {
	if (key < 0 || key >= input.NUM_KEYBOARD_KEYS) {
		return false;
	}

	return (input.keyboard_state_release[key / 32] & (1 << (key % 32))) != 0;
}

bool is_controller_button_held(SDL_GameControllerButton button) {
	if (button < 0 || button >= input.NUM_CONTROLLER_BUTTONS) {
		return false;
	}

	return (input.controller_state[button / 32] & (1 << (button % 32))) != 0;
}

bool is_controller_button_pressed(SDL_GameControllerButton button) {
	if (button < 0 || button >= input.NUM_CONTROLLER_BUTTONS) {
		return false;
	}

	return (input.controller_state_press[button / 32] & (1 << (button % 32))) != 0;
}

bool is_controller_button_released(SDL_GameControllerButton button) {
	if (button < 0 || button >= input.NUM_CONTROLLER_BUTTONS) {
		return false;
	}

	return (input.controller_state_release[button / 32] & (1 << (button % 32))) != 0;
}

float controller_get_axis(SDL_GameControllerAxis axis) {
	if (input.controller) {
		return SDL_GameControllerGetAxis(input.controller, axis) / 32768.0f;
	}
	return 0;
}

bool is_mouse_button_held(u32 button) {
	return (input.mouse_state & SDL_BUTTON(button)) != 0;
}

bool is_mouse_button_pressed(u32 button) {
	return (input.mouse_state_press & SDL_BUTTON(button)) != 0;
}

bool is_mouse_button_released(u32 button) {
	return (input.mouse_state_release & SDL_BUTTON(button)) != 0;
}
