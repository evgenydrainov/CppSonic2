#pragma once

#ifdef DEVELOPER

#include "common.h"
#include "font.h"

typedef bool (*ConsoleCallbackFn)(string str, void* userdata);

struct Console {
	static constexpr size_t CMD_HIST = 20;

	bool is_open;
	bool was_open_last_frame;

	float scroll;
	bump_array<char> cmd;
	bump_array<char> history;
	int caret;

	bump_array<string> cmd_hist;
	int history_index = -1;

	array<string> commands;

	ConsoleCallbackFn callback;
	void* callback_userdata;

	Font font;
	vec4 bg_color = get_color(0, 0, 0, 128);

	float console_anim_y;

	void init(ConsoleCallbackFn _callback, void* _callback_userdata,
			  Font _font,
			  array<string> _commands);
	void deinit();

	void handle_event(const SDL_Event& ev);

	void execute();

	void write(char ch);
	void write(string str);

	string get_autocomplete(string cmd);

	void update(float delta);
	void draw(float delta);
};

extern Console console;

#endif
