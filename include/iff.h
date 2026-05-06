/*
	This file is Copyrighted by Gemini - BIG SLOP INCORPORATED
*/
#pragma once

#include <exec/types.h>

/* Official IFF Chunk IDs (Stored as 32-bit integers) */
#define ID_ILBM MAKE_ID('I','L','B','M')
#define ID_BMHD MAKE_ID('B','M','H','D')
#define ID_CMAP MAKE_ID('C','M','A','P')
#define ID_BODY MAKE_ID('B','O','D','Y')
#define ID_CAMG MAKE_ID('C','A','M','G')

#define HAM_MODE 0x0800 /* The hardware bitflag for HAM */

/* The standard IFF ILBM Bitmap Header Structure */
struct BitMapHeader {
	UWORD w, h;             /* Width and Height */
	WORD  x, y;             /* X and Y offsets */
	UBYTE nPlanes;          /* Number of bitplanes (depth) */
	UBYTE masking;          /* Masking type */
	UBYTE compression;      /* 0 = None, 1 = ByteRun1 */
	UBYTE pad1;             /* Padding */
	UWORD transparentColor; /* Transparent color index */
	UBYTE xAspect, yAspect; /* Pixel aspect ratio */
	WORD  pageWidth, pageHeight;
};

struct ImageInfo {
	struct BitMapHeader header;
	UBYTE isham;
	ULONG cmsize;
	ULONG bmsize;
	UBYTE *cmap;
	UWORD *bitmap;
};

void IFF_UnpackByteRun1(BYTE *source, BYTE *dest, LONG unpackedSize);
BOOL IFF_LoadImage(STRPTR filename, struct ImageInfo *image);
void IFF_FreeImage(struct ImageInfo *image);
void IFF_DeInterleave(struct ImageInfo *image);