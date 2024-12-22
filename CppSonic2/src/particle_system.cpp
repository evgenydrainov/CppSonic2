#include "particle_system.h"

#include "sprite.h"
#include "assets.h"

bump_array<Particle> g_particles;

void init_particles() {
	g_particles = malloc_bump_array<Particle>(MAX_PARTICLES);
}

void deinit_particles() {
	free(g_particles.data);
}

void update_particles(float delta) {
	For (p, g_particles) {
		if (p->lifetime >= p->lifespan) {
			Remove(p, g_particles);
			continue;
		}

		p->pos.x += lengthdir_x(p->spd, p->dir) * delta;
		p->pos.y += lengthdir_y(p->spd, p->dir) * delta;

		p->spd += p->acc * delta;

		p->frame_index = sprite_animate(get_sprite(p->sprite_index), p->frame_index, delta);

		p->lifetime += delta;
	}
}

void draw_particles(float delta) {
	For (p, g_particles) {
		float f = p->lifetime / p->lifespan;

		vec2 scale = lerp(p->scale_from, p->scale_to, f);
		vec4 color = lerp(p->color_from, p->color_to, f);

		draw_sprite(get_sprite(p->sprite_index), (int)p->frame_index, p->pos, scale, 0, color);
	}
}

Particle* add_particle(const Particle& p) {
	Particle* result = array_add(&g_particles, p);
	return result;
}
