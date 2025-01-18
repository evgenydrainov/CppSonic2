#include "program.h"

#include "window_creation.h"
#include "renderer.h"
#include "assets.h"
#include "game.h"
#include "title_screen.h"
#include "console.h"

Program program;

void Program::init(int argc, char* argv[]) {
	Program_Mode mode = PROGRAM_TITLE;

	// if the second argument is --game
	if (argc >= 2 && strcmp(argv[1], "--game") == 0) {
		// ...and there's a third arg, then start the level
		if (argc >= 3) mode = PROGRAM_GAME;
	}

	set_program_mode(mode);
	transition_t = 1; // skip the fade in and only fade out

	this->argc = argc;
	this->argv = argv;
}

void Program::deinit() {
	deinit_program_mode();
}

void Program::update(float delta) {
	// change program mode
	if (next_program_mode != PROGRAM_NONE) {
		if (transition_t == 1) {
			deinit_program_mode();

			auto mode = next_program_mode;
			next_program_mode = PROGRAM_NONE;

			program_mode = mode;
			static_assert(NUM_PROGRAM_MODES == 3);
			switch (mode) {
				case PROGRAM_TITLE: {
					title_screen = {};
					title_screen.init();
					break;
				}

				case PROGRAM_GAME: {
					game = {};
					game.init(argc, argv);
					break;
				}
			}
		}

		Approach(&transition_t, 1.0f, TRANSITION_SPEED * delta);
	} else {
		Approach(&transition_t, 0.0f, TRANSITION_SPEED * delta);
	}

	static_assert(NUM_PROGRAM_MODES == 3);

	switch (program_mode) {
		case PROGRAM_TITLE: {
			title_screen.update(delta);
			break;
		}

		case PROGRAM_GAME: {
			game.update(delta);
			break;
		}
	}
}

void Program::draw(float delta) {
	static_assert(NUM_PROGRAM_MODES == 3);

	switch (program_mode) {
		case PROGRAM_TITLE: {
			title_screen.draw(delta);
			break;
		}

		case PROGRAM_GAME: {
			game.draw(delta);
			break;
		}
	}

	break_batch();
	renderer.proj_mat = glm::ortho<float>(0, window.game_width, window.game_height, 0);
	renderer.view_mat = {1};
	renderer.model_mat = {1};
	glViewport(0, 0, window.game_width, window.game_height);

	// fill the screen with black during transition
	if (transition_t != 0) {
		Rectf rect = {0, 0, window.game_width, window.game_height};

		vec4 color = color_black;
		color.a = transition_t;

		draw_rectangle(rect, color);

		break_batch();
	}

	// draw fps
	{
		char buf[16];
		string str = Sprintf(buf, "%.0f", roundf(window.avg_fps));
		draw_text(get_font(fnt_hud), str, {window.game_width, window.game_height}, HALIGN_RIGHT, VALIGN_BOTTOM);
	}

	break_batch();
}

void Program::late_draw(float delta) {
	switch (program_mode) {
		case PROGRAM_GAME: {
			game.late_draw(delta);
			break;
		}
	}
}

void Program::deinit_program_mode() {
	static_assert(NUM_PROGRAM_MODES == 3);

	switch (program_mode) {
		case PROGRAM_TITLE: {
			title_screen.deinit();
			break;
		}

		case PROGRAM_GAME: {
			game.deinit();
			break;
		}
	}
}

void Program::set_program_mode(Program_Mode mode) {
	if (next_program_mode != PROGRAM_NONE) {
		log_error("Trying to change program mode while in screen transition.");
		return;
	}

	next_program_mode = mode;
	transition_t = 0;
}

#ifdef DEVELOPER
static string s_ConsoleCommandsBuf[] = {
	"help",
	"title",
	"game",
	"collision_test",
	"show_height",
	"show_width",
	"show_player_hitbox",
	"show_debug_info",
	"show_hitboxes",
};

array<string> g_ConsoleCommands = s_ConsoleCommandsBuf;

bool console_callback(string str, void* userdata) {
	eat_whitespace(&str);
	string command = eat_non_whitespace(&str);

	if (command == "h" || command == "help") {
		console.write("Commands: ");
		For (it, g_ConsoleCommands) {
			console.write(*it);
			console.write(' ');
		}
		console.write("\n");

		return true;
	}

	if (command == "title") {
		program.set_program_mode(PROGRAM_TITLE);
		return true;
	}

	if (command == "game") {
		program.set_program_mode(PROGRAM_GAME);
		return true;
	}

	if (program.program_mode == PROGRAM_GAME) {
		if (command == "collision_test") {
			game.collision_test ^= true;
			return true;
		}

		if (command == "show_height") {
			game.show_height ^= true;
			if (game.show_height) game.show_width = false;
			return true;
		}

		if (command == "show_width") {
			game.show_width ^= true;
			if (game.show_width) game.show_height = false;
			return true;
		}

		if (command == "show_player_hitbox") {
			game.show_player_hitbox ^= true;
			return true;
		}

		if (command == "show_debug_info") {
			game.show_debug_info ^= true;
			return true;
		}

		if (command == "show_hitboxes") {
			game.show_hitboxes ^= true;
			return true;
		}
	}

	return false;
}
#endif
