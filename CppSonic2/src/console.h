#pragma once

#if defined(DEVELOPER) && !defined(EDITOR)

#include "common.h"
#include "font.h"

typedef bool (*ConsoleCallbackFn)(string str, void* userdata);

struct Console {
	bool show;
	float scroll;
	bump_array<char> cmd;
	bump_array<char> history;

	ConsoleCallbackFn callback;
	void* callback_userdata;

	Font font;
	vec4 bg_color = get_color(0, 0, 0, 128);

	void init(ConsoleCallbackFn _callback, void* _callback_userdata, Font _font);
	void deinit();

	void handle_event(const SDL_Event& ev);

	void execute();

	void write(char ch);
	void write(string str);

	void update(float delta);
	void draw(float delta);
};

extern Console console;

#endif
