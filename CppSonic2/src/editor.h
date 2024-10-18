#pragma once

#include "common.h"

struct Editor {
	void init();
	void deinit();
	void update(float delta);
	void draw(float delta);
};

extern Editor editor;
