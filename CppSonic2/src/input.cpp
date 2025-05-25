#include "input.h"

#include "window_creation.h"

Input input;

void Input::init() {}

void Input::deinit() {}

bool Input::handle_event(const SDL_Event& ev) {
	switch (ev.type) {
		case SDL_KEYDOWN: {
			SDL_Scancode scancode = ev.key.keysym.scancode;

			if (scancode == 0) break;

			u32 mask = 1;
			for (auto& it : g_InputBindings) {
				if (it.scancode0 == scancode || it.scancode1 == scancode) {
					if (ev.key.repeat) {
						state_repeat |= mask;
					} else {
						state_press |= mask;
						state |= mask;
					}
				}

				mask <<= 1;
			}
			break;
		}

		case SDL_KEYUP: {
			SDL_Scancode scancode = ev.key.keysym.scancode;

			if (scancode == 0) break;

			u32 mask = 1;
			for (auto& it : g_InputBindings) {
				if (it.scancode0 == scancode || it.scancode1 == scancode) {
					state_release |= mask;
					state &= ~mask;
				}

				mask <<= 1;
			}
			break;
		}
	}

	return false;
}

void Input::update(float delta) {
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
}

void Input::clear() {
	// `state` is not cleared
	state_press = 0;
	state_release = 0;
	state_repeat = 0;
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
