#pragma once

#include "common.h"

/*
* A module that handles OS window, GL context, game main loop.
*/

struct Window {
	/*   public   */

	bool should_quit;  // Set this to true when game should terminate.
	double target_fps = 60; // Used if vsync is off. See "init_window_and_opengl".

	/*   read-only   */

	SDL_Window* handle;
	SDL_GLContext gl_context;

	int game_width;
	int game_height;

	float fps; // for metrics
	float delta; // NOTE: multiplied by 60

	float avg_fps;

	double frame_took;

	bool frame_advance_mode;
	bool should_skip_frame;
	
	/*   private   */

	double prev_time;
	double frame_end_time;

	bool prev_time_is_initialized;
	bool prefer_borderless_fullscreen;

	float avg_fps_sum;
	float avg_fps_num_samples;
	double avg_fps_last_time_updated;

	double frame_took_t;

	u64 perf_counter_when_started;
	u64 perf_frequency;
	double perf_frequency_double;
};

extern Window window;

/*
If you pass "prefer_vsync" as false, then you may want to change "window.target_fps" after calling this function.

If vsync is false, then we do OS sleep + spinlock to keep the framerate.

Vsync option can be overriden by an environment variable "USE_VSYNC".

Fullscreen option can be overriden by an environment variable "USE_BORDERLESS_FULLSCREEN".

Note that vsync is forced anyway on most OS's window managers.
*/
void init_window_and_opengl(const char* title,
							int width, int height, int init_window_scale,
							bool prefer_vsync, bool prefer_borderless_fullscreen);

void deinit_window_and_opengl();

void begin_frame();
void swap_buffers();

bool window_handle_event(const SDL_Event& ev);

SDL_Window* get_window_handle(); // for common.h

// Enabling fullscreen doesn't change your monitor's display mode (resolution and refresh rate). At least it shouldn't.
// This means that it won't cause a flicker and rearrange all your windows.
void set_fullscreen(bool fullscreen);

bool is_fullscreen();
bool is_vsync_enabled();

double get_time();

struct ScopeTimer {
	double t;
	const char* name;

	ScopeTimer(const char* name) : name(name), t(get_time()) {}

	~ScopeTimer() {
		log_info("[%s] %fms", name, (get_time() - t) * 1000.0);
	}
};
