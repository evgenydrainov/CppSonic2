#include "common.h"
#include "window_creation.h"
#include "game.h"
#include "renderer.h"
#include "package.h"
#include "assets.h"
#include "program.h"

#ifdef EDITOR
#include "imgui_glue.h"
#include "editor.h"
#endif

#ifdef DEVELOPER
#include "console.h"
#endif


static int game_main(int argc, char* argv[]) {
	init_window_and_opengl("CppSonic2", 424, 240, 2, true, true);
	defer { deinit_window_and_opengl(); };

	init_mixer();
	defer { deinit_mixer(); };

	init_package();
	defer { deinit_package(); };

	load_global_assets();
	load_assets_for_game();
	defer { free_all_assets(); };

	init_renderer();
	defer { deinit_renderer(); };

	program.init(argc, argv);
	defer { program.deinit(); };

#ifdef DEVELOPER
	console.init(console_callback, nullptr, get_font(fnt_consolas_bold), g_ConsoleCommands);
	defer { console.deinit(); };
#endif

	while (!window.should_quit) {
		begin_frame();

		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			handle_event(ev);

#ifdef DEVELOPER
			console.handle_event(ev);
#endif
		}

		float delta = window.delta;

		// update
		program.update(delta);

#ifdef DEVELOPER
		console.update(delta);
#endif

		vec4 clear_color = color_cornflower_blue;
		render_begin_frame(clear_color);

		// draw
		program.draw(delta);

		render_end_frame();

		// late draw
		program.late_draw(delta);

#ifdef DEVELOPER
		console.draw(delta);
#endif

		swap_buffers();
	}

	return 0;
}

#ifdef EDITOR

static int editor_main(int argc, char* argv[]) {
	init_window_and_opengl("CppSonic2", 424, 240, 2, true, true);
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
		begin_frame();

		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			handle_event(ev);
			imgui_handle_event(ev);
		}

		float delta = window.delta;

		// update
		imgui_begin_frame();
		editor.update(delta);

		// render
		vec4 clear_color = color_cornflower_blue;
		render_begin_frame(clear_color);
		render_end_frame();

		imgui_render();

		swap_buffers();
	}

	return 0;
}

#endif

#ifdef EDITOR

int main(int argc, char* argv[]) {
	if (argc >= 2 && strcmp(argv[1], "--game") == 0) {
		return game_main(argc, argv);
	}

	return editor_main(argc, argv);
}

#else

int main(int argc, char* argv[]) {
	return game_main(argc, argv);
}

#endif
