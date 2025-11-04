#include "common.h"

static Arena temp_arena;

size_t temp_memory_max_usage_this_frame;
size_t temp_memory_max_usage_ever;

Arena* get_temp_arena() {
	return &temp_arena;
}
