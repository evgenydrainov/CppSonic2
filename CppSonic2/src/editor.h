#pragma once

#ifdef EDITOR

#include "common.h"
#include "texture.h"
#include "game.h"

#include "imgui/imgui.h"

#include <filesystem>

struct View {
	ImVec2 scrolling;
	float zoom = 1;
};

struct Selection {
	bool dragging;
	int drag_start_x;
	int drag_start_y;

	int x;
	int y;
	int w;
	int h;
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

	enum HTool {
		HTOOL_BRUSH,
		HTOOL_ERASER,
	};

	Mode mode;

	View heightmap_view;
	View tilemap_view;
	View tile_select_view;

	Tileset ts;
	Texture tileset_texture;
	Texture heightmap;
	Texture widthmap;

	Tilemap tm;

	Tile selected_tile;
	Tool tool;
	HTool htool;

	Selection tilemap_select_tool_selection;
	Selection tilemap_rect_tool_selection;
	bool heightmap_show_collision = true;
	bool heightmap_show_widths;

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

#endif
