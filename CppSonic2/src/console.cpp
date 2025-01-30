#include "console.h"

#ifdef DEVELOPER

#include "window_creation.h"
#include "renderer.h"

Console console;

void Console::init(ConsoleCallbackFn _callback, void* _callback_userdata,
				   Font _font,
				   array<string> _commands) {
	callback          = _callback;
	callback_userdata = _callback_userdata;
	font              = _font;
	commands          = _commands;
	
	cmd     = malloc_bump_array<char>(64);
	history = malloc_bump_array<char>(1024);

	cmd_hist = malloc_bump_array<string>(CMD_HIST);
}

void Console::deinit() {
	free(cmd.data);
	free(history.data);

	For (it, cmd_hist) {
		free(it->data);
	}
	free(cmd_hist.data);
}

void Console::handle_event(const SDL_Event& ev) {
	auto handle_keydown_event = [&](const SDL_Event& ev) {
		SDL_Scancode scancode = ev.key.keysym.scancode;

		if (scancode == SDL_SCANCODE_GRAVE) {
			is_open ^= true;
			console_anim_y = is_open ? 0.0f : 1.0f;

			if (is_open) {
				cmd.count = 0;
				scroll = 0;
				caret = 0;
				history_index = -1;
			}
			return;
		}

		if (!is_open) return;

		if (scancode == SDL_SCANCODE_ESCAPE) {
			is_open = false;
			console_anim_y = is_open ? 0.0f : 1.0f;
			return;
		}

		if (scancode == SDL_SCANCODE_BACKSPACE) {
			if (caret > 0) {
				array_remove(&cmd, caret - 1);
				caret--;
			}
			return;
		}

		if (scancode == SDL_SCANCODE_DELETE) {
			if (caret < cmd.count) {
				array_remove(&cmd, caret);
			}
			return;
		}

		if (scancode == SDL_SCANCODE_RETURN) {
			// ignore alt+enter
			if (!(ev.key.keysym.mod & KMOD_ALT)) {
				write(cmd);
				write('\n');

				execute();

				if (cmd_hist.capacity == cmd_hist.count) {
					size_t index = cmd_hist.count - 1;
					free(cmd_hist[index].data);
					array_remove(&cmd_hist, index);
				}
				string str = copy_string(cmd);
				array_insert(&cmd_hist, 0, str);

				cmd.count = 0;
				caret = 0;
				history_index = -1;
			}
			return;
		}

		if (scancode == SDL_SCANCODE_LEFT) {
			caret--;
			if (caret < 0) caret = 0;
			return;
		}

		if (scancode == SDL_SCANCODE_RIGHT) {
			caret++;
			if (caret > cmd.count) caret = cmd.count;
			return;
		}

		if (scancode == SDL_SCANCODE_UP) {
			if (history_index + 1 < cmd_hist.count) {
				history_index++;

				string hist = cmd_hist[history_index];

				Assert(hist.count <= cmd.capacity);
				memcpy(cmd.data, hist.data, hist.count);
				cmd.count = hist.count;

				caret = cmd.count;
			}
			return;
		}

		if (scancode == SDL_SCANCODE_DOWN) {
			if (history_index - 1 >= 0) {
				history_index--;

				string hist = cmd_hist[history_index];

				Assert(hist.count <= cmd.capacity);
				memcpy(cmd.data, hist.data, hist.count);
				cmd.count = hist.count;

				caret = cmd.count;
			} else if (history_index == 0) {
				history_index--;

				cmd.count = 0;

				caret = cmd.count;
			}
			return;
		}

		if (scancode == SDL_SCANCODE_TAB) {
			string autocomplete = get_autocomplete(cmd);
			if (autocomplete.count > 0) {
				Assert(autocomplete.count <= cmd.capacity);
				memcpy(cmd.data, autocomplete.data, autocomplete.count);
				cmd.count = autocomplete.count;

				caret = cmd.count;
			}
			return;
		}

		if (scancode == SDL_SCANCODE_HOME) {
			caret = 0;
			return;
		}

		if (scancode == SDL_SCANCODE_END) {
			caret = cmd.count;
			return;
		}
	};

	switch (ev.type) {
		case SDL_KEYDOWN: {
			handle_keydown_event(ev);
			break;
		}

		case SDL_TEXTINPUT: {
			char ch = ev.text.text[0];

			if (!is_open) break;

			if (ch == '`') break;

			if ((u8)ch >= 32 && (u8)ch <= 127) {
				if (cmd.count < cmd.capacity) {
					array_insert(&cmd, caret, ch);
					caret++;
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

	if (cmd.count == 0) {
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

string Console::get_autocomplete(string cmd) {
	string autocomplete = {};

	if (cmd.count > 0) {
		For (it, commands) {
			if (starts_with(*it, cmd)) {
				autocomplete = *it;
				break;
			}
		}
	}

	return autocomplete;
}

void Console::update(float delta) {
	float target = is_open ? 1.0f : 0.0f;
	Lerp_delta(&console_anim_y, target, 0.4f, delta);
}

void Console::draw(float delta) {
	if (console_anim_y < 0.01f) {
		return;
	}

	int backbuffer_width;
	int backbuffer_height;
	SDL_GL_GetDrawableSize(window.handle, &backbuffer_width, &backbuffer_height);

	break_batch();
	renderer.proj_mat = glm::ortho<float>(0, backbuffer_width, backbuffer_height, 0);
	renderer.view_mat = {1};
	renderer.model_mat = {1};
	glViewport(0, 0, backbuffer_width, backbuffer_height);

	float console_w = backbuffer_width;
	float console_h = backbuffer_height * 0.40f * console_anim_y;

	draw_rectangle(Rectf{0, 0, console_w, console_h}, bg_color);

	vec2 pos = {0, console_h + scroll};

	break_batch();
	glScissor(0, backbuffer_height - console_h, console_w, console_h);
	glEnable(GL_SCISSOR_TEST);

	// draw history
	draw_text(font, history, pos, HALIGN_LEFT, VALIGN_BOTTOM);

	// draw shell thing
	pos.x = draw_text(font, ">", pos, HALIGN_LEFT, VALIGN_BOTTOM).x;

	// draw autocomplete
	{
		string autocomplete = get_autocomplete(cmd);
		draw_text(font, autocomplete, pos, HALIGN_LEFT, VALIGN_BOTTOM, {0.5f, 0.5f, 0.5f, 1});
	}

	// draw cmd
	draw_text(font, cmd, pos, HALIGN_LEFT, VALIGN_BOTTOM);

	// draw caret
	{
		vec4 color = color_white;
		color.a = 0.5f + (sinf(SDL_GetTicks() / 200.0f) + 1.0f) / 4.0f;

		string str;
		str.data  = cmd.data;
		str.count = caret;

		vec2 text_size = measure_text(font, str);

		float caret_w = 3;
		float caret_h = font.line_height;

		draw_rectangle({pos.x + text_size.x, pos.y - text_size.y, caret_w, caret_h}, color);
	}

	break_batch();
	glDisable(GL_SCISSOR_TEST);

	was_open_last_frame = is_open;
}

#endif
