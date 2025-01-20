#include "title_screen.h"

#include "window_creation.h"
#include "renderer.h"
#include "font.h"
#include "assets.h"
#include "program.h"

#include "util.h"

Title_Screen title_screen;

static const char shd_palette_vert_src[] = R"(
#version 330 core

layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec3 in_Normal;
layout(location = 2) in vec4 in_Color;
layout(location = 3) in vec2 in_TexCoord;

out vec4 v_Color;
out vec2 v_TexCoord;

uniform mat4 u_MVP;

void main() {
	gl_Position = u_MVP * vec4(in_Position, 1.0);

	v_Color    = in_Color;
	v_TexCoord = in_TexCoord;
}
)";

static const char shd_palette_frag_src[] = R"(
#version 330 core

layout(location = 0) out vec4 FragColor;

in vec4 v_Color;
in vec2 v_TexCoord;

uniform sampler2D u_Texture;
uniform sampler2D u_Palette;
uniform float u_PaletteIndex;
uniform float u_PaletteSize;

void main() {
	vec4 index = texture(u_Texture, v_TexCoord);
	vec4 color = texture(u_Palette, vec2(index.r, u_PaletteIndex / u_PaletteSize));
	FragColor = vec4(color.rgb, index.a);
}
)";

static u32 shd_palette;

void Title_Screen::init() {
	if (!shd_palette) {
		u32 shd_palette_vert = compile_shader(GL_VERTEX_SHADER, shd_palette_vert_src, "shd_palette_vert");
		defer { glDeleteShader(shd_palette_vert); };

		u32 shd_palette_frag = compile_shader(GL_FRAGMENT_SHADER, shd_palette_frag_src, "shd_palette_frag");
		defer { glDeleteShader(shd_palette_frag); };

		// @Leak
		shd_palette = link_program(shd_palette_vert, shd_palette_frag, "shd_palette");
	}
}

void Title_Screen::deinit() {
	SDL_SetRelativeMouseMode(SDL_FALSE);
}

void Title_Screen::update(float delta) {
	if (program.transition_t != 0) {
		return;
	}

	if (is_key_pressed(SDL_SCANCODE_Z)) {
		program.set_program_mode(PROGRAM_GAME);
	}
}

void Title_Screen::draw(float delta) {
	{
		// bg
		vec4 color = get_color(0, 0, 108);
		glClearColor(color.r, color.g, color.b, color.a);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	{
		// 3D clouds

		static vec3 pos = {0, -40, 0};
		static float pitch = -1;
		static float yaw = -90;

		vec3 forward;
		forward.x = dcos(yaw) * dcos(pitch);
		forward.y = dsin(pitch);
		forward.z = dsin(yaw) * dcos(pitch);

		vec3 up = {0, 1, 0};

		int mouse_x = 0;
		int mouse_y = 0;

		SDL_GetRelativeMouseState(&mouse_x, &mouse_y);

		if (is_key_pressed(SDL_SCANCODE_F3)) {
			SDL_SetRelativeMouseMode((SDL_bool) !SDL_GetRelativeMouseMode());
		}

		if (SDL_GetRelativeMouseMode()) {
			pitch -= mouse_y / 4.0f;
			yaw   += mouse_x / 4.0f;

			float spd = 1.0f;

			if (is_key_held(SDL_SCANCODE_LSHIFT)) {
				spd /= 4;
			}

			if (is_key_held(SDL_SCANCODE_W)) {
				pos += (spd * delta) * forward;
			}

			if (is_key_held(SDL_SCANCODE_S)) {
				pos -= (spd * delta) * forward;
			}

			if (is_key_held(SDL_SCANCODE_A)) {
				pos.z += (spd * delta) * dsin(yaw - 90);
				pos.x += (spd * delta) * dcos(yaw - 90);
			}

			if (is_key_held(SDL_SCANCODE_D)) {
				pos.z += (spd * delta) * dsin(yaw + 90);
				pos.x += (spd * delta) * dcos(yaw + 90);
			}
		}

		pitch = clamp(pitch, -89.0f, 89.0f);
		yaw   = wrapf(yaw, 360.0f);

		break_batch();
		renderer.proj_mat = glm::perspectiveFov<float>(to_radians(60), window.game_width, window.game_height, 0.1f, 10'000.0f);
		renderer.view_mat = glm::lookAt(pos, pos + forward, up);
		renderer.model_mat = {1};
		glViewport(0, 0, window.game_width, window.game_height);

		// clouds
		const Texture& t = get_texture(tex_title_clouds);

		float r = 10;

		float off = glm::fract(SDL_GetTicks() / 5'000.0f);

		Vertex vertices[] = {
			{{-100 * r, 0, -100 * r}, {}, color_white, {0, 0 - off}},
			{{ 100 * r, 0, -100 * r}, {}, color_white, {r, 0 - off}},
			{{ 100 * r, 0,  100 * r}, {}, color_white, {r, r - off}},
			{{-100 * r, 0,  100 * r}, {}, color_white, {0, r - off}},
		};

		draw_quad(t, vertices);
	}

	break_batch();
	renderer.proj_mat = glm::ortho<float>(0, window.game_width, window.game_height, 0);
	renderer.view_mat = {1};
	renderer.model_mat = {1};
	glViewport(0, 0, window.game_width, window.game_height);

	{
		const Texture& t = get_texture(tex_title_water);
		vec2 pos = {0, window.game_height - t.height};

		// mountains left
		draw_sprite(get_sprite(spr_title_mountains_left), 0, pos);

		// mountains right
		pos.x = window.game_width;
		draw_sprite(get_sprite(spr_title_mountains_right), 0, pos);

		pos.x = 0;

		// water
		set_shader(shd_palette);

		glUniform1i(glGetUniformLocation(shd_palette, "u_Palette"), 1);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, get_texture(tex_title_water_palette).ID);

		int palette_index = (SDL_GetTicks() / 200) % 3 + 1;

		glUniform1f(glGetUniformLocation(shd_palette, "u_PaletteIndex"), palette_index);
		glUniform1f(glGetUniformLocation(shd_palette, "u_PaletteSize"), 4);

		auto draw_lane = [&](int lane, vec2 pos) {
			pos.y += 16 * lane;

			pos.x += (SDL_GetTicks() / (10 * (4 - lane))) % t.width;

			pos.x -= t.width;
			draw_texture(t, {0, lane * 16, t.width, 16}, pos);

			pos.x += t.width;
			draw_texture(t, {0, lane * 16, t.width, 16}, pos);

			pos.x += t.width;
			draw_texture(t, {0, lane * 16, t.width, 16}, pos);
		};

		draw_lane(0, pos);
		draw_lane(1, pos);
		draw_lane(2, pos);
		draw_lane(3, pos);

		reset_shader();
	}

	vec2 title_pos = {window.game_width / 2, window.game_height / 2};
	title_pos.y -= 1 * 8;

	{
		// medal
		vec2 pos = title_pos;
		draw_sprite(get_sprite(spr_title_medal), 0, pos);
		draw_sprite(get_sprite(spr_title_medal), 0, pos, {-1, 1});
	}

	{
		// sonic
		vec2 pos = title_pos;
		pos.x += 1 * 8;
		pos.y -= 4 * 8;

		draw_sprite(get_sprite(spr_title_sonic), 4, pos);
	}

	{
		// label
		vec2 pos = title_pos;
		pos.y += 7 * 8;
		draw_sprite(get_sprite(spr_title_label), 0, pos);
	}

	if (SDL_GetTicks() % 1000 > 500) {
		// press start
		vec2 pos = title_pos;
		pos.y += 12 * 8;

		draw_text(get_font(fnt_hud), "press start", pos, HALIGN_CENTER, VALIGN_MIDDLE, color_yellow);
	}

	break_batch();
}
