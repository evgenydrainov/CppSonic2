#include "sprite.h"

#include "renderer.h"
#include "assets.h"

Sprite create_sprite(const Texture& t,
					 int u, int v,
					 int width, int height,
					 int xorigin, int yorigin,
					 int frame_count, int frames_in_row,
					 float anim_spd, int loop_frame,
					 int xstride, int ystride) {
	if (frame_count == 0) frame_count = 1;
	if (frames_in_row == 0) frames_in_row = frame_count;
	if (xstride == 0) xstride = width;
	if (ystride == 0) ystride = height;

	Sprite result = {};
	result.texture = t;
	result.xorigin = xorigin;
	result.yorigin = yorigin;
	result.loop_frame = loop_frame;
	result.anim_spd = anim_spd;
	result.width = width;
	result.height = height;

	// @Leak
	result.frames = calloc_array<SpriteFrame>(frame_count);

	for (int i = 0; i < frame_count; i++) {
		SpriteFrame f = {};
		f.u = u + xstride * (i % frames_in_row);
		f.v = v + ystride * (i / frames_in_row);
		f.w = width;
		f.h = height;

		result.frames[i] = f;
	}

	return result;
}

void draw_sprite(const Sprite& s, int frame_index, vec2 pos,
				 vec2 scale, float angle,
				 vec4 color, glm::bvec2 flip) {
	const Texture& t = s.texture;

	Assert(frame_index >= 0);
	Assert(frame_index < s.frames.count);

	const SpriteFrame& frame = s.frames[frame_index];

	draw_texture(t, {frame.u, frame.v, frame.w, frame.h},
				 pos, scale, {(float)s.xorigin, (float)s.yorigin}, angle,
				 color, flip);
}

float sprite_animate(const Sprite& s, float frame_index, float delta) {
	frame_index += s.anim_spd * delta;

	if (frame_index >= s.frames.count) {
		float a = frame_index - s.loop_frame;
		float b = s.frames.count - s.loop_frame;
		Assert(b != 0);
		frame_index = s.loop_frame + fmodf(a, b);
	}

	return frame_index;
}
