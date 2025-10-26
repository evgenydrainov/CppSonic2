#pragma once

#if defined(__VITA__)
	#define GL_GLEXT_PROTOTYPES
	#include <GLES2/gl2.h>
	#include <GLES2/gl2ext.h>

	#define GL_RGB8                GL_RGB8_OES
	#define GL_RGBA8               GL_RGBA8_OES
	#define GL_DEPTH_COMPONENT24   GL_DEPTH_COMPONENT24_OES
	#define glGenVertexArrays      glGenVertexArraysOES
	#define glBindVertexArray      glBindVertexArrayOES
	#define glDeleteVertexArrays   glDeleteVertexArraysOES
#elif defined(__ANDROID__)
	#include "gles32.h"
#elif defined(__EMSCRIPTEN__)
	#include "gles30.h"
#else
	#include "gl33.h"
#endif
