#pragma once

#if defined(__ANDROID__)
	#include "gles32.h"
#elif defined(__EMSCRIPTEN__)
	#include "gles30.h"
#else
	#include "gles20.h"
#endif
