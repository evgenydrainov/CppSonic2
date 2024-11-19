#include "renderer.h"

#include "package.h"
#include "window_creation.h"
#include "util.h"

Renderer renderer = {};


static char texture_vert_shader_src[] = R"(
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



static char texture_frag_shader_src[] = R"(
#version 330 core

layout(location = 0) out vec4 FragColor;

in vec4 v_Color;
in vec2 v_TexCoord;

uniform sampler2D u_Texture;

void main() {
	vec4 color = texture(u_Texture, v_TexCoord);

	FragColor = color * v_Color;
}
)";



static char circle_frag_shader_src[] = R"(
#version 330 core

layout(location = 0) out vec4 FragColor;

in vec4 v_Color;
in vec2 v_TexCoord;

uniform sampler2D u_Texture;

void main() {
	vec4 color = texture(u_Texture, v_TexCoord);

	vec2 coord = v_TexCoord * 2.0 - 1.0;

	if (coord.x * coord.x + coord.y * coord.y >= 1.0) {
		color.a = 0.0;
	}

	FragColor = color * v_Color;
}
)";



static char sharp_bilinear_frag_shader_src[] = R"(
#version 330 core

/*
	Author: rsn8887 (based on TheMaister)
	License: Public domain

	This is an integer prescale filter that should be combined
	with a bilinear hardware filtering (GL_BILINEAR filter or some such) to achieve
	a smooth scaling result with minimum blur. This is good for pixelgraphics
	that are scaled by non-integer factors.
*/

layout(location = 0) out vec4 FragColor;

in vec4 v_Color;
in vec2 v_TexCoord;

uniform sampler2D u_Texture;

uniform vec2 u_SourceSize;
uniform vec2 u_Scale; // The integer scale.

void main() {
	vec2 texel = v_TexCoord * u_SourceSize;
	vec2 scale = u_Scale;

	vec2 texel_floored = floor(texel);
	vec2 s = fract(texel);
	vec2 region_range = 0.5 - 0.5 / scale;

	// Figure out where in the texel to sample to get correct pre-scaled bilinear.
	// Uses the hardware bilinear interpolator to avoid having to sample 4 times manually.

	vec2 center_dist = s - 0.5;
	vec2 f = (center_dist - clamp(center_dist, -region_range, region_range)) * scale + 0.5;

	vec2 mod_texel = texel_floored + f;

	vec4 color = texture(u_Texture, mod_texel / u_SourceSize);
	FragColor = color * v_Color;
}
)";



static void set_vertex_attribs() {
	// Position
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
	glEnableVertexAttribArray(0);

	// Normal
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
	glEnableVertexAttribArray(1);

	// Color
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
	glEnableVertexAttribArray(2);

	// Texcoord
	glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
	glEnableVertexAttribArray(3);
}

void init_renderer() {
	// 
	// Initialize.
	// 
	{
		glGenVertexArrays(1, &renderer.batch_vao);
		glGenBuffers(1, &renderer.batch_vbo);
		glGenBuffers(1, &renderer.batch_ebo);

		// 1. bind Vertex Array Object
		glBindVertexArray(renderer.batch_vao);

		// 2. copy our vertices array in a vertex buffer for OpenGL to use
		glBindBuffer(GL_ARRAY_BUFFER, renderer.batch_vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * BATCH_MAX_VERTICES, nullptr, GL_DYNAMIC_DRAW);

		u32* indices = (u32*) malloc(BATCH_MAX_INDICES * sizeof(u32));
		defer { free(indices); };

		u32 offset = 0;
		for (size_t i = 0; i < BATCH_MAX_INDICES; i += 6) {
			indices[i + 0] = offset + 0;
			indices[i + 1] = offset + 1;
			indices[i + 2] = offset + 2;

			indices[i + 3] = offset + 2;
			indices[i + 4] = offset + 3;
			indices[i + 5] = offset + 0;

			offset += 4;
		}

		// 3. copy our index array in a element buffer for OpenGL to use
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer.batch_ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u32) * BATCH_MAX_INDICES, indices, GL_STATIC_DRAW);

		// 4. then set the vertex attributes pointers
		set_vertex_attribs();

		glBindVertexArray(0);

		renderer.vertices = malloc_bump_array<Vertex>(BATCH_MAX_VERTICES);

		// stub texture
		glGenTextures(1, &renderer.stub_texture);
		glBindTexture(GL_TEXTURE_2D, renderer.stub_texture);

		u32 pixel_data[1] = {0xffffffff};
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel_data);

		glBindTexture(GL_TEXTURE_2D, 0);
	}

	// 
	// Load shaders.
	// 
	{
		u32 texture_vert_shader = compile_shader(GL_VERTEX_SHADER, texture_vert_shader_src);
		defer { glDeleteShader(texture_vert_shader); };

		u32 texture_frag_shader = compile_shader(GL_FRAGMENT_SHADER, texture_frag_shader_src);
		defer { glDeleteShader(texture_frag_shader); };

		u32 circle_frag_shader = compile_shader(GL_FRAGMENT_SHADER, circle_frag_shader_src);
		defer { glDeleteShader(circle_frag_shader); };

		u32 sharp_bilinear_frag_shader = compile_shader(GL_FRAGMENT_SHADER, sharp_bilinear_frag_shader_src);
		defer { glDeleteShader(sharp_bilinear_frag_shader); };

		renderer.texture_shader = link_program(texture_vert_shader, texture_frag_shader);
		renderer.circle_shader = link_program(texture_vert_shader, circle_frag_shader);
		renderer.sharp_bilinear_shader = link_program(texture_vert_shader, sharp_bilinear_frag_shader);

		renderer.current_shader = renderer.texture_shader;
	}

	{
		glGenTextures(1, &renderer.game_texture); // @Leak

		glBindTexture(GL_TEXTURE_2D, renderer.game_texture);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // GL_LINEAR for sharp bilinear shader
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, window.game_width, window.game_height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

		glBindTexture(GL_TEXTURE_2D, 0);
	}

	{
		glGenFramebuffers(1, &renderer.game_framebuffer); // @Leak

		glBindFramebuffer(GL_FRAMEBUFFER, renderer.game_framebuffer);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderer.game_texture, 0);
		// glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,  GL_TEXTURE_2D, game_depth_texture, 0);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
}

void deinit_renderer() {
	free(renderer.vertices.data);
}

void render_begin_frame(vec4 clear_color) {
	renderer.draw_took_t = get_time();

	Assert(renderer.vertices.count == 0);

	renderer.draw_calls      = renderer.curr_draw_calls;
	renderer.max_batch       = renderer.curr_max_batch;
	renderer.total_triangles = renderer.curr_total_triangles;

	renderer.curr_draw_calls      = 0;
	renderer.curr_max_batch       = 0;
	renderer.curr_total_triangles = 0;

	glBindFramebuffer(GL_FRAMEBUFFER, renderer.game_framebuffer);

	glClearColor(clear_color.r, clear_color.g, clear_color.b, clear_color.a);
	glClear(GL_COLOR_BUFFER_BIT);

	glViewport(0, 0, window.game_width, window.game_height);
}

void render_end_frame() {
	break_batch();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	int backbuffer_width;
	int backbuffer_height;
	SDL_GL_GetDrawableSize(window.handle, &backbuffer_width, &backbuffer_height);

	glViewport(0, 0, backbuffer_width, backbuffer_height);
	renderer.proj_mat = glm::ortho(0.0f, (float)backbuffer_width, (float)backbuffer_height, 0.0f);
	renderer.view_mat = {1};
	renderer.model_mat = {1};

	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	{
		u32 program    = renderer.sharp_bilinear_shader;
		u32 old_shader = renderer.current_shader;

		renderer.current_shader = program;
		glUseProgram(program);

		float xscale = backbuffer_width  / (float)window.game_width;
		float yscale = backbuffer_height / (float)window.game_height;
		float scale = fminf(xscale, yscale);

		renderer.game_texture_rect.w = (int) (window.game_width  * scale);
		renderer.game_texture_rect.h = (int) (window.game_height * scale);
		renderer.game_texture_rect.x = (backbuffer_width  - renderer.game_texture_rect.w) / 2;
		renderer.game_texture_rect.y = (backbuffer_height - renderer.game_texture_rect.h) / 2;

		{
			int u_SourceSize = glGetUniformLocation(program, "u_SourceSize");
			glUniform2f(u_SourceSize, (float)window.game_width, (float)window.game_height);
		}

		{
			float int_scale = fmaxf(floorf(scale), 1.0f);

			int u_Scale = glGetUniformLocation(program, "u_Scale");
			glUniform2f(u_Scale, int_scale, int_scale);
		}

		Texture t;
		t.ID = renderer.game_texture;
		t.width  = window.game_width;
		t.height = window.game_height;
		draw_texture(t, {}, {(float)renderer.game_texture_rect.x, (float)renderer.game_texture_rect.y}, {scale, scale}, {}, 0, color_white, {false, true});

		break_batch();

		renderer.current_shader = old_shader;
	}

	renderer.draw_took = get_time() - renderer.draw_took_t;
}

void break_batch() {
	if (renderer.vertices.count == 0) {
		return;
	}

	Assert(renderer.current_mode != MODE_NONE);
	Assert(renderer.current_texture != 0);

	{
		glBindBuffer(GL_ARRAY_BUFFER, renderer.batch_vbo);
		glBufferSubData(GL_ARRAY_BUFFER, 0, renderer.vertices.count * sizeof(Vertex), renderer.vertices.data);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	switch (renderer.current_mode) {
		case MODE_QUADS: {
			u32 program = renderer.current_shader;

			glUseProgram(program);
			defer { glUseProgram(0); };

			mat4 MVP = (renderer.proj_mat * renderer.view_mat) * renderer.model_mat;

			int u_MVP = glGetUniformLocation(program, "u_MVP");
			glUniformMatrix4fv(u_MVP, 1, GL_FALSE, &MVP[0][0]);

			glBindTexture(GL_TEXTURE_2D, renderer.current_texture);
			defer { glBindTexture(GL_TEXTURE_2D, 0); };

			glBindVertexArray(renderer.batch_vao);
			defer { glBindVertexArray(0); };

			Assert(renderer.vertices.count % 4 == 0);

			glDrawElements(GL_TRIANGLES, (GLsizei)renderer.vertices.count / 4 * 6, GL_UNSIGNED_INT, 0);

			renderer.curr_draw_calls++;
			renderer.curr_max_batch = max(renderer.curr_max_batch, renderer.vertices.count);
			renderer.curr_total_triangles += (GLsizei)renderer.vertices.count / 4 * 2;
			break;
		}

		case MODE_TRIANGLES: {
			u32 program = renderer.current_shader;

			glUseProgram(program);
			defer { glUseProgram(0); };

			mat4 MVP = (renderer.proj_mat * renderer.view_mat) * renderer.model_mat;

			int u_MVP = glGetUniformLocation(program, "u_MVP");
			glUniformMatrix4fv(u_MVP, 1, GL_FALSE, &MVP[0][0]);

			glBindTexture(GL_TEXTURE_2D, renderer.current_texture);
			defer { glBindTexture(GL_TEXTURE_2D, 0); };

			glBindVertexArray(renderer.batch_vao);
			defer { glBindVertexArray(0); };

			Assert(renderer.vertices.count % 3 == 0);

			glDrawArrays(GL_TRIANGLES, 0, (GLsizei)renderer.vertices.count);

			renderer.curr_draw_calls++;
			renderer.curr_max_batch = max(renderer.curr_max_batch, renderer.vertices.count);
			renderer.curr_total_triangles += (GLsizei)renderer.vertices.count / 3;
			break;
		}

		case MODE_LINES: {
			u32 program = renderer.current_shader;

			glUseProgram(program);
			defer { glUseProgram(0); };

			mat4 MVP = (renderer.proj_mat * renderer.view_mat) * renderer.model_mat;

			int u_MVP = glGetUniformLocation(program, "u_MVP");
			glUniformMatrix4fv(u_MVP, 1, GL_FALSE, &MVP[0][0]);

			glBindTexture(GL_TEXTURE_2D, renderer.current_texture);
			defer { glBindTexture(GL_TEXTURE_2D, 0); };

			glBindVertexArray(renderer.batch_vao);
			defer { glBindVertexArray(0); };

			Assert(renderer.vertices.count % 2 == 0);

			glDrawArrays(GL_LINES, 0, (GLsizei)renderer.vertices.count);

			renderer.curr_draw_calls++;
			renderer.curr_max_batch = max(renderer.curr_max_batch, renderer.vertices.count);
			break;
		}

		case MODE_POINTS: {
			u32 program = renderer.current_shader;

			glUseProgram(program);
			defer { glUseProgram(0); };

			mat4 MVP = (renderer.proj_mat * renderer.view_mat) * renderer.model_mat;

			int u_MVP = glGetUniformLocation(program, "u_MVP");
			glUniformMatrix4fv(u_MVP, 1, GL_FALSE, &MVP[0][0]);

			glBindTexture(GL_TEXTURE_2D, renderer.current_texture);
			defer { glBindTexture(GL_TEXTURE_2D, 0); };

			glBindVertexArray(renderer.batch_vao);
			defer { glBindVertexArray(0); };

			glDrawArrays(GL_POINTS, 0, (GLsizei)renderer.vertices.count);

			renderer.curr_draw_calls++;
			renderer.curr_max_batch = max(renderer.curr_max_batch, renderer.vertices.count);
			break;
		}

		case MODE_CIRCLES: {
			u32 program = renderer.circle_shader;

			glUseProgram(program);
			defer { glUseProgram(0); };

			mat4 MVP = (renderer.proj_mat * renderer.view_mat) * renderer.model_mat;

			int u_MVP = glGetUniformLocation(program, "u_MVP");
			glUniformMatrix4fv(u_MVP, 1, GL_FALSE, &MVP[0][0]);

			glBindTexture(GL_TEXTURE_2D, renderer.current_texture);
			defer { glBindTexture(GL_TEXTURE_2D, 0); };

			glBindVertexArray(renderer.batch_vao);
			defer { glBindVertexArray(0); };

			Assert(renderer.vertices.count % 4 == 0);

			glDrawElements(GL_TRIANGLES, (GLsizei)renderer.vertices.count / 4 * 6, GL_UNSIGNED_INT, 0);

			renderer.curr_draw_calls++;
			renderer.curr_max_batch = max(renderer.curr_max_batch, renderer.vertices.count);
			renderer.curr_total_triangles += (GLsizei)renderer.vertices.count / 4 * 2;
			break;
		}
	}

	renderer.vertices.count = 0;
	renderer.current_texture = 0;
	renderer.current_mode = MODE_NONE;
}


void draw_texture(Texture t, Rect src,
				  vec2 pos, vec2 scale,
				  vec2 origin, float angle, vec4 color, glm::bvec2 flip) {
	if (t.ID == 0) {
		log_error("Trying to draw invalid texture.");
		return;
	}

	if (src.w == 0 && src.h == 0) {
		src.w = t.width;
		src.h = t.height;
	}

	if (t.ID != renderer.current_texture
		|| renderer.current_mode != MODE_QUADS
		|| renderer.vertices.count + VERTICES_PER_QUAD > BATCH_MAX_VERTICES)
	{
		break_batch();

		renderer.current_texture = t.ID;
		renderer.current_mode = MODE_QUADS;
	}

	{
		float x1 = -origin.x;
		float y1 = -origin.y;
		float x2 = src.w - origin.x;
		float y2 = src.h - origin.y;

		float u1 =  src.x          / (float)t.width;
		float v1 =  src.y          / (float)t.height;
		float u2 = (src.x + src.w) / (float)t.width;
		float v2 = (src.y + src.h) / (float)t.height;

		if (flip.x) {
			float temp = u1;
			u1 = u2;
			u2 = temp;
		}

		if (flip.y) {
			float temp = v1;
			v1 = v2;
			v2 = temp;
		}

		Vertex vertices[] = {
			{{x1, y1, 0.0f}, {}, color, {u1, v1}}, // LT
			{{x2, y1, 0.0f}, {}, color, {u2, v1}}, // RT
			{{x2, y2, 0.0f}, {}, color, {u2, v2}}, // RB
			{{x1, y2, 0.0f}, {}, color, {u1, v2}}, // LB
		};

		// Has to be in this order (learned it the hard way)
		mat4 model = glm::translate(mat4{1.0f}, {pos.x, pos.y, 0.0f});
		model = glm::rotate(model, glm::radians(-angle), {0.0f, 0.0f, 1.0f});
		model = glm::scale(model, {scale.x, scale.y, 1.0f});

		vertices[0].pos = model * vec4{vertices[0].pos, 1.0f};
		vertices[1].pos = model * vec4{vertices[1].pos, 1.0f};
		vertices[2].pos = model * vec4{vertices[2].pos, 1.0f};
		vertices[3].pos = model * vec4{vertices[3].pos, 1.0f};

		array_add(&renderer.vertices, vertices[0]);
		array_add(&renderer.vertices, vertices[1]);
		array_add(&renderer.vertices, vertices[2]);
		array_add(&renderer.vertices, vertices[3]);
	}
}

void draw_texture_centered(Texture t,
						   vec2 pos, vec2 scale,
						   float angle, vec4 color, glm::bvec2 flip) {
	vec2 origin = {t.width / 2.0f, t.height / 2.0f};
	draw_texture(t, {}, pos, scale, origin, angle, color, flip);
}

void draw_rectangle(Rectf rect, vec4 color) {
	Texture t = {renderer.stub_texture, 1, 1};
	vec2 pos = {rect.x, rect.y};
	vec2 scale = {rect.w, rect.h};
	draw_texture(t, {}, pos, scale, {}, 0, color);
}

void draw_rectangle(Rectf rect, vec2 scale,
					vec2 origin, float angle, vec4 color) {
	Texture t = {renderer.stub_texture, 1, 1};
	vec2 pos = {rect.x, rect.y};

	origin.x /= rect.w;
	origin.y /= rect.h;

	rect.w *= scale.x;
	rect.h *= scale.y;

	draw_texture(t, {}, pos, {rect.w, rect.h}, origin, angle, color);
}

void draw_triangle(vec2 p1, vec2 p2, vec2 p3, vec4 color) {
	if (renderer.current_texture != renderer.stub_texture
		|| renderer.current_mode != MODE_TRIANGLES
		|| renderer.vertices.count + 3 > BATCH_MAX_VERTICES)
	{
		break_batch();

		renderer.current_texture = renderer.stub_texture;
		renderer.current_mode = MODE_TRIANGLES;
	}

	{
		Vertex vertices[] = {
			{{p1.x, p1.y, 0.0f}, {}, color, {}},
			{{p2.x, p2.y, 0.0f}, {}, color, {}},
			{{p3.x, p3.y, 0.0f}, {}, color, {}},
		};

		array_add(&renderer.vertices, vertices[0]);
		array_add(&renderer.vertices, vertices[1]);
		array_add(&renderer.vertices, vertices[2]);
	}
}

void draw_circle(vec2 pos, float radius, vec4 color, int precision) {
#if 0
	for (int i = 0; i < precision; i++) {
		float angle1 = (float)i       / (float)precision * 2.0f * glm::pi<float>();
		float angle2 = (float)(i + 1) / (float)precision * 2.0f * glm::pi<float>();
		vec2 p1 = pos + vec2{cosf(angle1), -sinf(angle1)} * radius;
		vec2 p2 = pos + vec2{cosf(angle2), -sinf(angle2)} * radius;
		draw_triangle(p1, p2, pos, color);
	}
#else
	if (renderer.current_texture != renderer.stub_texture
		|| renderer.current_mode != MODE_CIRCLES
		|| renderer.vertices.count + 4 > BATCH_MAX_VERTICES)
	{
		break_batch();

		renderer.current_texture = renderer.stub_texture;
		renderer.current_mode = MODE_CIRCLES;
	}

	{
		Vertex vertices[] = {
			{{pos.x - radius, pos.y - radius, 0.0f}, {}, color, {0.0f, 0.0f}}, // LT
			{{pos.x + radius, pos.y - radius, 0.0f}, {}, color, {1.0f, 0.0f}}, // RT
			{{pos.x + radius, pos.y + radius, 0.0f}, {}, color, {1.0f, 1.0f}}, // RB
			{{pos.x - radius, pos.y + radius, 0.0f}, {}, color, {0.0f, 1.0f}}, // LB
		};

		array_add(&renderer.vertices, vertices[0]);
		array_add(&renderer.vertices, vertices[1]);
		array_add(&renderer.vertices, vertices[2]);
		array_add(&renderer.vertices, vertices[3]);
	}
#endif
}

void draw_line(vec2 p1, vec2 p2, vec4 color) {
	if (renderer.current_texture != renderer.stub_texture
		|| renderer.current_mode != MODE_LINES
		|| renderer.vertices.count + 2 > BATCH_MAX_VERTICES)
	{
		break_batch();

		renderer.current_texture = renderer.stub_texture;
		renderer.current_mode = MODE_LINES;
	}

	p1 += vec2(0.5f, 0.5f);
	p2 += vec2(0.5f, 0.5f);

	{
		/* stolen from SDL2 */
		/* still not pixel perfect */

		const GLfloat xstart = p1.x;
		const GLfloat ystart = p1.y;
		const GLfloat xend = p2.x;
		const GLfloat yend = p2.y;
		/* bump a little in the direction we are moving in. */
		const GLfloat deltax = xend - xstart;
		const GLfloat deltay = yend - ystart;
		const GLfloat angle = atan2f(deltay, deltax);
		p2.x = xend + (cosf(angle) * 0.25f);
		p2.y = yend + (sinf(angle) * 0.25f);
	}

	Vertex vertices[] = {
		{{p1.x, p1.y, 0.0f}, {}, color, {}},
		{{p2.x, p2.y, 0.0f}, {}, color, {}},
	};

	array_add(&renderer.vertices, vertices[0]);
	array_add(&renderer.vertices, vertices[1]);
}

void draw_point(vec2 point, vec4 color) {
	if (renderer.current_texture != renderer.stub_texture
		|| renderer.current_mode != MODE_POINTS
		|| renderer.vertices.count + 1 > BATCH_MAX_VERTICES)
	{
		break_batch();

		renderer.current_texture = renderer.stub_texture;
		renderer.current_mode = MODE_POINTS;
	}

	point += vec2(0.5f, 0.5f);

	array_add(&renderer.vertices, {{point.x, point.y, 0.0f}, {}, color, {}});
}

void draw_rectangle_outline(Rectf rect, vec4 color) {
	rect.w -= 1;
	rect.h -= 1;

	vec2 pos = vec2(rect.x, rect.y);

	draw_line(pos, pos + vec2(rect.w, 0), color);
	draw_line(pos + vec2(0, rect.h), pos + vec2(rect.w, rect.h), color);

	draw_line(pos, pos + vec2(0, rect.h), color);
	draw_line(pos + vec2(rect.w, 0), pos + vec2(rect.w, rect.h), color);
}
