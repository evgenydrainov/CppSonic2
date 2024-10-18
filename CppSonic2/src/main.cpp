#include "common.h"
#include "window_creation.h"
#include "game.h"
#include "batch_renderer.h"
#include "package.h"
#include "imgui_glue.h"
#include "editor.h"


int main(int /*argc*/, char* /*argv*/[]) {
	init_window_and_opengl("CppSonic2", 424, 240, 2, false);
	defer { deinit_window_and_opengl(); };

	init_package();
	defer { deinit_package(); };

	init_renderer();
	defer { deinit_renderer(); };

	game.init();
	defer { game.deinit(); };

	init_imgui();
	defer { deinit_imgui(); };

	editor.init();
	defer { editor.deinit(); };

	while (!window.should_quit) {
		begin_frame();
		imgui_begin_frame();

		// update
		game.update(window.delta);

		editor.update(window.delta);

		// render
		{
			vec4 clear_color = color_cornflower_blue;
			render_begin_frame(clear_color);

			game.draw(window.delta);

			render_end_frame();

			imgui_render();
		}

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

