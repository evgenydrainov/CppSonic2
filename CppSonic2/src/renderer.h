#pragma once

#include "common.h"

/*
* A basic 2D batch renderer.
* 
* Call break_batch() before making raw OpenGL calls.
*/

constexpr size_t BATCH_MAX_QUADS    = 10'000;
constexpr size_t VERTICES_PER_QUAD  = 4;
constexpr size_t INDICES_PER_QUAD   = 6;
constexpr size_t BATCH_MAX_VERTICES = (BATCH_MAX_QUADS * VERTICES_PER_QUAD);
constexpr size_t BATCH_MAX_INDICES  = (BATCH_MAX_QUADS * INDICES_PER_QUAD);

struct Vertex {
	vec3 pos;
	vec3 normal;
	vec4 color;
	vec2 uv;
};

enum RenderMode {
	MODE_NONE,
	MODE_QUADS,
	MODE_TRIANGLES,
	MODE_LINES,
	MODE_POINTS,
	MODE_CIRCLES,
};

struct Texture {
	u32 id;
	int width;
	int height;
};

Texture load_texture(u8* pixel_data, int width, int height,
					 int filter = GL_NEAREST, int wrap = GL_CLAMP_TO_EDGE,
					 bool alpha_channel = true);

Texture load_depth_texture(int width, int height);

void free_texture(Texture* t);

struct Framebuffer {
	u32 id;
	Texture texture;
	Texture depth;
};

// NOTE: alpha channel is off by default
Framebuffer load_framebuffer(int width, int height,
							 int filter = GL_NEAREST, int wrap = GL_CLAMP_TO_EDGE,
							 bool alpha_channel = false, bool depth = true);

void free_framebuffer(Framebuffer* f);

struct Renderer {
	u32 current_texture;
	RenderMode current_mode;
	bump_array<Vertex> vertices;

	u32 texture_shader;  // These shaders should be handled by an asset system maybe
	u32 color_shader;
	u32 circle_shader;
	u32 sharp_bilinear_shader;

	u32 current_shader;

	u32 batch_vao;
	u32 batch_vbo;
	u32 batch_ebo;
	Texture texture_for_shapes; // 1x1 white texture

	Framebuffer framebuffer;  // Game is renderer to a framebuffer, and then the framebuffer is rendered to the screen.
	Rect game_texture_rect;

	int backbuffer_width;
	int backbuffer_height;

	mat4 proj_mat = {1};
	mat4 view_mat = {1};
	mat4 model_mat = {1};

	int draw_calls;
	size_t max_batch;
	int total_triangles;

	int curr_draw_calls;  // These values change during the frame, use draw_calls and max_batch for metrics
	size_t curr_max_batch;
	int curr_total_triangles;

	double draw_took;
	double draw_took_t;
};

extern Renderer renderer;

void init_renderer(); // assumes opengl is initialized
void deinit_renderer();

void render_begin_frame(vec4 clear_color);
void render_end_frame();

void break_batch(); // makes the draw call

void set_shader(u32 shader);
void reset_shader();

void set_proj_mat (const mat4& proj_mat);
void set_view_mat (const mat4& view_mat);
void set_model_mat(const mat4& model_mat);

void set_viewport(int x, int y, int width, int height);

void render_clear_color(vec4 color);

void draw_quad(const Texture& t, Vertex vertices[4]);

void draw_texture(const Texture& t, Rect src = {},
				  vec2 pos = {}, vec2 scale = {1, 1},
				  vec2 origin = {}, float angle = 0, vec4 color = color_white, glm::bvec2 flip = {});

// This version of the function is here because matrix multiplications are too slow in Debug build.
void draw_texture_simple(const Texture& t, Rect src = {},
						 vec2 pos = {}, vec2 origin = {}, vec4 color = color_white, glm::bvec2 flip = {});

void draw_texture_centered(const Texture& t,
						   vec2 pos = {}, vec2 scale = {1, 1},
						   float angle = 0, vec4 color = color_white, glm::bvec2 flip = {});

void draw_rectangle(Rectf rect, vec4 color);
void draw_rectangle(Rectf rect, vec2 scale,
					vec2 origin, float angle, vec4 color);

void draw_triangle(vec2 p1, vec2 p2, vec2 p3, vec4 color);

void draw_circle(vec2 pos, float radius, vec4 color, int precision = 12);

void draw_line(vec2 p1, vec2 p2, vec4 color);
void draw_line_exact(vec2 p1, vec2 p2, vec4 color);
void draw_line_thick(vec2 p1, vec2 p2, float thickness, vec4 color);

void draw_point(vec2 point, vec4 color);

void draw_rectangle_outline(Rectf rect, vec4 color);
void draw_rectangle_outline_exact(Rectf rect, vec4 color);
void draw_rectangle_outline_thick(Rectf rect, float thickness, vec4 color);

void draw_arrow_thick(vec2 p1, float length, float direction, float arrow_head_length, float thickness, vec4 color);
