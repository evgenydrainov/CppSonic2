#pragma once

#include "common.h"
#include "renderer.h"

struct SpriteFrame {
	int u;
	int v;
	int w;
	int h;
};

struct Sprite {
	Texture texture;
	array<SpriteFrame> frames;
	int xorigin;
	int yorigin;
	int loop_frame; // The frame from which animation will loop.
	float anim_spd; // Animation speed
	int width;
	int height;
};

Sprite create_sprite(const Texture& t,
					 int u, int v,
					 int width, int height,
					 int xorigin, int yorigin,
					 int frame_count = 0, int frames_in_row = 0,
					 float anim_spd = 0, int loop_frame = 0,
					 int xstride = 0, int ystride = 0);

void draw_sprite(const Sprite& s, int frame_index, vec2 pos,
				 vec2 scale = {1, 1}, float angle = 0,
				 vec4 color = color_white, glm::bvec2 flip = {});

float sprite_animate(const Sprite& s, float frame_index, float delta);
