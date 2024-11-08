#include "console.h"

#if defined(DEVELOPER) && !defined(EDITOR)

#include "window_creation.h"
#include "renderer.h"

Console console;

void Console::init(ConsoleCallbackFn _callback, void* _callback_userdata, Font _font) {
	callback          = _callback;
	callback_userdata = _callback_userdata;
	font              = _font;
	
	cmd     = malloc_bump_array<char>(64);
	history = malloc_bump_array<char>(1024);
}

void Console::deinit() {
	free(history.data);
	free(cmd.data);
}

void Console::handle_event(const SDL_Event& ev) {
	switch (ev.type) {
		case SDL_KEYDOWN: {
			SDL_Scancode scancode = ev.key.keysym.scancode;

			if (scancode == SDL_SCANCODE_GRAVE) {
				show ^= true;
				window.disable_input = show;

				if (show) {
					cmd.count = 0;
					scroll = 0;
					// history_index = -1;
					// caret = 0;
				}
				break;
			}

			if (!show) break;

			if (scancode == SDL_SCANCODE_BACKSPACE) {
				if (cmd.count > 0) {
					cmd.count--;
				}
				break;
			}

			if (scancode == SDL_SCANCODE_RETURN) {
				write(cmd);
				write('\n');

				execute();

				cmd.count = 0;
				// history_index = -1;
				// caret = 0;
				break;
			}
			break;
		}

		case SDL_TEXTINPUT: {
			char ch = ev.text.text[0];

			if (!show) break;

			if (ch == '`') break;

			if ((u8)ch >= 32 && (u8)ch <= 127) {
				if (cmd.count < cmd.capacity) {
					array_add(&cmd, ch);
				}
			}
			break;
		}
	}
}

void Console::execute() {
	if (!callback) {
		write("[ERROR] Console callback is null!\n");
		return;
	}

	bool result = callback(cmd, callback_userdata);
	if (!result) {
		write("Invalid command.\n");
	}
}

void Console::write(char ch) {
	if (history.count == history.capacity) {
		array_remove(&history, history.begin());
	}
	array_add(&history, ch);
}

void Console::write(string str) {
	for (size_t i = 0; i < str.count; i++) {
		write(str[i]);
	}
}

void Console::update(float delta) {
	if (!show) return;
}

void Console::draw(float delta) {
	if (!show) return;

	int backbuffer_w;
	int backbuffer_h;
	SDL_GL_GetDrawableSize(get_window_handle(), &backbuffer_w, &backbuffer_h);

	float console_w = backbuffer_w;
	float console_h = backbuffer_h * 0.40f;

	draw_rectangle(Rectf{0, 0, console_w, console_h}, bg_color);

	vec2 pos = vec2(0, console_h + scroll);

	break_batch();
	glScissor(0, backbuffer_h - console_h, console_w, console_h);
	glEnable(GL_SCISSOR_TEST);

	// draw history
	draw_text(font, history, pos, HALIGN_LEFT, VALIGN_BOTTOM);

	// draw shell thing
	pos.x = draw_text(font, ">", pos, HALIGN_LEFT, VALIGN_BOTTOM).x;

	// draw cmd
	draw_text(font, cmd, pos, HALIGN_LEFT, VALIGN_BOTTOM);

	/*/ draw caret
	{
		vec4 color;
		if ((SDL_GetTicks() % 800) > 400) {
			color = {1, 1, 1, 1.00f};
		} else {
			color = {1, 1, 1, 0.50f};
		}

		string str;
		str.data  = user_input_line.data;
		str.count = caret;

		vec2 text_size = r->measure_text(font, str);

		r->draw_rectangle({(int)(x + text_size.x), (int)(y - text_size.y), 2, 16}, color);
	}*/

	break_batch();
	glDisable(GL_SCISSOR_TEST);
}

#endif
