#include "renderer.h"

#include "package.h"
#include "window_creation.h"
#include "util.h"

Renderer renderer = {};


static char texture_vert_shader_src[] =
R"(layout(location = 0) in vec3 in_Position;
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



static char texture_frag_shader_src[] =
R"(layout(location = 0) out vec4 FragColor;

in vec4 v_Color;
in vec2 v_TexCoord;

uniform sampler2D u_Texture;

void main() {
	vec4 color = texture(u_Texture, v_TexCoord);

	FragColor = color * v_Color;
}
)";



static char circle_frag_shader_src[] =
R"(layout(location = 0) out vec4 FragColor;

in vec4 v_Color;
in vec2 v_TexCoord;

uniform sampler2D u_Texture;

void main() {
	vec4 color = texture(u_Texture, v_TexCoord);

	vec2 coord = v_TexCoord * 2.0 - 1.0;

	if (coord.x * coord.x + coord.y * coord.y >= 1.0) {
		discard;
	}

	FragColor = color * v_Color;
}
)";



static char sharp_bilinear_frag_shader_src[] =
R"(/*
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


static char hq4x_frag_shader_src[] =
R"(layout(location = 0) out vec4 FragColor;

in vec4 v_Color;
in vec2 v_TexCoord;

uniform sampler2D u_Texture;

uniform vec2 u_TexelSize;

void main()
{
	vec2 dg1 = u_TexelSize * 0.5;
	vec2 dg2 = vec2(-dg1.x, dg1.y);
	vec2 sd1 = dg1 * 0.5;
	vec2 sd2 = dg2 * 0.5;
	
	vec4 c  = texture(u_Texture, v_TexCoord);
	vec4 i1 = texture(u_Texture, v_TexCoord - sd1);
	vec4 i2 = texture(u_Texture, v_TexCoord - sd2);
	vec4 i3 = texture(u_Texture, v_TexCoord + sd1);
	vec4 i4 = texture(u_Texture, v_TexCoord + sd2);
	vec4 o1 = texture(u_Texture, v_TexCoord - dg1);
	vec4 o3 = texture(u_Texture, v_TexCoord + dg1);
	vec4 o2 = texture(u_Texture, v_TexCoord - dg2);
	vec4 o4 = texture(u_Texture, v_TexCoord + dg2);
	
	float ko1 = dot(abs(o1 - c), vec4(1.0));
	float ko2 = dot(abs(o2 - c), vec4(1.0));
	float ko3 = dot(abs(o3 - c), vec4(1.0));
	float ko4 = dot(abs(o4 - c), vec4(1.0));
	
	float k1 = min(dot(abs(i1 - i3), vec4(1.0)), max(ko1, ko3));
	float k2 = min(dot(abs(i2 - i4), vec4(1.0)), max(ko2, ko4));
	
	float w1 = k2; if (ko3 < ko1) w1 *= ko3 / ko1;
	float w2 = k1; if (ko4 < ko2) w2 *= ko4 / ko2;
	float w3 = k2; if (ko1 < ko3) w3 *= ko1 / ko3;
	float w4 = k1; if (ko2 < ko4) w4 *= ko2 / ko4;
	
	FragColor = (w1 * o1 + w2 * o2 + w3 * o3 + w4 * o4 + 0.001 * c) / (w1 + w2 + w3 + w4 + 0.001) * v_Color;
}
)";



static int get_internal_format_from_gl_format(u32 gl_format) {
	switch (gl_format) {
		case GL_RGB:  return GL_RGB8;
		case GL_RGBA: return GL_RGBA8;
	}
	Assert(!"format not supported");
}

Texture load_texture(u8* pixel_data, int width, int height,
					 int filter, int wrap,
					 u32 gl_format) {
	Texture t = {};
	t.width = width;
	t.height = height;

	glGenTextures(1, &t.id);
	glBindTexture(GL_TEXTURE_2D, t.id);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);

	int internal_format = get_internal_format_from_gl_format(gl_format);
	glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, gl_format, GL_UNSIGNED_BYTE, pixel_data);

	glBindTexture(GL_TEXTURE_2D, 0);
	return t;
}

Texture load_depth_texture(int width, int height) {
	Texture t = {};
	t.width = width;
	t.height = height;

	glGenTextures(1, &t.id);
	glBindTexture(GL_TEXTURE_2D, t.id);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

	glBindTexture(GL_TEXTURE_2D, 0);
	return t;
}

void free_texture(Texture* t) {
	if (t->id != 0) glDeleteTextures(1, &t->id);
	*t = {};
}

Framebuffer load_framebuffer(int width, int height,
							 int filter, int wrap,
							 u32 gl_format, bool depth_texture) {
	Framebuffer f = {};
	f.texture = load_texture(nullptr, width, height, filter, wrap, gl_format);
	if (depth_texture) {
		f.depth = load_depth_texture(width, height);
	}

	glGenFramebuffers(1, &f.id);

	glBindFramebuffer(GL_FRAMEBUFFER, f.id);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, f.texture.id, 0);
	if (depth_texture) {
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, f.depth.id, 0);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return f;
}

void free_framebuffer(Framebuffer* f) {
	free_texture(&f->texture);
	free_texture(&f->depth);

	if (f->id != 0) glDeleteFramebuffers(1, &f->id);
	*f = {};
}

void free_shader(Shader* s) {
	if (s->id != 0) glDeleteProgram(s->id);
	*s = {};
}

void set_vertex_attribs() {
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
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); // ebo must be unbound after vao

		renderer.vertices = malloc_bump_array<Vertex>(BATCH_MAX_VERTICES);

		u8 pixel_data[] = {255, 255, 255, 255};
		renderer.texture_for_shapes = load_texture(pixel_data, 1, 1, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_RGBA);

		renderer.framebuffer = load_framebuffer(window.game_width, window.game_height, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_RGB, false);
	}

	// 
	// Load shaders.
	// 
	{
		u32 texture_vert_shader = compile_shader(GL_VERTEX_SHADER, texture_vert_shader_src, "texture_vert");
		defer { glDeleteShader(texture_vert_shader); };

		u32 texture_frag_shader = compile_shader(GL_FRAGMENT_SHADER, texture_frag_shader_src, "texture_frag");
		defer { glDeleteShader(texture_frag_shader); };

		u32 circle_frag_shader = compile_shader(GL_FRAGMENT_SHADER, circle_frag_shader_src, "circle_frag");
		defer { glDeleteShader(circle_frag_shader); };

		u32 sharp_bilinear_frag_shader = compile_shader(GL_FRAGMENT_SHADER, sharp_bilinear_frag_shader_src, "sharp_bilinear_frag");
		defer { glDeleteShader(sharp_bilinear_frag_shader); };

		u32 hq4x_frag_shader = compile_shader(GL_FRAGMENT_SHADER, hq4x_frag_shader_src, "hq4x_frag");
		defer { glDeleteShader(hq4x_frag_shader); };

		renderer.texture_shader.id        = link_program(texture_vert_shader, texture_frag_shader,        "texture_shader");
		renderer.circle_shader.id         = link_program(texture_vert_shader, circle_frag_shader,         "circle_shader");
		renderer.sharp_bilinear_shader.id = link_program(texture_vert_shader, sharp_bilinear_frag_shader, "sharp_bilinear_shader");
		renderer.hq4x_shader.id           = link_program(texture_vert_shader, hq4x_frag_shader,           "hq4x_shader");

		renderer.current_shader = renderer.texture_shader.id;
		glUseProgram(renderer.current_shader);
	}
}

void deinit_renderer() {
	free(renderer.vertices.data);

	glDeleteBuffers(1, &renderer.batch_ebo);
	glDeleteBuffers(1, &renderer.batch_vbo);
	glDeleteVertexArrays(1, &renderer.batch_vao);

	free_texture(&renderer.texture_for_shapes);
	free_framebuffer(&renderer.framebuffer);
}

void render_begin_frame(vec4 clear_color) {
	Assert(renderer.vertices.count == 0);

	renderer.draw_took_t = get_time();

	renderer.draw_calls      = renderer.curr_draw_calls;
	renderer.max_batch       = renderer.curr_max_batch;
	renderer.total_triangles = renderer.curr_total_triangles;

	renderer.curr_draw_calls      = 0;
	renderer.curr_max_batch       = 0;
	renderer.curr_total_triangles = 0;

	glBindFramebuffer(GL_FRAMEBUFFER, renderer.framebuffer.id);
	glViewport(0, 0, renderer.framebuffer.texture.width, renderer.framebuffer.texture.height);

	if (clear_color.a > 0) {
		glClearColor(clear_color.r, clear_color.g, clear_color.b, clear_color.a);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	// setup default matrices
	renderer.proj_mat = get_ortho(0, renderer.framebuffer.texture.width, renderer.framebuffer.texture.height, 0);
	renderer.view_mat = get_identity();
	renderer.model_mat = get_identity();
}

void render_end_frame() {
	break_batch();

	int backbuffer_width;
	int backbuffer_height;
	SDL_GL_GetDrawableSize(window.handle, &backbuffer_width, &backbuffer_height);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, backbuffer_width, backbuffer_height);

	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	renderer.proj_mat = get_ortho(0, backbuffer_width, backbuffer_height, 0);
	renderer.view_mat = get_identity();
	renderer.model_mat = get_identity();

	{
		u32 program = renderer.sharp_bilinear_shader.id;
		set_shader(program);

		float xscale = backbuffer_width  / (float)window.game_width;
		float yscale = backbuffer_height / (float)window.game_height;
		float scale = fminf(xscale, yscale);

		renderer.game_texture_rect.w = (int) (window.game_width  * scale);
		renderer.game_texture_rect.h = (int) (window.game_height * scale);
		renderer.game_texture_rect.x = (backbuffer_width  - renderer.game_texture_rect.w) / 2;
		renderer.game_texture_rect.y = (backbuffer_height - renderer.game_texture_rect.h) / 2;

		renderer.backbuffer_width  = backbuffer_width;
		renderer.backbuffer_height = backbuffer_height;

		{
			int u_SourceSize = glGetUniformLocation(program, "u_SourceSize");
			glUniform2f(u_SourceSize, (float)window.game_width, (float)window.game_height);
		}

		{
			float int_scale = fmaxf(floorf(scale), 1.0f);

			int u_Scale = glGetUniformLocation(program, "u_Scale");
			glUniform2f(u_Scale, int_scale, int_scale);
		}

		/*{
			int u_TexelSize = glGetUniformLocation(program, "u_TexelSize");
			glUniform2f(u_TexelSize, 1.0f / (float)window.game_width, 1.0f / (float)window.game_height);
		}*/

		draw_texture(renderer.framebuffer.texture, {}, {(float)renderer.game_texture_rect.x, (float)renderer.game_texture_rect.y}, {scale, scale}, {}, 0, color_white, {false, true});

		break_batch();

		reset_shader();
	}

	// glFinish();

	renderer.draw_took = get_time() - renderer.draw_took_t;
}

static void setup_uniforms(u32 program) {
	int u_MVP = glGetUniformLocation(program, "u_MVP");
	if (u_MVP != -1) {
		mat4 MVP = (renderer.proj_mat * renderer.view_mat) * renderer.model_mat;

		glUniformMatrix4fv(u_MVP, 1, GL_FALSE, &MVP[0][0]);
	}

	int u_ModelView = glGetUniformLocation(program, "u_ModelView");
	if (u_ModelView != -1) {
		mat4 model_view = renderer.view_mat * renderer.model_mat;

		glUniformMatrix4fv(u_ModelView, 1, GL_FALSE, &model_view[0][0]);
	}

	int u_Model = glGetUniformLocation(program, "u_Model");
	if (u_Model != -1) {
		glUniformMatrix4fv(u_Model, 1, GL_FALSE, &renderer.model_mat[0][0]);
	}

	int u_View = glGetUniformLocation(program, "u_View");
	if (u_View != -1) {
		glUniformMatrix4fv(u_Model, 1, GL_FALSE, &renderer.view_mat[0][0]);
	}

	int u_Proj = glGetUniformLocation(program, "u_Proj");
	if (u_Proj != -1) {
		glUniformMatrix4fv(u_Proj, 1, GL_FALSE, &renderer.proj_mat[0][0]);
	}

	int u_Texture = glGetUniformLocation(program, "u_Texture");
	if (u_Texture != -1) {
		glUniform1i(u_Texture, 0);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, renderer.current_texture);
	}
}

void break_batch() {
	if (renderer.vertices.count == 0) {
		return;
	}

	Assert(renderer.current_mode != MODE_NONE);
	Assert(renderer.current_texture != 0);

	// upload vertices to gpu
	glBindBuffer(GL_ARRAY_BUFFER, renderer.batch_vbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, renderer.vertices.count * sizeof(Vertex), renderer.vertices.data);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	u32 program = (renderer.current_mode == MODE_CIRCLES) ? renderer.circle_shader.id : renderer.current_shader;

	glUseProgram(program);
	defer { glUseProgram(0); };

	setup_uniforms(program);

	glBindVertexArray(renderer.batch_vao);
	defer { glBindVertexArray(0); };

	u32 gl_mode = 0;
	bool use_index_buffer = false;

	switch (renderer.current_mode) {
		case MODE_QUADS: {
			Assert(renderer.vertices.count % 4 == 0);
			gl_mode = GL_TRIANGLES;
			use_index_buffer = true;
			renderer.curr_total_triangles += renderer.vertices.count / 4 * 2;
			break;
		}

		case MODE_TRIANGLES: {
			Assert(renderer.vertices.count % 3 == 0);
			gl_mode = GL_TRIANGLES;
			renderer.curr_total_triangles += renderer.vertices.count / 3;
			break;
		}

		case MODE_LINES: {
			Assert(renderer.vertices.count % 2 == 0);
			gl_mode = GL_LINES;
			break;
		}

		case MODE_POINTS: {
			Assert(renderer.vertices.count % 1 == 0);
			gl_mode = GL_POINTS;
			break;
		}

		case MODE_CIRCLES: {
			Assert(renderer.vertices.count % 4 == 0);
			gl_mode = GL_TRIANGLES;
			use_index_buffer = true;
			renderer.curr_total_triangles += renderer.vertices.count / 4 * 2;
			break;
		}
	}

	if (use_index_buffer) {
		int num_indices = renderer.vertices.count / 4 * 6;
		glDrawElements(gl_mode, num_indices, GL_UNSIGNED_INT, NULL);
	} else {
		glDrawArrays(gl_mode, 0, renderer.vertices.count);
	}

	renderer.curr_draw_calls++;
	renderer.curr_max_batch = max(renderer.curr_max_batch, renderer.vertices.count);

	renderer.vertices.count = 0;
	renderer.current_texture = 0;
	renderer.current_mode = MODE_NONE;
}

void set_shader(u32 shader) {
	if (shader == 0) {
		log_error("Trying to set invalid shader.");
		return;
	}

	if (renderer.current_shader != shader) {
		break_batch();
		renderer.current_shader = shader;

		// @Cleanup: to be able to set uniforms
		glUseProgram(renderer.current_shader);
	}
}

void reset_shader() {
	u32 shader = renderer.texture_shader.id;

	if (renderer.current_shader != shader) {
		break_batch();
		renderer.current_shader = shader;

		// @Cleanup: to be able to set uniforms
		glUseProgram(renderer.current_shader);
	}
}

void set_render_target(const Framebuffer& f) {
	break_batch();
	glBindFramebuffer(GL_FRAMEBUFFER, f.id);
	glViewport(0, 0, f.texture.width, f.texture.height);

	renderer.proj_mat = get_ortho(0, f.texture.width, f.texture.height, 0);
}

void reset_render_target() {
	break_batch();
	glBindFramebuffer(GL_FRAMEBUFFER, renderer.framebuffer.id);
	glViewport(0, 0, renderer.framebuffer.texture.width, renderer.framebuffer.texture.height);

	renderer.proj_mat = get_ortho(0, renderer.framebuffer.texture.width, renderer.framebuffer.texture.height, 0);
}

void set_proj_mat(const mat4& proj_mat) {
	break_batch();
	renderer.proj_mat = proj_mat;
}

void set_view_mat(const mat4& view_mat) {
	break_batch();
	renderer.view_mat = view_mat;
}

void set_model_mat(const mat4& model_mat) {
	break_batch();
	renderer.model_mat = model_mat;
}

void set_viewport(int x, int y, int width, int height) {
	break_batch();
	glViewport(x, y, width, height);
}

void render_clear_color(vec4 color) {
	break_batch();
	glClearColor(color.r, color.g, color.b, color.a);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

static void push_vertices(RenderMode mode, const Texture& t, array<Vertex> vertices) {
	if (t.id == 0) {
		log_error("Trying to draw invalid texture.");
		return;
	}

	if (t.id != renderer.current_texture
		|| renderer.current_mode != mode
		|| renderer.vertices.count + vertices.count > BATCH_MAX_VERTICES)
	{
		break_batch();

		renderer.current_texture = t.id;
		renderer.current_mode = mode;
	}

	For (it, vertices) array_add(&renderer.vertices, *it);
}

void draw_quad(const Texture& t, Vertex vertices[4]) {
	push_vertices(MODE_QUADS, t, array<Vertex>{vertices, 4});
}

void draw_texture(const Texture& t, Rect src,
				  vec2 pos, vec2 scale,
				  vec2 origin, float angle, vec4 color, glm::bvec2 flip) {

	#ifdef RENDERER_DRAW_AT_FLOORED_POS
		pos = floor(pos);
	#endif

	if (src.w == 0 && src.h == 0) {
		src.w = t.width;
		src.h = t.height;
	}

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

	mat4 model = glm::translate(mat4{1.0f}, {pos.x, pos.y, 0.0f});
	model = glm::rotate(model, glm::radians(-angle), {0.0f, 0.0f, 1.0f});
	model = glm::scale(model, {scale.x, scale.y, 1.0f});

	vertices[0].pos = model * vec4{vertices[0].pos, 1.0f};
	vertices[1].pos = model * vec4{vertices[1].pos, 1.0f};
	vertices[2].pos = model * vec4{vertices[2].pos, 1.0f};
	vertices[3].pos = model * vec4{vertices[3].pos, 1.0f};

	draw_quad(t, vertices);
}

void draw_texture_simple(const Texture& t, Rect src,
						 vec2 pos, vec2 origin, vec4 color, glm::bvec2 flip) {
	if (src.w == 0 && src.h == 0) {
		src.w = t.width;
		src.h = t.height;
	}

	float x1 = pos.x - origin.x;
	float y1 = pos.y - origin.y;
	float x2 = pos.x + src.w - origin.x;
	float y2 = pos.y + src.h - origin.y;

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

	draw_quad(t, vertices);
}

void draw_texture_centered(const Texture& t,
						   vec2 pos, vec2 scale,
						   float angle, vec4 color, glm::bvec2 flip) {
	vec2 origin = {t.width / 2.0f, t.height / 2.0f};
	draw_texture(t, {}, pos, scale, origin, angle, color, flip);
}

void draw_rectangle(Rectf rect, vec4 color) {
	vec2 pos = {rect.x, rect.y};
	vec2 scale = {rect.w, rect.h};
	draw_texture(renderer.texture_for_shapes, {}, pos, scale, {}, 0, color);
}

void draw_rectangle(Rectf rect, vec2 scale,
					vec2 origin, float angle, vec4 color) {
	vec2 pos = {rect.x, rect.y};

	origin.x /= rect.w;
	origin.y /= rect.h;

	rect.w *= scale.x;
	rect.h *= scale.y;

	draw_texture(renderer.texture_for_shapes, {}, pos, {rect.w, rect.h}, origin, angle, color);
}

void draw_triangle(vec2 p1, vec2 p2, vec2 p3, vec4 color) {
	Vertex vertices[] = {
		{{p1.x, p1.y, 0.0f}, {}, color, {}},
		{{p2.x, p2.y, 0.0f}, {}, color, {}},
		{{p3.x, p3.y, 0.0f}, {}, color, {}},
	};

	push_vertices(MODE_TRIANGLES, renderer.texture_for_shapes, vertices);
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
	Vertex vertices[] = {
		{{pos.x - radius, pos.y - radius, 0.0f}, {}, color, {0.0f, 0.0f}}, // LT
		{{pos.x + radius, pos.y - radius, 0.0f}, {}, color, {1.0f, 0.0f}}, // RT
		{{pos.x + radius, pos.y + radius, 0.0f}, {}, color, {1.0f, 1.0f}}, // RB
		{{pos.x - radius, pos.y + radius, 0.0f}, {}, color, {0.0f, 1.0f}}, // LB
	};

	push_vertices(MODE_CIRCLES, renderer.texture_for_shapes, vertices);
#endif
}

void draw_line(vec2 p1, vec2 p2, vec4 color) {
	// opengl diamond rule
	p1 += vec2{0.5f, 0.5f};
	p2 += vec2{0.5f, 0.5f};

	/* stolen from SDL2 */
	/* bump a little in the direction we are moving in. */
	float deltax = p2.x - p1.x;
	float deltay = p2.y - p1.y;
	float angle = atan2f(deltay, deltax);
	p2.x += cosf(angle) * 0.50f;
	p2.y += sinf(angle) * 0.50f;

	Vertex vertices[] = {
		{{p1.x, p1.y, 0.0f}, {}, color, {}},
		{{p2.x, p2.y, 0.0f}, {}, color, {}},
	};

	push_vertices(MODE_LINES, renderer.texture_for_shapes, vertices);
}

void draw_line_exact(vec2 p1, vec2 p2, vec4 color) {
	Vertex vertices[] = {
		{{p1.x, p1.y, 0.0f}, {}, color, {}},
		{{p2.x, p2.y, 0.0f}, {}, color, {}},
	};

	push_vertices(MODE_LINES, renderer.texture_for_shapes, vertices);
}

void draw_line_thick(vec2 p1, vec2 p2, float thickness, vec4 color) {
	float dir = point_direction(p1, p2);

	const float sqrt2_over_2 = 0.70710678118654752440084436210485f;

	vec2 lt = p1 + lengthdir_v2(thickness * sqrt2_over_2, dir - 135);
	vec2 rt = p1 + lengthdir_v2(thickness * sqrt2_over_2, dir + 135);
	vec2 lb = p2 + lengthdir_v2(thickness * sqrt2_over_2, dir - 45);
	vec2 rb = p2 + lengthdir_v2(thickness * sqrt2_over_2, dir + 45);

	Vertex vertices[] = {
		{{lt.x, lt.y, 0.0f}, {}, color, {}}, // LT
		{{rt.x, rt.y, 0.0f}, {}, color, {}}, // RT
		{{rb.x, rb.y, 0.0f}, {}, color, {}}, // RB
		{{lb.x, lb.y, 0.0f}, {}, color, {}}, // LB
	};

	draw_quad(renderer.texture_for_shapes, vertices);
}

void draw_point(vec2 point, vec4 color) {
	// opengl diamond rule
	point += vec2{0.5f, 0.5f};

	Vertex vertices[] = {
		{{point.x, point.y, 0.0f}, {}, color, {}},
	};

	push_vertices(MODE_POINTS, renderer.texture_for_shapes, vertices);
}

void draw_rectangle_outline(Rectf rect, vec4 color) {
	rect.w -= 1;
	rect.h -= 1;

	vec2 pos = {rect.x, rect.y};

	draw_line(pos, pos + vec2{rect.w, 0}, color);
	draw_line(pos + vec2{0, rect.h}, pos + vec2{rect.w, rect.h}, color);

	draw_line(pos, pos + vec2{0, rect.h}, color);
	draw_line(pos + vec2{rect.w, 0}, pos + vec2{rect.w, rect.h}, color);
}

void draw_rectangle_outline_exact(Rectf rect, vec4 color) {
	vec2 pos = {rect.x, rect.y};

	draw_line_exact(pos, pos + vec2{rect.w, 0}, color);
	draw_line_exact(pos + vec2{0, rect.h}, pos + vec2{rect.w, rect.h}, color);

	draw_line_exact(pos, pos + vec2{0, rect.h}, color);
	draw_line_exact(pos + vec2{rect.w, 0}, pos + vec2{rect.w, rect.h}, color);
}

void draw_rectangle_outline_thick(Rectf rect, float thickness, vec4 color) {
	vec2 pos = {rect.x, rect.y};

	draw_line_thick(pos, pos + vec2{rect.w, 0}, thickness, color);
	draw_line_thick(pos + vec2{0, rect.h}, pos + vec2{rect.w, rect.h}, thickness, color);

	draw_line_thick(pos, pos + vec2{0, rect.h}, thickness, color);
	draw_line_thick(pos + vec2{rect.w, 0}, pos + vec2{rect.w, rect.h}, thickness, color);
}

void draw_arrow_thick(vec2 p1, float length, float direction, float arrow_head_length, float thickness, vec4 color) {
	vec2 p2 = p1 + lengthdir_v2(length, direction);

	draw_line_thick(p1, p2 - lengthdir_v2(0.5f, direction), thickness, color);

	vec2 p3 = p2 + lengthdir_v2(arrow_head_length, direction + 135);
	draw_line_thick(p2, p3, thickness, color);

	vec2 p4 = p2 + lengthdir_v2(arrow_head_length, direction - 135);
	draw_line_thick(p2, p4, thickness, color);
}
