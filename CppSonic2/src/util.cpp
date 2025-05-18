#include "util.h"

#include "package.h"

u32 compile_shader(GLenum type, string source, const char* debug_name) {
	u32 shader = glCreateShader(type);

#if defined(__ANDROID__)
	string version_string = "#version 320 es\n";
#elif defined(__EMSCRIPTEN__)
	string version_string = "#version 300 es\n";
#else
	string version_string = "#version 330 core\n";
#endif

	string precision_string = "#ifdef GL_ES\n"
		"precision highp float;\n"
		"#endif\n";

	const char* sources[] = {version_string.data, precision_string.data, source.data};
	int lengths[] = {version_string.count, precision_string.count, source.count};
	glShaderSource(shader, ArrayLength(sources), sources, lengths);

	glCompileShader(shader);

	int success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		char buf[512];
		glGetShaderInfoLog(shader, sizeof(buf), NULL, buf);

		if (debug_name) log_error("While compiling %s...", debug_name);
		log_error("Shader Compile Error:\n%s", buf);
	}

	return shader;
}

u32 link_program(u32 vertex_shader, u32 fragment_shader, const char* debug_name) {
	u32 program = glCreateProgram();

	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);

	glLinkProgram(program);
	// TODO: glValidateProgram(program);

	int success;
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success) {
		char buf[512];
		glGetProgramInfoLog(program, sizeof(buf), NULL, buf);

		if (debug_name) log_error("While linking %s...", debug_name);
		log_error("Shader Link Error:\n%s", buf);
	}

	return program;
}

u32 create_vertex_array_obj(array<Vertex> vertices,
							array<u32> indices,
							u32* out_vbo, u32* out_ebo) {
	u32 vao = 0;
	u32 vbo = 0;
	u32 ebo = 0;

	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	if (indices.count > 0) glGenBuffers(1, &ebo);

	glBindVertexArray(vao);

	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, vertices.count * sizeof(vertices[0]), vertices.data, GL_STATIC_DRAW);

	if (indices.count > 0) {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.count * sizeof(indices[0]), indices.data, GL_STATIC_DRAW);
	}

	set_vertex_attribs();

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	if (out_vbo) *out_vbo = vbo;
	if (out_ebo) *out_ebo = ebo;
	return vao;
}

Shader load_shader_from_file(const char* vert_fname, const char* frag_fname) {
	string vertex_source = get_file_str(vert_fname);
	u32 vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_source, vert_fname);
	defer { glDeleteShader(vertex_shader); };

	string fragment_source = get_file_str(frag_fname);
	u32 fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_source, frag_fname);
	defer { glDeleteShader(fragment_shader); };

	Shader shader = {};
	shader.id = link_program(vertex_shader, fragment_shader, vert_fname);

	return shader;
}

bump_array<Vertex> load_3d_model_from_obj_file(const char* fname) {
	const size_t NUM_POSITIONS = 10'000;
	const size_t NUM_UVS       = 10'000;
	const size_t NUM_NORMALS   = 10'000;
	const size_t NUM_VERTICES  = 10'000;

	auto positions = malloc_bump_array<vec3>(NUM_POSITIONS);
	defer { free(positions.data); };

	auto uvs = malloc_bump_array<vec2>(NUM_UVS);
	defer { free(uvs.data); };

	auto normals = malloc_bump_array<vec3>(NUM_NORMALS);
	defer { free(normals.data); };

	auto vertices = malloc_bump_array<Vertex>(NUM_VERTICES);

	string text = get_file_str(fname);

	while (text.count > 0) {
		string line = eat_line(&text);

		if (line.count == 0) {
			continue;
		}

		if (line[0] == '#') {
			continue;
		}

		eat_whitespace(&line);
		string s = eat_non_whitespace(&line);

		if (s == "v") {
			eat_whitespace(&line);
			string x = eat_non_whitespace(&line);

			eat_whitespace(&line);
			string y = eat_non_whitespace(&line);

			eat_whitespace(&line);
			string z = eat_non_whitespace(&line);

			vec3 pos;
			pos.x = string_to_f32(x);
			pos.y = string_to_f32(y);
			pos.z = string_to_f32(z);

			array_add(&positions, pos);
		} else if (s == "vn") {
			eat_whitespace(&line);
			string x = eat_non_whitespace(&line);

			eat_whitespace(&line);
			string y = eat_non_whitespace(&line);

			eat_whitespace(&line);
			string z = eat_non_whitespace(&line);

			vec3 normal;
			normal.x = string_to_f32(x);
			normal.y = string_to_f32(y);
			normal.z = string_to_f32(z);

			array_add(&normals, normal);
		} else if (s == "vt") {
			eat_whitespace(&line);
			string u = eat_non_whitespace(&line);

			eat_whitespace(&line);
			string v = eat_non_whitespace(&line);

			vec2 uv;
			uv.x = string_to_f32(u);
			uv.y = string_to_f32(v);

			array_add(&uvs, uv);
		} else if (s == "f") {
			eat_whitespace(&line);
			string vert1 = eat_non_whitespace(&line);

			eat_whitespace(&line);
			string vert2 = eat_non_whitespace(&line);

			eat_whitespace(&line);
			string vert3 = eat_non_whitespace(&line);

			auto add_vert = [&](string vert) {
				string pos = eat_numeric(&vert);
				advance(&vert); // Skip '/'

				string uv = eat_numeric(&vert);
				advance(&vert); // Skip '/'

				string normal = eat_numeric(&vert);

				Vertex v;
				v.pos    = positions[string_to_u32(pos)    - 1];
				v.normal = normals  [string_to_u32(normal) - 1];
				v.uv     = uvs      [string_to_u32(uv)     - 1];
				v.color  = color_white;

				array_add(&vertices, v);
			};

			add_vert(vert1);
			add_vert(vert2);
			add_vert(vert3);
		}
	}

	return vertices;
}
