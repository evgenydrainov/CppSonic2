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
#else
	game.init();
	defer { game.deinit(); };
#endif

#if defined(DEVELOPER) && !defined(EDITOR)
	console.init(console_callback, nullptr, game.font_consolas);
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
#else
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


#pragma warning(push, 0)


#define STB_SPRINTF_IMPLEMENTATION
#include <stb/stb_sprintf.h>
#undef STB_SPRINTF_IMPLEMENTATION


#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
// TODO: STBI_ASSERT
#include <stb/stb_image.h>
#undef STB_IMAGE_IMPLEMENTATION


#define GLAD_GL_IMPLEMENTATION
#include <glad/gl.h>
#undef GLAD_GL_IMPLEMENTATION


#pragma warning(pop)

