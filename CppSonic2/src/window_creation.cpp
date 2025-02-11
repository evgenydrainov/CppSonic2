#include "window_creation.h"

// @Cleanup?
#include "renderer.h"
#include "console.h"

#include <SDL_system.h>

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

	// SDL_TriggerBreakpoint();
}
#endif




void init_window_and_opengl(const char* title,
							int width, int height, int init_window_scale,
							bool prefer_vsync, bool prefer_borderless_fullscreen) {
	// SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);

	window.perf_counter_when_started = SDL_GetPerformanceCounter();
	window.perf_frequency = SDL_GetPerformanceFrequency();
	window.perf_frequency_double = (double)window.perf_frequency;

	// SDL_SetHint("SDL_WINDOWS_DPI_AWARENESS", "permonitorv2");
	SDL_SetHint("SDL_WINDOWS_DPI_SCALING", "1");

	SDL_SetHint("SDL_IOS_ORIENTATIONS", "LandscapeLeft");

	//SDL_SetHint("SDL_ANDROID_TRAP_BACK_BUTTON", "1");

	if (SDL_Init(SDL_INIT_VIDEO
				 | SDL_INIT_GAMECONTROLLER) != 0) {
		panic_and_abort("Couldn't initialize SDL: %s", SDL_GetError());
	}

	log_info("Platform: %s", SDL_GetPlatform());
	log_info("sizeof(void*): %d", (int) sizeof(void*));

	{
		SDL_version ver;

		SDL_VERSION(&ver);
		log_info("Compiled against SDL %u.%u.%u", ver.major, ver.minor, ver.patch);

		SDL_GetVersion(&ver);
		log_info("Linked against SDL %u.%u.%u", ver.major, ver.minor, ver.patch);
	}

	{
		log_info("Available video backends:");

		int num_drivers = SDL_GetNumVideoDrivers();
		for (int i = 0; i < num_drivers; i++) {
			log_info("%s", SDL_GetVideoDriver(i));
		}

		log_info("Current video backend: %s", SDL_GetCurrentVideoDriver());
	}

	u32 window_flags = (SDL_WINDOW_OPENGL
						| SDL_WINDOW_RESIZABLE
						| SDL_WINDOW_ALLOW_HIGHDPI /*for Mac*/);

#ifdef __ANDROID__
	window_flags |= SDL_WINDOW_FULLSCREEN;

	log_info("Android API level: %d.", SDL_GetAndroidSDKVersion());

	{
		SDL_DisplayMode mode;
		SDL_GetDesktopDisplayMode(0, &mode);

		log_info("mode.w %d", mode.w);
		log_info("mode.h %d", mode.h);

		width = height / (float)mode.h * (float)mode.w;
	}
#endif

	window.handle = SDL_CreateWindow(title,
									 SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
									 width * init_window_scale, height * init_window_scale,
									 window_flags);
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

#if defined(__ANDROID__)
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#elif defined(__EMSCRIPTEN__)
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#else
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif

#ifdef _DEBUG
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif

	window.gl_context = SDL_GL_CreateContext(window.handle);
	SDL_GL_MakeCurrent(window.handle, window.gl_context);

	{
#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
		auto load = gladLoadGLES2;
#else
		auto load = gladLoadGL;
#endif

		int version = load([](const char* name) {
			return (GLADapiproc) SDL_GL_GetProcAddress(name);
		});

		log_info("Loaded GL %d.%d", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));

		if (GLAD_VERSION_MAJOR(version) < 3) {
			panic_and_abort("Couldn't load OpenGL. Got version %d.%d", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));
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

	{
		int max_texture_size;
		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);

		log_info("GL max texture size: %d",         max_texture_size);
		log_info("GL vendor: %s",                   (const char*)glGetString(GL_VENDOR));
		log_info("GL renderer: %s",                 (const char*)glGetString(GL_RENDERER));
		log_info("GL version: %s",                  (const char*)glGetString(GL_VERSION));
		log_info("GL shading language version: %s", (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));
	}

	{
		window.vsync = prefer_vsync;

		char* USE_VSYNC = SDL_getenv("USE_VSYNC"); // @Leak
		if (USE_VSYNC) {
			window.vsync = (SDL_atoi(USE_VSYNC) != 0);
		}

		window.prefer_borderless_fullscreen = prefer_borderless_fullscreen;

		char* USE_BORDERLESS_FULLSCREEN = SDL_getenv("USE_BORDERLESS_FULLSCREEN"); // @Leak
		if (USE_BORDERLESS_FULLSCREEN) {
			window.prefer_borderless_fullscreen = (SDL_atoi(USE_BORDERLESS_FULLSCREEN) != 0);
		}

		char* LD_LIBRARY_PATH = SDL_getenv("LD_LIBRARY_PATH"); // @Leak
		log_info("LD_LIBRARY_PATH: %s", LD_LIBRARY_PATH ? LD_LIBRARY_PATH : "not set");
	}

	// {
	// 	int w;
	// 	int h;
	// 	SDL_GetWindowSize(window.handle, &w, &h);

	// 	int wp;
	// 	int hp;
	// 	SDL_GetWindowSizeInPixels(window.handle, &wp, &hp);

	// 	log_info("Detected DPI scale from SDL_GetWindowSizeInPixels: %f %f", wp/(float)w, hp/(float)h);
	// }

	{
		int display = SDL_GetWindowDisplayIndex(window.handle);

		float ddpi;
		float hdpi;
		float vdpi;
		SDL_GetDisplayDPI(display, &ddpi, &hdpi, &vdpi);

		log_info("Detected DPI scale from SDL_GetDisplayDPI: %f %f %f", ddpi/96.0f, hdpi/96.0f, vdpi/96.0f);
	}

	SDL_GL_SetSwapInterval(window.vsync ? 1 : 0);

	glDisable(GL_CULL_FACE);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glDisable(GL_DEPTH_TEST);
}

void deinit_window_and_opengl() {
	if (window.gl_context) SDL_GL_DeleteContext(window.gl_context);
	window.gl_context = nullptr;

	if (window.handle) SDL_DestroyWindow(window.handle);
	window.handle = nullptr;

	SDL_Quit();
}

void handle_event(const SDL_Event& ev) {
	switch (ev.type) {
		case SDL_QUIT: {
			window.should_quit = true;
			break;
		}

		case SDL_KEYDOWN: {
			SDL_Scancode scancode = ev.key.keysym.scancode;

			if (scancode >= 0 && scancode < window.NUM_KEYS) {
				bool ignore = false;

				// ignore alt+f4
				if (scancode == SDL_SCANCODE_F4 && (ev.key.keysym.mod & KMOD_ALT)) {
					ignore = true;
				}

				if (!ignore) {
					if (ev.key.repeat) {
						window.key_repeat[scancode / 32] |= 1 << (scancode % 32);
					} else {
						window.key_pressed[scancode / 32] |= 1 << (scancode % 32);
					}
				}
			}

			switch (scancode) {
				// fullscreen on alt+enter
				case SDL_SCANCODE_RETURN: {
					if (!ev.key.repeat) {
						if (ev.key.keysym.mod & KMOD_ALT) {
							set_fullscreen(!is_fullscreen());
						}
					}
					break;
				}

				// fullscreen on F4
				case SDL_SCANCODE_F4: {
					if (!ev.key.repeat) {
						// ignore alt+f4
						if (!(ev.key.keysym.mod & KMOD_ALT)) {
							set_fullscreen(!is_fullscreen());
						}
					}
					break;
				}

				// enable frame advance mode/goto next frame
				case SDL_SCANCODE_F5: {
					window.frame_advance_mode = true;
					window.should_skip_frame = false;
					break;
				}

				// disable frame advance mode
				case SDL_SCANCODE_F6: {
					window.frame_advance_mode = false;
					break;
				}

#ifdef __ANDROID__
				/*case SDL_SCANCODE_AC_BACK: {
					SDL_StartTextInput();
					break;
				}*/
#endif
			}
			break;
		}

		case SDL_CONTROLLERDEVICEADDED: {
			if (!window.controller) {
				window.controller = SDL_GameControllerOpen(ev.cdevice.which);

				log_info("Opened controller %s.", SDL_GameControllerName(window.controller));
			}
			break;
		}

		case SDL_CONTROLLERDEVICEREMOVED: {
			if (window.controller) {
				SDL_Joystick* joystick = SDL_GameControllerGetJoystick(window.controller);
				if (SDL_JoystickInstanceID(joystick) == ev.cdevice.which) {
					log_info("Closing controller %s...", SDL_GameControllerName(window.controller));

					SDL_GameControllerClose(window.controller);
					window.controller = nullptr;
				}
			}
			break;
		}

		case SDL_CONTROLLERBUTTONDOWN: {
			if (window.controller) {
				SDL_Joystick* joystick = SDL_GameControllerGetJoystick(window.controller);
				if (SDL_JoystickInstanceID(joystick) == ev.cbutton.which) {
					SDL_GameControllerButton button = (SDL_GameControllerButton) ev.cbutton.button;

					if (button >= 0 && button < window.NUM_CONTROLLER_BUTTONS) {
						window.controller_button_pressed[button / 32] |= 1 << (button % 32);
					}
				}
			}
			break;
		}
	}
}

void begin_frame() {
	window.frame_took_t = get_time();

	if (!window.prev_time_is_initialized) {
		window.prev_time = get_time() - 1.0; // 1.0 / window.target_fps;

		window.prev_time_is_initialized = true;
	}

	double prev_time = window.prev_time;
	double time = get_time();
	window.prev_time = time;

	window.frame_end_time = time + (1.0 / window.target_fps);

	// Set delta.
	{
		window.delta = (float)((time - prev_time) * 60.0);

		// 
		// Don't go below 30 fps. No upper limit for now.
		// 
		float max_delta = 2.0f;
		window.delta = fminf(window.delta, max_delta);
	}

	window.fps = (float)(1.0 / (time - prev_time));

	memset(window.key_pressed,               0, sizeof(window.key_pressed));
	memset(window.key_repeat,                0, sizeof(window.key_repeat));
	memset(window.controller_button_pressed, 0, sizeof(window.controller_button_pressed));

	window.avg_fps_sum += window.fps;
	window.avg_fps_num_samples += 1;

	if (time - window.avg_fps_last_time_updated > 1) {
		window.avg_fps = window.avg_fps_sum / window.avg_fps_num_samples;

		window.avg_fps_sum = 0;
		window.avg_fps_num_samples = 0;

		window.avg_fps_last_time_updated = time;
	}

	// handle mouse
	{
		u32 prev = window.mouse_state;
		window.mouse_state = SDL_GetMouseState(&window.mouse_x, &window.mouse_y);

		window.mouse_state_press   = window.mouse_state & (~prev);
		window.mouse_state_release = (~window.mouse_state) & prev;

		auto rect = renderer.game_texture_rect;
		window.mouse_x_world = (window.mouse_x - rect.x) / (float)rect.w * (float)window.game_width;
		window.mouse_y_world = (window.mouse_y - rect.y) / (float)rect.h * (float)window.game_height;
	}

	window.should_skip_frame = window.frame_advance_mode;
}

void swap_buffers() {
	window.frame_took = get_time() - window.frame_took_t;

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


static bool is_input_disabled() {
#ifdef DEVELOPER
	return console.is_open || console.was_open_last_frame;
#else
	return false;
#endif
}

bool is_key_pressed(SDL_Scancode key, bool repeat) {
	if (!(key >= 0 && key < window.NUM_KEYS)) {
		return false;
	}

	if (is_input_disabled()) {
		return false;
	}

	bool result = (window.key_pressed[key / 32] & (1 << (key % 32))) != 0;
	if (repeat) {
		result |= (window.key_repeat[key / 32] & (1 << (key % 32))) != 0;
	}
	return result;
}

bool is_key_held(SDL_Scancode key) {
	if (!(key >= 0 && key < SDL_NUM_SCANCODES)) {
		return false;
	}

	if (is_input_disabled()) {
		return false;
	}

	const u8* state = SDL_GetKeyboardState(nullptr);
	return (state[key] != 0);
}

bool is_controller_button_held(SDL_GameControllerButton button) {
	if (is_input_disabled()) {
		return false;
	}

	if (window.controller) {
		return SDL_GameControllerGetButton(window.controller, button);
	}
	return false;
}

bool is_controller_button_pressed(SDL_GameControllerButton button) {
	if (!(button >= 0 && button < window.NUM_CONTROLLER_BUTTONS)) {
		return false;
	}

	if (is_input_disabled()) {
		return false;
	}

	return (window.controller_button_pressed[button / 32] & (1 << (button % 32))) != 0;
}

float controller_get_axis(SDL_GameControllerAxis axis) {
	if (is_input_disabled()) {
		return 0;
	}

	if (window.controller) {
		return SDL_GameControllerGetAxis(window.controller, axis) / 32768.0f;
	}
	return 0;
}

bool is_mouse_button_held(u32 button) {
	if (is_input_disabled()) {
		return false;
	}

	return (window.mouse_state & SDL_BUTTON(button)) != 0;
}

bool is_mouse_button_pressed(u32 button) {
	if (is_input_disabled()) {
		return false;
	}

	return (window.mouse_state_press & SDL_BUTTON(button)) != 0;
}

bool is_mouse_button_released(u32 button) {
	if (is_input_disabled()) {
		return false;
	}

	return (window.mouse_state_release & SDL_BUTTON(button)) != 0;
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

		// NOTE: on an old AMD driver, when you enabled fullscreen, the refresh rate of your monitor would change.

		if (window.prefer_borderless_fullscreen) {
			SDL_SetWindowFullscreen(window.handle, SDL_WINDOW_FULLSCREEN_DESKTOP);
		} else {
			SDL_SetWindowFullscreen(window.handle, SDL_WINDOW_FULLSCREEN);
		}
	} else {
		SDL_SetWindowFullscreen(window.handle, 0);
	}
}

bool is_fullscreen() {
	u32 flags = SDL_GetWindowFlags(window.handle);
	return (flags & SDL_WINDOW_FULLSCREEN) != 0;
}

double get_time() {
	u64 diff = SDL_GetPerformanceCounter() - window.perf_counter_when_started;
	return (double)diff / window.perf_frequency_double;
}
