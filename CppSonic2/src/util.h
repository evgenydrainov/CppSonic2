#pragma once

#include "common.h"

inline u32 compile_shader(GLenum type, string source, const char* debug_name = nullptr) {
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

inline u32 link_program(u32 vertex_shader, u32 fragment_shader, const char* debug_name = nullptr) {
	u32 program = glCreateProgram();

	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);

	glLinkProgram(program);

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
