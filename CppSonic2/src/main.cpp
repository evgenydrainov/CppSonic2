#include "common.h"
#include "window_creation.h"
#include "game.h"
#include "renderer.h"
#include "package.h"

#ifdef EDITOR
#include "imgui_glue.h"
#include "editor.h"
#endif


int main(int /*argc*/, char* /*argv*/[]) {
	init_window_and_opengl("CppSonic2", 424, 240, 2, true);
	defer { deinit_window_and_opengl(); };

#ifdef EDITOR
	SDL_MaximizeWindow(window.handle);
#endif

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

	while (!window.should_quit) {
		begin_frame();

		// update
#ifdef EDITOR
		imgui_begin_frame();

		editor.update(window.delta);
#else
		game.update(window.delta);
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

