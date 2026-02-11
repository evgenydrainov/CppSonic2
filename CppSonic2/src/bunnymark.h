#pragma once

#include "common.h"
#include "renderer.h"

typedef struct Bunny {
    vec2 position;
    vec2 speed;
    vec4 color;
} Bunny;

struct Bunnymark {
	Texture texBunny;
	Bunny* bunnies;
	int bunniesCount;

	void init();
	void deinit();

	void update(float delta);
	void draw(float delta);
};

extern Bunnymark bunnymark;
