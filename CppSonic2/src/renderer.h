#pragma once

#include "common.h"

/*
* A basic 2D batch renderer.
* 
* Call break_batch() before making raw OpenGL calls.
*/

#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
constexpr size_t BATCH_MAX_QUADS    = 1'000;
#else
constexpr size_t BATCH_MAX_QUADS    = 10'000;
#endif

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
					 int filter, int wrap,
					 u32 gl_format);

Texture load_depth_texture(int width, int height);

void free_texture(Texture* t);

struct Framebuffer {
	u32 id;
	Texture texture;
	Texture depth;
};

Framebuffer load_framebuffer(int width, int height,
							 int filter, int wrap,
							 u32 gl_format, bool depth_texture);

void free_framebuffer(Framebuffer* f);

struct Shader {
	u32 id;
};

void free_shader(Shader* s);

struct Renderer {
	u32 current_texture;
	RenderMode current_mode;
	u32 current_shader;
	bump_array<Vertex> vertices;

	Shader texture_shader;  // These shaders should be handled by an asset system maybe
	Shader circle_shader;
	Shader sharp_bilinear_shader;
	Shader hq4x_shader;

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

void set_vertex_attribs();

void init_renderer(); // assumes opengl is initialized
void deinit_renderer();

void render_begin_frame(vec4 clear_color);
void render_end_frame();

void break_batch(); // makes the draw call

void set_shader(u32 shader);
void reset_shader();

void set_render_target(const Framebuffer& f);
void reset_render_target();

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
