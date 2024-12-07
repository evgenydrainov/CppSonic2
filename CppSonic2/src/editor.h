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
		MODE_OBJECTS,
	};

	enum HMode {
		HMODE_HEIGHTS,
		HMODE_WIDTHS,
		HMODE_ANGLES,
	};

	enum Tool {
		TOOL_BRUSH,
		TOOL_LINE,
		TOOL_RECT,
		TOOL_SELECT,
		TOOL_TOP_SOLID_BRUSH,
		TOOL_LRB_SOLID_BRUSH,
	};

	enum HTool {
		HTOOL_BRUSH,
		HTOOL_ERASER,
		HTOOL_AUTO,
		HTOOL_SELECT,
	};

	Mode mode;
	HMode hmode;

	View heightmap_view;
	View tilemap_view;
	View tile_select_view;

	Tileset ts;
	Texture tileset_texture;
	SDL_Surface* tileset_surface;
	Texture heightmap;
	Texture widthmap;

	Tilemap tm;

	array<Tile> brush;
	glm::ivec2 brush_size;

	Selection tilemap_tile_selection;
	Tool tool;
	HTool htool;

	Selection tilemap_select_tool_selection;
	Selection tilemap_rect_tool_selection;

	bool show_tile_indices;
	bool show_collision;
	int layer_index;

	bump_array<Object> objects;
	int selected_object = -1;
	int hmap_selected_tile = -1;

	bool is_level_open;
	std::filesystem::path current_level_dir;

	bool show_demo_window;

	const char* process_name;

	void init();
	void deinit();
	void update(float delta);
	void draw(float delta);

	void update_window_caption();

	void clear_state();

	void import_s1_level(const char* level_data_path,
						 const char* chunk_data_path,
						 const char* tile_height_data_path,
						 const char* tile_width_data_path,
						 const char* tile_indices_path,
						 const char* startpos_file_path,
						 const char* tile_angle_data_path,
						 const char* tileset_texture_path);
};

extern Editor editor;

#endif
