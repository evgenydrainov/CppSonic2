#pragma once

#include "common.h"

enum Program_Mode {
	PROGRAM_NONE,
	PROGRAM_TITLE,
	PROGRAM_GAME,

	NUM_PROGRAM_MODES,
};

struct Program {
	static constexpr float TRANSITION_SPEED = 1.0f / 60.0f;

	int player_lives = 3;
	int player_score;

	Program_Mode program_mode;
	Program_Mode next_program_mode;
	float transition_t;

	bool show_debug_info;

	string level_filepath;

	int argc;
	char** argv;

	void init(int argc, char* argv[]);
	void deinit();

	void update(float delta);
	void draw(float delta);
	void late_draw(float delta);

	void deinit_program_mode();
	void set_program_mode(Program_Mode mode);
};

extern Program program;

#ifdef DEVELOPER
extern array<string> g_ConsoleCommands;

bool console_callback(string str, void* userdata);
#endif
