/*
	This file is Copyrighted by Gemini - BIG SLOP INCORPORATED
*/

#include <exec/memory.h>
#include <libraries/iffparse.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/iffparse.h>
#include <stdio.h>

#include "iff.h"

/* --- The Decompression Routine --- */
void IFF_UnpackByteRun1(BYTE *source, BYTE *dest, LONG unpackedSize)
{
	LONG bytesWritten = 0;
	UBYTE controlByte;

	while (bytesWritten < unpackedSize) {
		controlByte = *source++;
		if (controlByte & 0x80) {
			WORD repeatCount = (-controlByte) + 1;
			BYTE dataByte = *source++;
			while (repeatCount-- > 0) {
				*dest++ = dataByte;
				bytesWritten++;
			}
		} else {
			WORD copyCount = controlByte + 1;
			while (copyCount-- > 0) {
				*dest++ = *source++;
				bytesWritten++;
			}
		}
	}
}

/* --- The Main IFF Loader --- */
BOOL IFF_LoadImage(STRPTR filename, struct ImageInfo *image)
{
	struct IFFHandle *iff = NULL;
	struct ContextNode *cn = NULL;
	struct StoredProperty *sp = NULL;
	struct BitMapHeader *bmhd = NULL;
	
	BPTR fileLock = 0;
	BYTE *compressedData = NULL;
	BYTE *chipRamBuffer = NULL;
	BOOL success = FALSE;

	/* 1. Allocate the IFFHandle */
	if (!(iff = AllocIFF())) {
		goto cleanup;
	}

	/* 2. Open the file via AmigaDOS */
	if (!(fileLock = Open(filename, MODE_OLDFILE))) {
		goto cleanup;
	}

	/* Attach the DOS file to the IFF parser */
	iff->iff_Stream = fileLock;
	InitIFFasDOS(iff);

	/* 3. Open the IFF stream for reading */
	if (OpenIFF(iff, IFFF_READ) != 0) {
		goto cleanup;
	}

	/* 4. Define our Targets! 
		  Tell iffparse to hold the BMHD and CMAP in memory automatically,
		  and STOP parsing when it hits the BODY chunk. */
	PropChunk(iff, ID_ILBM, ID_BMHD);
	PropChunk(iff, ID_ILBM, ID_CMAP);
	PropChunk(iff, ID_ILBM, ID_CAMG);
	StopChunk(iff, ID_ILBM, ID_BODY);

	/* 5. Parse the File! (This scans the file until it hits the BODY) */
	if (ParseIFF(iff, IFFPARSE_SCAN) != 0) {
		goto cleanup;
	}

	/* 6. Extract the BitMapHeader (BMHD) */
	if (!(sp = FindProp(iff, ID_ILBM, ID_BMHD))) {
		goto cleanup;
	}
	bmhd = (struct BitMapHeader *)sp->sp_Data;
	image->header = *bmhd;

	/* Inside your IFF parser after finding the CAMG chunk: */
	image->isham = FALSE;
	if ((sp = FindProp(iff, ID_ILBM, ID_CAMG))) {
		ULONG viewMode = *((ULONG *)sp->sp_Data);
		if (viewMode & HAM_MODE) {
			image->isham = TRUE;
		}
	}

	/* 7. Extract the Color Palette (CMAP) */
	if ((sp = FindProp(iff, ID_ILBM, ID_CMAP))) {
		/* sp->sp_Data contains the raw RGB byte triplets */
		image->cmsize = sp->sp_Size;
		image->cmap = AllocMem(sp->sp_Size, MEMF_ANY);
		if (image->cmap) {
			memcpy(image->cmap, sp->sp_Data, sp->sp_Size);
		} else {
			image->cmsize = 0;
		}
	}

	/* 8. Handle the BODY (Graphics Data) */
	cn = CurrentChunk(iff);
	if (!cn || cn->cn_ID != ID_BODY) {
		goto cleanup;
	}

	/* Calculate the uncompressed size: (Width in bytes) * Height * Depth */
	LONG rowBytes = ((bmhd->w + 15) / 16) * 2; /* Round up to nearest WORD */
	LONG uncompressedSize = rowBytes * bmhd->h * bmhd->nPlanes;

	/* Allocate the final CHIP RAM buffer for Intuition/Custom Chips */
	chipRamBuffer = AllocMem(uncompressedSize, MEMF_CHIP);
	if (!chipRamBuffer) {
		goto cleanup;
	}

	image->bitmap = (UWORD *)chipRamBuffer;
	image->bmsize = uncompressedSize;

	if (bmhd->compression == 1) {
		/* --- COMPRESSED (ByteRun1) --- */
		LONG compressedSize = cn->cn_Size;
		
		/* Allocate temporary FAST RAM for the compressed file data */
		compressedData = AllocMem(compressedSize, MEMF_ANY);
		if (!compressedData) goto cleanup;

		/* Read the compressed chunk from disk into FAST RAM */
		ReadChunkBytes(iff, compressedData, compressedSize);

		/* Decompress it directly into CHIP RAM */
		IFF_UnpackByteRun1(compressedData, chipRamBuffer, uncompressedSize);
		
		/* Free the temporary FAST RAM */
		FreeMem(compressedData, compressedSize);
		compressedData = NULL;
	} 
	else {
		/* --- UNCOMPRESSED --- */
		/* Read directly from disk into CHIP RAM */
		ReadChunkBytes(iff, chipRamBuffer, uncompressedSize);
	}

	success = TRUE;

cleanup:
	/* 9. Professional Teardown */
	if (iff) {
		CloseIFF(iff);
		FreeIFF(iff);
	}
	if (fileLock) {
		Close(fileLock);
	}

	return success;
}

void IFF_FreeImage(struct ImageInfo *image)
{
	if (image->cmap) {
		FreeMem(image->cmap, image->cmsize);
	}
	if (image->bitmap) {
		FreeMem(image->bitmap, image->bmsize);
	}
	memset(image, 0, sizeof(*image));
}

/* A conceptual Amiga de-interleave loop */
void IFF_DeInterleave(struct ImageInfo *image)
{
	WORD depth = image->header.nPlanes;
	WORD height = image->header.h;
	BYTE *interleavedSource = (BYTE *)image->bitmap;
	BYTE *planarDest = AllocMem(image->bmsize, MEMF_CHIP);
	LONG bytesPerRow = ((image->header.w + 15) / 16) * 2;
    LONG planeSize = bytesPerRow * height;
    
    for (WORD row = 0; row < height; row++) {
        for (WORD plane = 0; plane < depth; plane++) {
            
            /* Calculate where to read from the IFF buffer */
            LONG sourceOffset = (row * depth * bytesPerRow) + (plane * bytesPerRow);
            
            /* Calculate where to write in the strict planar buffer */
            LONG destOffset = (plane * planeSize) + (row * bytesPerRow);
            
            /* Copy the row! (In production, use the Blitter or CopyMem for speed) */
            CopyMem(&interleavedSource[sourceOffset], &planarDest[destOffset], bytesPerRow);
        }
    }

	FreeMem(image->bitmap, image->bmsize);
	image->bitmap = (UWORD *)planarDest;
}