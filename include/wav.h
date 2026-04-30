#pragma once
#include <stdint.h>
#include <stdbool.h>

#include <proto/dos.h>
#include <proto/exec.h>

typedef struct _GUID {
	uint32_t Data1;
	uint16_t  Data2;
	uint16_t  Data3;
	uint8_t  Data4[8];
} GUID;

typedef struct tWAVEFORMATEX {
	uint16_t  wFormatTag;
	uint16_t  nChannels;
	uint32_t nSamplesPerSec;
	uint32_t nAvgBytesPerSec;
	uint16_t  nBlockAlign;
	uint16_t  wBitsPerSample;
	uint16_t  cbSize;
} WAVEFORMATEX, *PWAVEFORMATEX, *NPWAVEFORMATEX, *LPWAVEFORMATEX;

typedef struct WAVEFORMATEXTENSIBLE {
	WAVEFORMATEX Format;
	union {
		uint16_t wValidBitsPerSample;
		uint16_t wSamplesPerBlock;
		uint16_t wReserved;
	} Samples;
	uint32_t        dwChannelMask;
	GUID         SubFormat;
}  *PWAVEFORMATEXTENSIBLE;

struct WavChunkHeader
{
	uint8_t id[4];
	uint32_t size;
};

struct WavInfo
{
	uint32_t rate;
	uint32_t size;
	uint8_t *left;
	uint8_t *right;
};

bool WAV_LoadFile(const char *filename, struct WavInfo *info);
void WAV_Cleanup(struct WavInfo *info);
