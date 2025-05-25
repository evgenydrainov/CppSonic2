#include "program.h"

#include "window_creation.h"
#include "renderer.h"
#include "assets.h"
#include "console.h"
#include "input.h"
#include "game.h"
#include "title_screen.h"

Program program;

void Program::init(int argc, char* argv[]) {
	Program_Mode mode = PROGRAM_TITLE;

#ifdef DEVELOPER
	// if the second argument is --game
	if (argc >= 2 && strcmp(argv[1], "--game") == 0) {
		// ...and there's a third arg, then start the level
		if (argc >= 3) {
			mode = PROGRAM_GAME;

			level_filepath = copy_c_string(argv[2]);
		}
	}
#endif

	if (level_filepath.count == 0) {
		level_filepath = copy_string("levels/EEZ_Act1");
	}

	set_program_mode(mode);
	transition_t = 1; // skip the fade in

	show_debug_info = true;

	this->argc = argc;
	this->argv = argv;
}

void Program::deinit() {
	deinit_program_mode();
}

void Program::update(float delta) {
	if (is_key_pressed(SDL_SCANCODE_F1)) {
		show_debug_info ^= true;
	}

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

	set_proj_mat(get_ortho(0, window.game_width, window.game_height, 0));
	defer { set_proj_mat({1}); };

	set_viewport(0, 0, window.game_width, window.game_height);

	// fill the screen with black during transition
	if (transition_t != 0) {
		Rectf rect = {0, 0, (float)window.game_width, (float)window.game_height};

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
	int backbuffer_width;
	int backbuffer_height;
	SDL_GL_GetDrawableSize(window.handle, &backbuffer_width, &backbuffer_height);

	set_proj_mat(get_ortho(0, backbuffer_width, backbuffer_height, 0));
	set_viewport(0, 0, backbuffer_width, backbuffer_height);

	vec2 pos = {};

	if (window.frame_advance_mode) {
		string str = "F5 - Next Frame\nF6 - Disable Frame Advance Mode\n";
		pos = draw_text_shadow(get_font(fnt_consolas_bold), str, pos);
	}

#ifdef DEVELOPER
	if (program.show_debug_info) {
		char buf[256];
		string str = Sprintf(buf,
							 "frame: %fms\n"
							 "update: %fms\n"
							 "draw: %fms\n"
							 "draw calls: %d\n"
							 "total triangles: %d\n",
							 window.frame_took * 1000.0,
							 (window.frame_took - renderer.draw_took) * 1000.0,
							 renderer.draw_took * 1000.0,
							 renderer.draw_calls,
							 renderer.total_triangles);
		pos = draw_text_shadow(get_font(fnt_consolas_bold), str, pos);

		if (program_mode == PROGRAM_GAME) {
			Player* p = &game.player;

			char buf[256];
			string str = Sprintf(buf,
								 "state: %s\n"
								 "ground speed: %f\n"
								 "ground angle: %f\n"
								 "xspeed: %f\n"
								 "yspeed: %f\n",
								 GetPlayerStateName(p->state),
								 p->ground_speed,
								 p->ground_angle,
								 p->speed.x,
								 p->speed.y);
			pos = draw_text_shadow(get_font(fnt_consolas_bold), str, pos);
		}
	}
#endif

	break_batch();
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
	"load_level",
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

	if (command == "show_debug_info") {
		program.show_debug_info ^= true;
		return true;
	}

	if (command == "load_level") {
		eat_whitespace(&str);
		string level = eat_non_whitespace(&str);

		free(program.level_filepath.data);
		program.level_filepath = copy_string(level);

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

		if (command == "show_hitboxes") {
			game.show_hitboxes ^= true;
			return true;
		}
	}

	return false;
}
#endif
