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

	enum Tool {
		TOOL_BRUSH,
		TOOL_LINE,
		TOOL_RECT,
		TOOL_SELECT,
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
	Tool tool;
	int select_x;
	int select_y;
	int select_w;
	int select_h;

	bool is_level_open;
	std::filesystem::path current_level_dir;

	bool show_demo_window;

	void init();
	void deinit();
	void update(float delta);
	void draw(float delta);

	void update_window_caption();

	void clear_state();
};

extern Editor editor;
