#pragma once

#include "common.h"

inline u32 compile_shader(GLenum type, const char* source) {
	u32 shader = glCreateShader(type);

	const char* sources[] = {source};
	glShaderSource(shader, ArrayLength(sources), sources, NULL);

	glCompileShader(shader);

	int success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		char buf[512];
		glGetShaderInfoLog(shader, sizeof(buf), NULL, buf);
		log_error("Shader Compile Error: %s", buf);
	}

	return shader;
}

inline u32 link_program(u32 vertex_shader, u32 fragment_shader) {
	u32 program = glCreateProgram();

	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);

	glLinkProgram(program);

	int success;
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success) {
		char buf[512];
		glGetProgramInfoLog(program, sizeof(buf), NULL, buf);
		log_error("Shader Link Error: %s", buf);
	}

	return program;
}
