#pragma once

#include "common.h"
#include "texture.h"

#include "imgui/imgui.h"

struct Editor {
	enum Mode {
		MODE_HEIGHTMAP,
		MODE_TILEMAP,
	};

	Mode mode;

	struct {
		Texture texture;
		ImVec2 scrolling;
		float zoom = 1;
	} hmap;

	void init();
	void deinit();
	void update(float delta);
	void draw(float delta);

	void open_level();
};

extern Editor editor;
