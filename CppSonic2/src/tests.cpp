#include "tests.h"

static void test_arena_realloc() {
	Arena arena = allocate_arena(128, get_libc_allocator());
	defer { free(arena.data); };

	Allocator allocator = get_arena_allocator(&arena);

	// misalign
	arena.count += 1;

	u8* memory = nullptr;

	memory = arealloc(memory, 16, 0, allocator);
	Assert(arena.count == 24);

	memory = arealloc(memory, 32, 16, allocator);
	Assert(arena.count == 40);

	memory = arealloc(memory, 16, 32, allocator);
	Assert(arena.count == 24);

	memory = arealloc(memory, 32, 16, allocator);
	Assert(arena.count == 40);

	memory = arealloc(memory, 16, 32, allocator);
	Assert(arena.count == 24);

	memory = arealloc(memory, 32, 16, allocator);
	Assert(arena.count == 40);

	memory = arealloc(memory, 16, 32, allocator);
	Assert(arena.count == 24);

	afree(memory, 16, allocator);
	Assert(arena.count == 8);
}

void run_tests() {
	test_arena_realloc();
}
