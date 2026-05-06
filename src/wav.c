#include <string.h>

#include "wav.h"
#include "log.h"

#define Swap16(x) ((((UWORD)(x)) >> 8) | (((UWORD)(x)) << 8))

static inline ULONG Swap32(ULONG val)
{
    ULONG result;

    __asm__ __volatile__(
        "ror.w  #8, %0 \n\t"  /* Rotate lower 16 bits by 8 */
        "swap   %0     \n\t"  /* Swap upper and lower 16-bit halves */
        "ror.w  #8, %0"       /* Rotate the NEW lower 16 bits by 8 */

        : "=d" (result)       /* Output: Force compiler to use a Data Register (d0-d7) */
        : "0"  (val)          /* Input: Tell compiler to use the EXACT SAME register as output */
    );

    return result;
}

#define WAV_LoadChunkHeader(f, c) { int x = Read(f, c, sizeof(struct WavChunkHeader)); (c)->size = Swap32((c)->size); (void)x; }

static void WAV_Unsigned2Signed(UBYTE *data, ULONG numFrames)
{
	for (uint32_t i = 0; i < numFrames; i++) {
        /* Cast to unsigned, shift down, cast back to signed */
        data[i] = (BYTE)((UBYTE)data[i] - 128);
    }
}

/* * numFrames is the number of L/R PAIRS.
 * If your stereo file is 10,000 bytes total, numFrames is 5,000.
 */
static void WAV_Deinterleave(UBYTE *source, UBYTE *leftDest, UBYTE *rightDest, ULONG numFrames)
{

	/* 1. Force the compiler to put these pointers into
		  the 68000's physical Address Registers (A0, A1, A2). */
	register UBYTE *src  = source;
	register UBYTE *dstL = leftDest;
	register UBYTE *dstR = rightDest;

	/* 2. Loop Unrolling: Process 8 frames at a time.
		  This slashes the number of times the CPU has to
		  check the 'while' condition by 87%. */
	register ULONG chunks = numFrames >> 3;   /* Fast divide by 8 */
	register ULONG remainder = numFrames & 7; /* Fast modulo 8 */

	/* Process 16 bytes (8 Left, 8 Right) per loop iteration */
	while (chunks--) {
		*dstL++ = *src++;  *dstR++ = *src++;
		*dstL++ = *src++;  *dstR++ = *src++;
		*dstL++ = *src++;  *dstR++ = *src++;
		*dstL++ = *src++;  *dstR++ = *src++;
		*dstL++ = *src++;  *dstR++ = *src++;
		*dstL++ = *src++;  *dstR++ = *src++;
		*dstL++ = *src++;  *dstR++ = *src++;
		*dstL++ = *src++;  *dstR++ = *src++;
	}

	/* 3. Clean up the trailing frames (if numFrames wasn't perfectly divisible by 8) */
	while (remainder--) {
		*dstL++ = *src++;
		*dstR++ = *src++;
	}
}

bool WAV_LoadFile(const char *filename, struct WavInfo *info)
{
	bool deinterleave = false;
	uint8_t *data = NULL;
	bool result = false;
	BPTR fileLock = Open(filename, MODE_OLDFILE);
	if (!fileLock) {
		return false;
	}

	uint8_t formatId[4] = {0};
	struct WavChunkHeader chunk = {0};
	struct WAVEFORMATEXTENSIBLE format;

	WAV_LoadChunkHeader(fileLock, &chunk);
	if (strncmp(chunk.id, "RIFF", 4)) {
		LOG_DEBUG("WAV_LoadFile: no RIFF");
		goto cleanup;
	}

	Read(fileLock, formatId, 4);
	if (strncmp(formatId, "WAVE", 4)) {
		LOG_DEBUG("WAV_LoadFile: no WAVE");
		goto cleanup;
	}

	WAV_LoadChunkHeader(fileLock, &chunk);
	if (strncmp(chunk.id, "fmt ", 4)) {
		LOG_DEBUG("WAV_LoadFile: no fmt");
		goto cleanup;
	}

	LOG_DEBUG("fmt size: %u", chunk.size);
	if (Read(fileLock, &format, chunk.size) != (int)chunk.size) {
		LOG_DEBUG("WAV_LoadFile: failed to load fmt");
		goto cleanup;
	}

	// swap endianess
	format.Format.nChannels = Swap16(format.Format.nChannels);
	format.Format.wBitsPerSample = Swap16(format.Format.wBitsPerSample);
	format.Format.nSamplesPerSec = Swap32(format.Format.nSamplesPerSec);

	switch (format.Format.nChannels) {
		case 1:
			deinterleave = false;
			break;
		case 2:
			deinterleave = true;
			break;
		default:
			LOG_DEBUG("WAV_LoadFile: unsupported number of channels (%d)", format.Format.nChannels);
			goto cleanup;
	}

	if (format.Format.wBitsPerSample != 8) {
		LOG_DEBUG("WAV_LoadFile: unsupported bits per sample (%d)", format.Format.wBitsPerSample);
		goto cleanup;
	}

	while (1) {
		WAV_LoadChunkHeader(fileLock, &chunk);
		if (strncmp(chunk.id, "data", 4)) {
			// skip chunk
			Seek(fileLock, chunk.size, OFFSET_CURRENT);
		} else {
			break;
		}
	}

	if (strncmp(chunk.id, "data", 4)) {
		LOG_DEBUG("WAV_LoadFile: no data (%.4s)", chunk.id);
		goto cleanup;
	}

	data = AllocMem(chunk.size, deinterleave ? 0 : MEMF_CHIP);
	if (!data) {
		LOG_DEBUG("WAV_LoadFile: not enough memory for data (%u)", chunk.size);
		goto cleanup;
	}

	// load all PCM data to memory
	if (Read(fileLock, data, chunk.size) != (int32_t)chunk.size) {
		LOG_DEBUG("WAV_LoadFile: failed to load data (%u)", chunk.size);
		goto cleanup;
	}

	// apply the unsigned -> signed conversion
	WAV_Unsigned2Signed(data, chunk.size);

	if (deinterleave) {
		uint32_t frames = chunk.size / 2;
		info->size = frames;
		info->left = AllocMem(frames, MEMF_CHIP);
		if (!info->left) {
			LOG_DEBUG("WAV_LoadFile: not enough chipmem for left deinterleaved data (%u)", frames);
			goto cleanup;
		}

		info->right = AllocMem(frames, MEMF_CHIP);
		if (!info->right) {
			LOG_DEBUG("WAV_LoadFile: not enough chipmem for right deinterleaved data (%u)", frames);
			goto cleanup;
		}

		WAV_Deinterleave(data, info->left, info->right, frames);
	} else {
		info->left = data;
		info->right = data;
		info->size = chunk.size;
		data = NULL;
	}

	info->rate = format.Format.nSamplesPerSec;
	result = true;

cleanup:
	if (data) {
		if (data == info->left) {
			info->left = NULL;
		}

		FreeMem(data, chunk.size);
	}

	if (!result) {
		WAV_Cleanup(info);
	}

	Close(fileLock);
	return result;
}

void WAV_Cleanup(struct WavInfo *info)
{
	if (info->left) {
		FreeMem(info->left, info->size);
	}
	if (info->right && info->right != info->left) {
		FreeMem(info->right, info->size);
	}
	memset(info, 0, sizeof(*info));
}
