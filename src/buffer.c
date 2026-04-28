#include <memory.h>
#include <stdlib.h>
#include <assert.h>

#include <proto/dos.h>

#include "system.h"

#include "buffer.h"

#define LOG_FACILITY(...)
#define LOG_DEBUG(...)
#define LOG_TRACE(...)

LOG_FACILITY(Buffer, LL_INFO);

void buffer_init(buffer_t* buffer, uint16_t size, uint16_t preallocCount)
{
	LOG_DEBUG("Init (%p, %u, %u)", buffer, (ULONG)size, (ULONG)preallocCount);
	buffer->count = 0;
	buffer->size = size;
	buffer->capacity = size * preallocCount;
	buffer->data = malloc(buffer->capacity);
	if (!buffer->data) {
		buffer->capacity = 0;
	}
}

void buffer_clear(buffer_t *buffer)
{
	assert(buffer_resize(buffer, buffer->size, 0) == true);
	buffer->user = 0;
}

bool buffer_resize(buffer_t* buffer, uint16_t size, uint16_t count)
{
	LOG_TRACE("Resize (%p, %u, %u)", buffer, (ULONG)size, (ULONG)count);
	const uint32_t MinimumGrowSize = 16;
	uint32_t newCapacity = size * count;
	if (newCapacity > buffer->capacity || !buffer->data) {
		uint32_t diff = newCapacity - buffer->capacity;
		if (diff < MinimumGrowSize) {
			newCapacity += MinimumGrowSize - diff;
		}
		// + 1.5x growth rate
		newCapacity += buffer->capacity + (buffer->capacity >> 1);
		void *data = realloc(buffer->data, newCapacity);
		if (!data) {
			// not enough memory
			return false;
		}
		buffer->data = data;
		buffer->capacity = newCapacity;
	}
	buffer->size = size;
	buffer->count = count;
	return true;
}

bool buffer_resizec(buffer_t* buffer, uint16_t count)
{
	return buffer_resize(buffer, buffer->size, count);
}

void buffer_cleanup(buffer_t* buffer)
{
	LOG_DEBUG("Cleanup (%p)", buffer);
	if (buffer->data) {
		free(buffer->data);
	}
	memset(buffer, 0, sizeof(*buffer));
}

void *buffer_emplace_back(buffer_t* buffer)
{
	LOG_TRACE("EmplaceBack (%p)", buffer);
	uint32_t size = buffer->count * buffer->size;
	if (buffer_resizec(buffer, buffer->count + 1)) {
		return (char *)buffer->data + size;
	}
	return NULL;
}

bool buffer_pop_back(buffer_t* buffer)
{
	LOG_TRACE("PopBack (%p)", buffer);
	uint16_t count = buffer->count;
	if (count) {
		buffer_resizec(buffer, count - 1);
		return true;
	}

	return false;
}

void *buffer_back(buffer_t* buffer)
{
	LOG_TRACE("Back (%p)", buffer);
	uint16_t count = buffer->count;
	uint16_t size = buffer->size;
	if (count && buffer->data && buffer->capacity) {
		return (char *)buffer->data + size * (count - 1);
	}
	return NULL;
}

void *buffer_at(buffer_t* buffer, uint16_t i)
{
	LOG_TRACE("At (%p, %u)", buffer, (ULONG)i);
	uint16_t count = buffer->count;
	uint16_t size = buffer->size;
	if (i < count && buffer->data && buffer->capacity) {
		return (char *)buffer->data + size * i;
	}
	return NULL;
}

bool buffer_append(buffer_t *buffer, const void *data, uint16_t count)
{
	LOG_TRACE("Append (%p, %p, %u)", buffer, data, (unsigned)count);
	uint32_t size = buffer->count * buffer->size;
	if (!buffer_resizec(buffer, buffer->count + count)) {
		return false;
	}

	memcpy((char *)buffer->data + size, data, buffer->size * count);
	return true;
}

bool buffer_append_file(buffer_t *buffer, uint32_t file, uint16_t count)
{
	LOG_TRACE("AppendFile (%p, %p, %u)", buffer, file, (unsigned)count);
	uint32_t size2read = buffer->size * count;
	uint32_t size = buffer->count * buffer->size;
	if (!buffer_resizec(buffer, buffer->count + count)) {
		return false;
	}
	uint32_t read = Read(file, (char *)buffer->data + size, size2read);
	if (read != size2read) {
		return false;
	}
	return true;
}

bool buffer_append_string(buffer_t *buffer, const char *str, bool null_terminate)
{
	assert(buffer->size == 1);
	if (*str) {
		uint16_t len = strlen(str);
		if (null_terminate) {
			++len;
		}
		return buffer_append(buffer, str, len);
	}

	return null_terminate ? buffer_append(buffer, "", 1) : true;
}

bool buffer_append_char(buffer_t *buffer, char ch)
{
	assert(buffer->size == 1);
	return buffer_append(buffer, &ch, 1);
}

bool buffer_gc(buffer_t *buffer)
{
	LOG_TRACE("GC (%p)", buffer);
	uint32_t size = buffer->count * buffer->size;
	if (buffer->data && size && buffer->capacity > size) {
		void *newdata = realloc(buffer->data, size);
		if (!newdata) {
			return false;
		}

		buffer->data = newdata;
		return true;
	}

	return false;
}
