#include "window_creation.h"

#include "imgui_glue.h"

Window window;


#ifdef _DEBUG
static void GLAPIENTRY gl_debug_callback(GLenum source,
										 GLenum type,
										 unsigned int id,
										 GLenum severity,
										 GLsizei /*length*/,
										 const char *message,
										 const void * /*userParam*/) {
	// ignore non-significant error/warning codes
	// if (id == 131169 || id == 131185 || id == 131218 || id == 131204) {
	// 	return;
	// }

	log_info("---------------");
	log_info("Debug message (%u): %s", id, message);

	switch (source)
	{
		case GL_DEBUG_SOURCE_API:             log_info("Source: API"); break;
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   log_info("Source: Window System"); break;
		case GL_DEBUG_SOURCE_SHADER_COMPILER: log_info("Source: Shader Compiler"); break;
		case GL_DEBUG_SOURCE_THIRD_PARTY:     log_info("Source: Third Party"); break;
		case GL_DEBUG_SOURCE_APPLICATION:     log_info("Source: Application"); break;
		case GL_DEBUG_SOURCE_OTHER:           log_info("Source: Other"); break;
	}

	switch (type)
	{
		case GL_DEBUG_TYPE_ERROR:               log_info("Type: Error"); break;
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: log_info("Type: Deprecated Behaviour"); break;
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  log_info("Type: Undefined Behaviour"); break;
		case GL_DEBUG_TYPE_PORTABILITY:         log_info("Type: Portability"); break;
		case GL_DEBUG_TYPE_PERFORMANCE:         log_info("Type: Performance"); break;
		case GL_DEBUG_TYPE_MARKER:              log_info("Type: Marker"); break;
		case GL_DEBUG_TYPE_PUSH_GROUP:          log_info("Type: Push Group"); break;
		case GL_DEBUG_TYPE_POP_GROUP:           log_info("Type: Pop Group"); break;
		case GL_DEBUG_TYPE_OTHER:               log_info("Type: Other"); break;
	}

	switch (severity)
	{
		case GL_DEBUG_SEVERITY_HIGH:         log_info("Severity: high"); break;
		case GL_DEBUG_SEVERITY_MEDIUM:       log_info("Severity: medium"); break;
		case GL_DEBUG_SEVERITY_LOW:          log_info("Severity: low"); break;
		case GL_DEBUG_SEVERITY_NOTIFICATION: log_info("Severity: notification"); break;
	}

	// SDL_Window* win = SDL_GL_GetCurrentWindow();
	// SDL_ShowSimpleMessageBox(0, "", message, win);

	SDL_TriggerBreakpoint();
}
#endif




void init_window_and_opengl(const char* title,
							int width, int height, int init_window_scale,
							bool prefer_vsync) {
	// SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);

	SDL_SetHint("SDL_WINDOWS_DPI_AWARENESS", "system");

	if (SDL_Init(SDL_INIT_VIDEO
				 | SDL_INIT_AUDIO) != 0) {
		panic_and_abort("Couldn't initialize SDL: %s", SDL_GetError());
	}

	window.handle = SDL_CreateWindow(title,
									 SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
									 width * init_window_scale, height * init_window_scale,
									 SDL_WINDOW_OPENGL
									 | SDL_WINDOW_RESIZABLE);
	window.game_width  = width;
	window.game_height = height;

	if (!window.handle) {
		panic_and_abort("Couldn't create window: %s", SDL_GetError());
	}

	// 
	// A little workaround for Linux Mint Cinnamon.
	// 
#if defined(_DEBUG) && defined(__unix__)
	SDL_RaiseWindow(window.handle);
#endif


	SDL_SetWindowMinimumSize(window.handle, width, height);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

#ifdef _DEBUG
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif

	window.gl_context = SDL_GL_CreateContext(window.handle);
	SDL_GL_MakeCurrent(window.handle, window.gl_context);

	{
		int version = gladLoadGL([](const char* name) {
			return (GLADapiproc) SDL_GL_GetProcAddress(name);
		});

		if (version == 0) {
			panic_and_abort("Couldn't load OpenGL.");
		}
	}


#ifdef _DEBUG
	if (GLAD_GL_KHR_debug) {
		glEnable(GL_DEBUG_OUTPUT);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageCallback(gl_debug_callback, nullptr);
		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
	}
#endif

	// Decide on vsync.
	{
		window.vsync = prefer_vsync;

		char* env_use_vsync = SDL_getenv("USE_VSYNC"); // @Leak
		if (env_use_vsync) {
			window.vsync = (SDL_atoi(env_use_vsync) != 0);
		}
	}

	SDL_GL_SetSwapInterval(window.vsync ? 1 : 0);

	glDisable(GL_CULL_FACE);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glDisable(GL_DEPTH_TEST);
}

void deinit_window_and_opengl() {
	SDL_GL_DeleteContext(window.gl_context);
	window.gl_context = nullptr;

	SDL_DestroyWindow(window.handle);
	window.handle = nullptr;

	SDL_Quit();
}

static void handle_event(SDL_Event* ev) {
	switch (ev->type) {
		case SDL_QUIT: {
			window.should_quit = true;
			break;
		}

		case SDL_KEYDOWN: {
			SDL_Scancode scancode = ev->key.keysym.scancode;

			if (scancode >= 0 && scancode < window.NUM_KEYS) {
				bool ignore = false;

				// ignore alt+f4
				if (scancode == SDL_SCANCODE_F4 && (ev->key.keysym.mod & KMOD_ALT)) {
					ignore = true;
				}

				if (!ignore) {
					if (ev->key.repeat) {
						window.key_repeat[scancode / 32] |= 1 << (scancode % 32);
					} else {
						window.key_pressed[scancode / 32] |= 1 << (scancode % 32);
					}
				}
			}
			break;
		}
	}
}

void begin_frame() {
	if (!window.prev_time_is_initialized) {
		window.prev_time = get_time() - 1.0 / window.target_fps;

		window.prev_time_is_initialized = true;
	}

	double time = get_time();

	window.frame_end_time = time + (1.0 / window.target_fps);

	// Set delta.
	{
		window.delta = (float)((time - window.prev_time) * 60.0);

		// 
		// Don't go below 30 fps. No upper limit for now.
		// 
		float max_delta = 2.0f;
		window.delta = min(window.delta, max_delta);
	}

	window.fps = (float)(1.0 / (time - window.prev_time));

	window.prev_time = time;

	memset(window.key_pressed, 0, sizeof(window.key_pressed));
	memset(window.key_repeat,  0, sizeof(window.key_repeat));

	SDL_Event ev;
	while (SDL_PollEvent(&ev)) {
		// @Cleanup
		imgui_handle_event(&ev);

		handle_event(&ev);
	}
}

void swap_buffers() {
	SDL_GL_SwapWindow(window.handle);

	if (!window.vsync) {
		double time_left = window.frame_end_time - get_time();

		if (time_left > 0.0) {
			double sleep_time = time_left * 0.95;
			SDL_Delay((u32)(sleep_time * 1000.0));

			// spinlock
			while (get_time() < window.frame_end_time) {}
		}
	}
}


bool is_key_pressed(SDL_Scancode key, bool repeat) {
	bool result = false;

	if (!(key >= 0 && key < window.NUM_KEYS)) return result;

	/*if (!g->console.show)*/
	{
		result |= (window.key_pressed[key / 32] & (1 << (key % 32))) != 0;

		if (repeat) {
			result |= (window.key_repeat[key / 32] & (1 << (key % 32))) != 0;
		}
	}

	return result;
}

bool is_key_held(SDL_Scancode key) {
	bool result = false;

	if (!(key >= 0 && key < SDL_NUM_SCANCODES)) return result;

	/*if (!g->console.show)*/
	{
		const u8* state = SDL_GetKeyboardState(nullptr);

		result |= (state[key] != 0);
	}

	return result;
}

SDL_Window* get_window_handle() {
	return window.handle;
}

void set_fullscreen(bool fullscreen) {
	if (fullscreen) {
		SDL_DisplayMode mode;
		int display = SDL_GetWindowDisplayIndex(window.handle);
		SDL_GetDesktopDisplayMode(display, &mode);
		SDL_SetWindowDisplayMode(window.handle, &mode);
		SDL_SetWindowFullscreen(window.handle, SDL_WINDOW_FULLSCREEN);
	} else {
		SDL_SetWindowFullscreen(window.handle, 0);
	}
}

bool is_fullscreen() {
	u32 flags = SDL_GetWindowFlags(window.handle);
	return (flags & SDL_WINDOW_FULLSCREEN) != 0;
}

double get_time() {
	return (double)SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
}
