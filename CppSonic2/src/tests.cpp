#include "tests.h"

static void test_arena_realloc() {
	Arena arena = allocate_arena(get_libc_allocator(), 128);
	defer { free(arena.data); };

	Allocator allocator = get_arena_allocator(&arena);

	// misalign
	arena.count += 1;

	u8* memory = nullptr;

	memory = allocator_resize(allocator, memory, 16, 0);
	Assert(arena.count == 24);

	memory = allocator_resize(allocator, memory, 32, 16);
	Assert(arena.count == 40);

	memory = allocator_resize(allocator, memory, 16, 32);
	Assert(arena.count == 24);

	memory = allocator_resize(allocator, memory, 32, 16);
	Assert(arena.count == 40);

	memory = allocator_resize(allocator, memory, 16, 32);
	Assert(arena.count == 24);

	memory = allocator_resize(allocator, memory, 32, 16);
	Assert(arena.count == 40);

	memory = allocator_resize(allocator, memory, 16, 32);
	Assert(arena.count == 24);

	allocator_free(allocator, memory, 16);
	Assert(arena.count == 8);
}

void run_tests() {
	test_arena_realloc();
}
