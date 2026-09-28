#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

/* Example
// Macro to align memory sizes to the nearest multiple of the machine's word size (usually 8 bytes)
#define ALIGN_UP(size, alignment) (((size) + ((alignment) - 1)) & ~((alignment) - 1))
#ifndef DEFAULT_ALIGNMENT
#define DEFAULT_ALIGNMENT (2 * sizeof(void *))
#endif


int main() {

size_t aligned_size = ALIGN_UP(250, DEFAULT_ALIGNMENT);
uint32_t alingment = DEFAULT_ALIGNMENT;

printf("%zu\n", aligned_size);
printf("%zu\n", alingment);


}
*/

bool is_power_of_two(uintptr_t x) {
	return x > 0 && (x & (x - 1)) == 0;
}

uintptr_t align_forward(uintptr_t ptr, size_t align) {
	uintptr_t p, a, modulo;

	assert(is_power_of_two(align));

	p = ptr;
	a = (uintptr_t)align;
	modulo = p & (a - 1);

	if (modulo != 0) {
		p += a - modulo;
	}
	return p;
}

#ifndef DEFAULT_ALIGNMENT
#define DEFAULT_ALIGNMENT (2 * sizeof(void *))
#endif

typedef struct Arena Arena;
struct Arena {
	unsigned char *buf;
	size_t buf_len;
	size_t prev_offset;
	size_t curr_offset;
};

void arena_init(Arena *a, void *backing_buffer, size_t backing_buffer_length) {
	a->buf = (unsigned char *)backing_buffer;
	a->buf_len = backing_buffer_length;
	a->curr_offset = 0;
	a->prev_offset = 0;
}

void *arena_alloc_align(Arena *a, size_t size, size_t align) {
	uintptr_t curr_ptr = (uintptr_t)a->buf + (uintptr_t)a->curr_offset;
	uintptr_t offset = align_forward(curr_ptr, align);
	offset -= (uintptr_t)a->buf;

	if (offset + size <= a->buf_len) {
		void *ptr = &a->buf[offset];
		a->prev_offset = offset;
		a->curr_offset = offset + size;

		memset(ptr, 0, size);
		return ptr;
	}
	return NULL;
}

void *arena_alloc(Arena *a, size_t size) {
	return arena_alloc_align(a, size, DEFAULT_ALIGNMENT);
}

void arena_free(Arena *a, void *ptr) {
	(void)a;
	(void)ptr;
}

void *arena_resize_align(Arena *a, void *old_memory, size_t old_size, size_t new_size, size_t align) {
	unsigned char *old_mem = (unsigned char *)old_memory;

	assert(is_power_of_two(align));

	if (old_mem == NULL || old_size == 0) {
		return arena_alloc_align(a, new_size, align);
	} else if (a->buf <= old_mem && old_mem < a->buf + a->buf_len) {
		// If old_memory was the most recent allocation, attempt in-place resize
		if (a->buf + a->prev_offset == old_mem) {
			if (a->prev_offset + new_size <= a->buf_len) {
				a->curr_offset = a->prev_offset + new_size;
				if (new_size > old_size) {
					// Zero only the newly expanded region
					memset(old_mem + old_size, 0, new_size - old_size);
				}
				return old_memory;
			}
			return NULL; // Cannot fit in arena
		} else {
			// Allocate new memory block and copy contents over
			void *new_memory = arena_alloc_align(a, new_size, align);
			if (new_memory != NULL) {
				size_t copy_size = old_size < new_size ? old_size : new_size;
				memmove(new_memory, old_memory, copy_size);
			}
			return new_memory;
		}
	} else {
		assert(0 && "Memory is out of bounds of the buffer in this arena");
		return NULL;
	}
}

void *arena_resize(Arena *a, void *old_memory, size_t old_size, size_t new_size) {
	return arena_resize_align(a, old_memory, old_size, new_size, DEFAULT_ALIGNMENT);
}

void arena_free_all(Arena *a) {
	a->curr_offset = 0;
	a->prev_offset = 0;
}

typedef struct Temp_Arena_Memory Temp_Arena_Memory;
struct Temp_Arena_Memory {
	Arena *arena;
	size_t prev_offset;
	size_t curr_offset;
};

Temp_Arena_Memory temp_arena_memory_begin(Arena *a) {
	Temp_Arena_Memory temp;
	temp.arena = a;
	temp.prev_offset = a->prev_offset;
	temp.curr_offset = a->curr_offset;
	return temp;
}

void temp_arena_memory_end(Temp_Arena_Memory temp) {
	temp.arena->prev_offset = temp.prev_offset;
	temp.arena->curr_offset = temp.curr_offset;
}

int main(int argc, char **argv) {
	(void)argc;
	(void)argv;
	int i;

	unsigned char backing_buffer[256];
	Arena a = {0};
	arena_init(&a, backing_buffer, sizeof(backing_buffer));

	for (i = 0; i < 10; i++) {
		int *x;
		float *f;
		char *str;

		arena_free_all(&a);

		x = (int *)arena_alloc(&a, sizeof(int));
		f = (float *)arena_alloc(&a, sizeof(float));
		str = (char *)arena_alloc(&a, 10);

		*x = 123;
		*f = 987;
		memmove(str, "Hellope", 7);

		printf("%p: %d\n", (void *)x, *x);
		printf("%p: %f\n", (void *)f, *f);
		printf("%p: %s\n", (void *)str, str);

		str = (char *)arena_resize(&a, str, 10, 16);
		memmove(str + 7, " world!", 7);
		printf("%p: %s\n", (void *)str, str);
	}

	arena_free_all(&a);
	return 0;
}
