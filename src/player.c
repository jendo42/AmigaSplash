#include <exec/types.h>
#include <exec/memory.h>
#include <devices/audio.h>
#include <proto/exec.h>
#include <exec/io.h>
#include <proto/graphics.h>
#include <stdio.h>
#include <clib/alib_protos.h>

#include "system.h"
#include "player.h"
#include "log.h"

/* 1 = Ch 0, 8 = Ch 3. These are the ONLY Left speakers. */
static UBYTE leftChannelMap[] = { 1, 8 };

/* 2 = Ch 1, 4 = Ch 2. These are the ONLY Right speakers. */
static UBYTE rightChannelMap[] = { 2, 4 };

void PL_StartStereoPCM(struct Playback *playback, BYTE *leftBuffer, BYTE *rightBuffer, ULONG sizePerChannel, ULONG sampleRate)
{
	int32_t err;
	if (!sizePerChannel || sizePerChannel > 131072) {
		// no samples provided or buffer is too long
		return;
	}

	if (!leftBuffer && !rightBuffer) {
		// no samples provided
		return;
	}

	/* Reply port for both IORequests */
	struct MsgPort *replyPort = CreatePort(NULL, 0);
	playback->reply = replyPort;

	/* We need TWO requests */
	struct IOAudio *ioLeft = leftBuffer ? (struct IOAudio *)CreateExtIO(replyPort, sizeof(struct IOAudio)) : NULL;
	playback->left = ioLeft;

	struct IOAudio *ioRight = rightBuffer ? (struct IOAudio *)CreateExtIO(replyPort, sizeof(struct IOAudio)) : NULL;
	playback->right = ioRight;

	if (ioLeft) {
		/* Set up Left Allocation */
		ioLeft->ioa_Request.io_Message.mn_ReplyPort = replyPort;
		ioLeft->ioa_Request.io_Message.mn_Node.ln_Pri = 127;
		ioLeft->ioa_Request.io_Command = ADCMD_ALLOCATE;
		ioLeft->ioa_Request.io_Flags   = ADIOF_NOWAIT;
		ioLeft->ioa_AllocKey = 0;
		ioLeft->ioa_Data = leftChannelMap;
		ioLeft->ioa_Length = sizeof(leftChannelMap);

		err = OpenDevice(AUDIONAME, 0, (struct IORequest *)ioLeft, 0);
		LOG_DEBUG("OpenDevice(ioLeft): %d, key: %X", err, ioLeft->ioa_AllocKey);
	}

	if (ioRight) {
		/* Set up Right Allocation */
		ioRight->ioa_Request.io_Message.mn_ReplyPort = replyPort;
		ioRight->ioa_Request.io_Message.mn_Node.ln_Pri = 127;
		ioRight->ioa_Request.io_Command = ADCMD_ALLOCATE;
		ioRight->ioa_Request.io_Flags   = ADIOF_NOWAIT;
		ioRight->ioa_AllocKey = 0;
		ioRight->ioa_Data     = rightChannelMap;
		ioRight->ioa_Length   = sizeof(rightChannelMap);

		err = OpenDevice(AUDIONAME, 0, (struct IORequest *)ioRight, 0);
		LOG_DEBUG("OpenDevice(ioRight): %d, key: %X", err, ioRight->ioa_AllocKey);
	}

	uint32_t baseClock = GfxBase->DisplayFlags & NTSC ? 3579545 : 3546895;
	sizePerChannel &= ~1;
	sampleRate = baseClock / sampleRate;

	/* Setup Playback for Left */
	if (ioLeft) {
		ioLeft->ioa_Request.io_Command = CMD_WRITE;
		ioLeft->ioa_Request.io_Flags = ADIOF_PERVOL;
		ioLeft->ioa_Data = leftBuffer;
		ioLeft->ioa_Length = sizePerChannel;
		ioLeft->ioa_Period = sampleRate;
		ioLeft->ioa_Volume = 64;
		ioLeft->ioa_Cycles = 1;
	}

	/* Setup Playback for Right */
	if (ioRight) {
		ioRight->ioa_Request.io_Command = CMD_WRITE;
		ioRight->ioa_Request.io_Flags = ADIOF_PERVOL;
		ioRight->ioa_Data = rightBuffer;
		ioRight->ioa_Length = sizePerChannel;
		ioRight->ioa_Period = sampleRate; /* Ensure pitch matches perfectly! */
		ioRight->ioa_Volume = 64;
		ioRight->ioa_Cycles = 1;
	}

	/* Fire both channels */
	Disable();
	if (ioLeft) {
		BeginIO((struct IORequest *)ioLeft);
	}
	if (ioRight) {
		BeginIO((struct IORequest *)ioRight);
	}
	Enable();

	LOG_DEBUG("BeginIO: Left: %d; Right: %d", ioLeft ? ioLeft->ioa_Request.io_Error : 0, ioRight ? ioRight->ioa_Request.io_Error : 0);
}

void PL_WaitForPlayback(struct Playback *playback)
{
	struct IOAudio *ioLeft = playback->left;
	struct IOAudio *ioRight = playback->right;

	/* Now we wait for BOTH to finish. */

	if (ioLeft) {
		WaitIO((struct IORequest *)ioLeft);
	}

	if (ioRight) {
		WaitIO((struct IORequest *)ioRight);
	}
}

void PL_CleanupPlayback(struct Playback *playback)
{
	/* Cleanup (Free channels and close devices) */
	struct IOAudio *ioLeft = playback->left;
	if (ioLeft) {
		//ioLeft->ioa_Request.io_Command = ADCMD_FREE;
		//DoIO((struct IORequest *)ioLeft);
		LOG_DEBUG("Closing ioLeft");
		CloseDevice((struct IORequest *)ioLeft);
		DeleteExtIO((struct IORequest *)ioLeft);
	}
	struct IOAudio *ioRight = playback->right;
	if (ioRight) {
		//ioRight->ioa_Request.io_Command = ADCMD_FREE;
		//DoIO((struct IORequest *)ioRight);
		LOG_DEBUG("Closing ioRight");
		CloseDevice((struct IORequest *)ioRight);
		DeleteExtIO((struct IORequest *)ioRight);
	}
	if (playback->reply) {
		DeletePort(playback->reply);
	}
	memset(playback, 0, sizeof(*playback));
}
