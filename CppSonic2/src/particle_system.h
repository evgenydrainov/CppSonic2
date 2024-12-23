#pragma once

#include "common.h"

struct Particle {
	vec2 pos;
	float spd;
	float dir;
	float acc;

	float lifetime;
	float lifespan = 60;

	u32 sprite_index;
	float frame_index;

	vec4 color_from = color_white;
	vec4 color_to   = color_white;

	vec2 scale_from = {1, 1};
	vec2 scale_to   = {1, 1};
};

constexpr size_t MAX_PARTICLES = 10'000;

extern bump_array<Particle> g_Particles;

void init_particles();
void deinit_particles();

void update_particles(float delta);
void draw_particles(float delta);

Particle* add_particle(const Particle& p);
