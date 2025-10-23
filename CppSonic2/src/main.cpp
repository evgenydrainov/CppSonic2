#include "common.h"
#include "window_creation.h"
#include "renderer.h"
#include "package.h"
#include "assets.h"
#include "input.h"
#include "program.h"
#include "tests.h"

#ifdef EDITOR
#include "imgui_glue.h"
#include "editor.h"
#endif

#ifdef DEVELOPER
#include "console.h"
#endif


#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif


static Arena temp_arena;
Arena* get_temp_arena() { return &temp_arena; }

size_t temp_memory_max_usage_this_frame;
size_t temp_memory_max_usage_ever;


static void do_one_frame() {
	reset_temporary_storage();

	begin_frame();

	// handle events
	{
		input.clear();

		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			bool handled = false;

			if (!handled) handled = window_handle_event(ev);

			#ifdef DEVELOPER
				if (!handled) handled = console.handle_event(ev);
			#endif

			if (!handled) handled = input.handle_event(ev);
		}
	}

	// update
	{
		input.update(window.delta, renderer.game_texture_rect, window.game_width, window.game_height);

		program.update(window.delta);

		#ifdef DEVELOPER
			console.update(window.delta);
		#endif
	}

	// draw
	{
		vec4 clear_color = {};
		render_begin_frame(clear_color);

		program.draw(window.delta);

		render_end_frame();
	}

	// late draw
	{
		program.late_draw(window.delta);

		#ifdef DEVELOPER
			console.draw(get_font(fnt_consolas_bold), window.delta);
		#endif

		break_batch();
	}

	swap_buffers();
}

static int game_main(int argc, char* argv[]) {
	init_window_and_opengl("Sonic VHS", 424, 240, 2, true, true);
	defer { deinit_window_and_opengl(); };

	init_package();
	defer { deinit_package(); };

	input.init();
	defer { input.deinit(); };

	init_mixer();
	defer { deinit_mixer(); };

	load_global_assets();
	load_assets_for_game();
	defer { free_all_assets(); };

	init_renderer();
	defer { deinit_renderer(); };

	program.init(argc, argv);
	defer { program.deinit(); };

#ifdef DEVELOPER
	console.init(console_callback, nullptr, g_ConsoleCommands);
	defer { console.deinit(); };
#endif

#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop(do_one_frame, 0, 1);
#else
	while (!window.should_quit) {
		do_one_frame();
	}
#endif

	return 0;
}


static int editor_main(int argc, char* argv[]) {
#ifdef EDITOR
	init_window_and_opengl("Editor", 424, 240, 2, true, true);
	defer { deinit_window_and_opengl(); };

	init_package();
	defer { deinit_package(); };

	load_global_assets();
	load_assets_for_editor();
	defer { free_all_assets(); };

	init_renderer();
	defer { deinit_renderer(); };

	init_imgui();
	defer { deinit_imgui(); };

	editor.init(argc, argv);
	defer { editor.deinit(); };

	while (!window.should_quit) {
		reset_temporary_storage();

		begin_frame();

		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			bool handled = false;

			/*if (!handled) handled = */window_handle_event(ev);

			/*if (!handled) handled = */imgui_handle_event(ev);
		}

		// update
		imgui_begin_frame();
		editor.update(window.delta);

		// render
		vec4 clear_color = color_black;
		render_begin_frame(clear_color);
		render_end_frame();

		imgui_render();

		swap_buffers();
	}
#else
	log_error("Trying to launch the editor but it wasn't compiled in.");
#endif

	return 0;
}


enum Launch_Mode {
	LAUNCH_GAME,
	LAUNCH_EDITOR,
};


int main(int argc, char* argv[]) {
	init_temporary_storage(Megabytes(1));
	defer { deinit_temporary_storage(); };

#if defined(_DEBUG)
	run_tests();
#endif

	Launch_Mode launch_mode = LAUNCH_GAME;

	if (argc >= 2) {
		if (strcmp(argv[1], "--editor") == 0) {
			launch_mode = LAUNCH_EDITOR;
		} else if (strcmp(argv[1], "--game") == 0) {
			launch_mode = LAUNCH_GAME;
		}
	}

	if (launch_mode == LAUNCH_GAME) {
		return game_main(argc, argv);
	} else if (launch_mode == LAUNCH_EDITOR) {
		return editor_main(argc, argv);
	}

	return 0;
}

