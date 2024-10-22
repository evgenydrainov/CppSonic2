#pragma once

#include "common.h"
#include "texture.h"

#include "imgui/imgui.h"

#include <filesystem>

struct Editor {
	enum Mode {
		MODE_HEIGHTMAP,
		MODE_TILEMAP,
	};

	Mode mode;

	struct {
		ImVec2 scrolling;
		float zoom = 1;

		Texture texture;
	} hmap;

	struct {
		ImVec2 scrolling;
		float zoom = 1;

		array<u32> tiles;
		int width;
		int height;
	} tmap;

	bool is_level_open;
	std::filesystem::path current_level_dir;

	void init();
	void deinit();
	void update(float delta);
	void draw(float delta);

	void update_window_caption();
};

extern Editor editor;
