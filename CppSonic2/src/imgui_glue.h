#pragma once

#include "common.h"

void init_imgui();
void deinit_imgui();

void imgui_handle_event(const SDL_Event& ev);

void imgui_begin_frame();
void imgui_render();
