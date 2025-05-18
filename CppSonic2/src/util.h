#pragma once

#include "common.h"
#include "renderer.h"

u32 compile_shader(GLenum type, string source, const char* debug_name = nullptr);

u32 link_program(u32 vertex_shader, u32 fragment_shader, const char* debug_name = nullptr);

u32 create_vertex_array_obj(array<Vertex> vertices,
							array<u32> indices = {},
							u32* out_vbo = nullptr, u32* out_ebo = nullptr);

Shader load_shader_from_file(const char* vert_fname, const char* frag_fname);

// result must be free()'d
bump_array<Vertex> load_3d_model_from_obj_file(const char* fname);
