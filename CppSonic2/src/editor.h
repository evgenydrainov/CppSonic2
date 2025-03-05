#pragma once

#ifdef EDITOR

#include "common.h"
#include "texture.h"
#include "game.h"

#include <filesystem>

struct View {
	vec2 scrolling;
	float zoom = 1;

	// The thing that is being panned and zoomed inside this view.
	vec2 thing_p0;
	vec2 thing_p1;
};

#define ACTION_TYPE_ENUM(X) \
	X(ACTION_SET_TILE_HEIGHT) \
	X(ACTION_SET_TILE_WIDTH) \
	X(ACTION_SET_TILE_ANGLE) \
	X(ACTION_SET_TILES)

DEFINE_NAMED_ENUM(ActionType, ACTION_TYPE_ENUM)

struct SetTileHeight {
	int tile_index;
	int in_tile_pos_x;
	int height_from;
	int height_to;
};

struct SetTileWidth {
	int tile_index;
	int in_tile_pos_y;
	int width_from;
	int width_to;
};

struct SetTile {
	int tile_index;
	int layer_index;
	Tile tile_from;
	Tile tile_to;
};

struct Action {
	ActionType type;
	union {
		struct {
			dynamic_array<SetTileHeight> sets;
		} set_tile_height;

		struct {
			dynamic_array<SetTileWidth> sets;
		} set_tile_width;

		struct {
			int tile_index;
			float angle_from;
			float angle_to;
		} set_tile_angle;

		struct {
			dynamic_array<SetTile> sets;
		} set_tiles;
	};
};

void free_action(Action* action);

struct TilesetEditor {
	enum Mode {
		MODE_HEIGHTS,
		MODE_WIDTHS,
		MODE_ANGLES,
	};

	enum Tool {
		TOOL_SELECT,
		TOOL_BRUSH,
		TOOL_ERASER,
		TOOL_AUTO,
	};

	Mode mode;
	Tool tool;
	View tileset_view;
	int selected_tile_index;

	dynamic_array<SetTileHeight> set_tile_heights;

	void update(float delta);
};

struct TilemapEditor {
	enum Tool {
		TOOL_SELECT,
		TOOL_BRUSH,
		TOOL_ERASER,
	};

	Tool tool;
	View tilemap_view;
	View tileset_view;
	int layer_index;
	bool layer_visible[3] = {true, false, true};
	bool show_objects = true;
	bool highlight_current_layer;

	dynamic_array<Tile> brush;
	int brush_w;
	int brush_h;

	dynamic_array<SetTile> set_tiles;

	bool dragging;
	int dragging_x1;
	int dragging_y1;
	int selection_x;
	int selection_y;
	int selection_w;
	int selection_h;

	void update(float delta);
};

struct ObjectsEditor {
	void update(float delta);
};

enum NotificationType {
	NOTIF_INFO,
};

struct Notification {
	NotificationType type;
	char buf[128];
	float timer = 2.5f;
	float alpha;
};

struct Editor {
	enum State {
		STATE_TILESET_EDITOR,
		STATE_TILEMAP_EDITOR,
		STATE_OBJECTS_EDITOR,
	};

	State state;

	TilesetEditor tileset_editor;
	TilemapEditor tilemap_editor;
	ObjectsEditor objects_editor;

	std::filesystem::path current_level_dir;
	bool is_level_open;

	Texture tileset_texture;
	SDL_Surface* tileset_surface;

	Tilemap tm;
	Tileset ts;
	bump_array<Object> objects;

	Texture heightmap;
	Texture widthmap;

	bump_array<Action> actions;
	int action_index = -1;

	bool show_demo_window;
	bool show_undo_history_window;

	const char* process_name;

	dynamic_array<Notification> notifications;

	void init(int argc, char* argv[]);
	void deinit();
	void update(float delta);
	void draw(float delta);

	void update_window_caption();

	void pick_and_open_level();
	void close_level();
	void try_undo();
	void try_redo();
	void action_add(const Action& action);
	void action_add_and_perform(const Action& action);
	void action_perform(const Action& action);
	void action_revert(const Action& action);
	bool try_run_game();
	void notify(NotificationType type, const char* fmt, ...);
};

extern Editor editor;

#endif
