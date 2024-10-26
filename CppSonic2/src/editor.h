#pragma once

#include "common.h"
#include "texture.h"

#include "imgui/imgui.h"

#include <filesystem>

struct View {
	ImVec2 scrolling;
	float zoom = 1;
};

struct Editor {
	enum Mode {
		MODE_HEIGHTMAP,
		MODE_TILEMAP,
	};

	Mode mode;

	View heightmap_view;
	Texture tileset_texture;

	View tilemap_view;
	View tile_select_view;

	array<u32> tilemap_tiles;
	int tilemap_width;
	int tilemap_height;

	int selected_tile_index;

	bool is_level_open;
	std::filesystem::path current_level_dir;

	void init();
	void deinit();
	void update(float delta);
	void draw(float delta);

	void update_window_caption();

	void clear_state();
};

extern Editor editor;
