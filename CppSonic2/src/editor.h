#pragma once

#include "common.h"

#include "imgui/imgui.h"

struct Editor {
	enum Mode {
		MODE_HEIGHTMAP,
		MODE_TILEMAP,
	};

	Mode mode;

	void init();
	void deinit();
	void update(float delta);
	void draw(float delta);
};

extern Editor editor;
