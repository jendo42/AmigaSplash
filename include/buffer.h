#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct buffer buffer_t;
typedef bool (*buffer_foreach_cb_t)(uint16_t index, void *item, void *user);

struct buffer
{
	void* data;
	void* user;
	uint32_t capacity;
	uint16_t count;
	uint16_t size;
};

void buffer_cleanup(buffer_t* buffer);
void buffer_init(buffer_t* buffer, uint16_t size, uint16_t preallocCount);
void buffer_clear(buffer_t* buffer);
bool buffer_resize(buffer_t* buffer, uint16_t size, uint16_t count);
bool buffer_resizec(buffer_t* buffer, uint16_t count);

void* buffer_emplace_back(buffer_t* buffer);
bool buffer_pop_back(buffer_t* buffer);
void* buffer_back(buffer_t* buffer);

void* buffer_at(buffer_t* buffer, uint16_t i);
bool buffer_append(buffer_t *buffer, const void *data, uint16_t count);
bool buffer_append_file(buffer_t *buffer, uint32_t file, uint16_t count);
bool buffer_append_string(buffer_t *buffer, const char *str, bool null_terminate);
bool buffer_append_char(buffer_t *buffer, char ch);
bool buffer_gc(buffer_t *buffer);
