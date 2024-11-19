#include "common.h"
#include "window_creation.h"
#include "game.h"
#include "renderer.h"
#include "package.h"

#ifdef EDITOR
#include "imgui_glue.h"
#include "editor.h"
#endif

#if defined(DEVELOPER) && !defined(EDITOR)
#include "console.h"
#endif


int main(int /*argc*/, char* /*argv*/[]) {
	init_window_and_opengl("CppSonic2", 424, 240, 2, true, true);
	defer { deinit_window_and_opengl(); };

	init_package();
	defer { deinit_package(); };

	init_renderer();
	defer { deinit_renderer(); };

#ifdef EDITOR
	init_imgui();
	defer { deinit_imgui(); };

	editor.init();
	defer { editor.deinit(); };
#endif

#ifndef EDITOR
	game.init();
	defer { game.deinit(); };
#endif

#if defined(DEVELOPER) && !defined(EDITOR)
	console.init(console_callback, nullptr, game.consolas_bold);
	defer { console.deinit(); };
#endif

	while (!window.should_quit) {
		begin_frame();

		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			handle_event(ev);

#ifdef EDITOR
			imgui_handle_event(ev);
#endif

#if defined(DEVELOPER) && !defined(EDITOR)
			console.handle_event(ev);
#endif
		}

		// update
#ifdef EDITOR
		imgui_begin_frame();
		editor.update(window.delta);
#endif

#ifndef EDITOR
		game.update(window.delta);
#endif

#if defined(DEVELOPER) && !defined(EDITOR)
		console.update(window.delta);
#endif

		// render
		vec4 clear_color = color_cornflower_blue;
		render_begin_frame(clear_color);

#ifndef EDITOR
		game.draw(window.delta);
#endif

		render_end_frame();

#ifndef EDITOR
		game.late_draw(window.delta);
#endif

#ifdef EDITOR
		imgui_render();
#endif

#if defined(DEVELOPER) && !defined(EDITOR)
		console.draw(window.delta);
#endif

		swap_buffers();
	}

	return 0;
}


