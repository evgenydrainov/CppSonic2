#pragma warning(push, 0)


#define STB_SPRINTF_IMPLEMENTATION
#include <stb/stb_sprintf.h>
#undef STB_SPRINTF_IMPLEMENTATION


#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
// TODO: STBI_ASSERT
#include <stb/stb_image.h>
#undef STB_IMAGE_IMPLEMENTATION


#define GLAD_GL_IMPLEMENTATION
#define GLAD_GLES2_IMPLEMENTATION
#include <glad/gl.h>
#undef GLAD_GL_IMPLEMENTATION


#pragma warning(pop)


