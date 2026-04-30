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

static UBYTE channelMap[] = { 1, 2, 4, 8 }; /* Try channels 0, 1, 2, then 3 */

void PL_StartSimple(BYTE *sampleData, ULONG sizeInBytes, ULONG sampleRate)
{
    struct MsgPort *replyPort = NULL;
    struct IOAudio *ioAudio = NULL;
    BOOL deviceOpened = FALSE;

    /* 1. Create the port and IO request */
    replyPort = CreateMsgPort();
    if (!replyPort) goto cleanup;

    ioAudio = (struct IOAudio *)CreateIORequest(replyPort, sizeof(struct IOAudio));
    if (!ioAudio) goto cleanup;

    /* 2. Setup the allocation map BEFORE opening the device */
    ioAudio->ioa_Request.io_Message.mn_Node.ln_Pri = 0;
    ioAudio->ioa_Data = channelMap;
    ioAudio->ioa_Length = sizeof(channelMap);

    /* 3. Open the Device.
          This automatically allocates the channel and generates the AllocKey! */
    if (OpenDevice(AUDIONAME, 0, (struct IORequest *)ioAudio, 0) != 0) {
        LOG_DEBUG("Error: Could not open audio.device or all channels are busy!");
        goto cleanup;
    }
    deviceOpened = TRUE;

    /* 4. Play the sound!
          Notice we DO NOT overwrite ioa_AllocKey here. We just set the playback rules. */
    ioAudio->ioa_Request.io_Command = CMD_WRITE;
    ioAudio->ioa_Request.io_Flags = ADIOF_PERVOL;

    ioAudio->ioa_Data = sampleData;       /* ABSOLUTELY MUST BE MEMF_CHIP */
    ioAudio->ioa_Length = sizeInBytes & ~1;    /* Must be an even number! */
    ioAudio->ioa_Period = 3579545 / sampleRate;
    ioAudio->ioa_Volume = 64;             /* 64 is max volume */
    ioAudio->ioa_Cycles = 1;              /* Play once */

    /* 5. Execute and block until finished */
    DoIO((struct IORequest *)ioAudio);
	/* Read the secret error code! */
    if (ioAudio->ioa_Request.io_Error != 0) {
        LOG_DEBUG("AUDIO REJECTED! Error Code: %d", ioAudio->ioa_Request.io_Error);
    }

cleanup:
    /* 6. Clean up. CloseDevice automatically frees any channels we allocated. */
    if (deviceOpened) {
        CloseDevice((struct IORequest *)ioAudio);
    }
    if (ioAudio) {
        DeleteIORequest((struct IORequest *)ioAudio);
    }
    if (replyPort) {
        DeleteMsgPort(replyPort);
    }
}

void PL_StartStereoPCM(struct Playback *playback, BYTE *leftBuffer, BYTE *rightBuffer, ULONG sizePerChannel, ULONG sampleRate)
{
	int32_t err;
	struct Message *msg;
	struct MsgPort *replyPort = CreatePort(NULL, 0);
	playback->reply = replyPort;

	/* We need TWO requests now */
	struct IOAudio *ioLeft = (struct IOAudio *)CreateExtIO(replyPort, sizeof(struct IOAudio));
	playback->left = ioLeft;

	struct IOAudio *ioRight = (struct IOAudio *)CreateExtIO(replyPort, sizeof(struct IOAudio));
	playback->right = ioRight;

	/* Set up Left Allocation */
	ioLeft->ioa_AllocKey = 0;
	ioLeft->ioa_Request.io_Message.mn_Node.ln_Pri = 127;
	ioLeft->ioa_Data = leftChannelMap;
	ioLeft->ioa_Length = sizeof(leftChannelMap);

	/* Set up Right Allocation */
	ioRight->ioa_AllocKey = 0;
	ioRight->ioa_Request.io_Message.mn_Node.ln_Pri = 127;
	ioRight->ioa_Data = rightChannelMap;
	ioRight->ioa_Length = sizeof(rightChannelMap);

	/* Open the device TWICE (once for each IO request) */
	err = OpenDevice(AUDIONAME, 0, (struct IORequest *)ioLeft, 0);
	LOG_DEBUG("OpenDevice(ioLeft): %d, key: %X", err, ioLeft->ioa_AllocKey);

	err = OpenDevice(AUDIONAME, 0, (struct IORequest *)ioRight, 0);
	LOG_DEBUG("OpenDevice(ioRight): %d, key: %X", err, ioRight->ioa_AllocKey);

	/* Allocate the Channels */
	//ioLeft->ioa_Request.io_Command = ADCMD_ALLOCATE;
	//ioLeft->ioa_Request.io_Flags = ADIOF_NOWAIT;
	//err = DoIO((struct IORequest *)ioLeft);
	//LOG_DEBUG("ADCMD_ALLOCATE(ioLeft): %d", err);

	//ioRight->ioa_Request.io_Command = ADCMD_ALLOCATE;
	//ioRight->ioa_Request.io_Flags = ADIOF_NOWAIT;
	//err = DoIO((struct IORequest *)ioRight);
	//LOG_DEBUG("ADCMD_ALLOCATE(ioRight): %d", err);

	uint32_t baseClock = GfxBase->DisplayFlags & NTSC ? 3579545 : 3546895;
	sizePerChannel &= ~1;
	sampleRate = baseClock / sampleRate;

	/* Setup Playback for Left */
	ioLeft->ioa_Request.io_Command = CMD_WRITE;
	ioLeft->ioa_Request.io_Flags = ADIOF_PERVOL;
	ioLeft->ioa_Data = leftBuffer;
	ioLeft->ioa_Length = sizePerChannel;
	ioLeft->ioa_Period = sampleRate;
	ioLeft->ioa_Volume = 64;
	ioLeft->ioa_Cycles = 1;

	/* Setup Playback for Right */
	ioRight->ioa_Request.io_Command = CMD_WRITE;
	ioRight->ioa_Request.io_Flags = ADIOF_PERVOL;
	ioRight->ioa_Data = rightBuffer;
	ioRight->ioa_Length = sizePerChannel;
	ioRight->ioa_Period = sampleRate; /* Ensure pitch matches perfectly! */
	ioRight->ioa_Volume = 64;
	ioRight->ioa_Cycles = 1;

	/* FIRE BOTH CHANNELS ASYNCHRONOUSLY!
	   Because the 68000 CPU executes instructions incredibly fast,
	   these will start practically simultaneously. */
	if (sizePerChannel) {
		if (leftBuffer) {
			BeginIO((struct IORequest *)ioLeft);
			LOG_DEBUG("IORequest(ioLeft): %d", ioLeft->ioa_Request.io_Error);
		}
		if (rightBuffer) {
			BeginIO((struct IORequest *)ioRight);
			LOG_DEBUG("IORequest(ioLeft): %d", ioRight->ioa_Request.io_Error);
		}
	}
}

void PL_WaitForPlayback(struct Playback *playback)
{
	struct IOAudio *ioLeft = playback->left;
	struct IOAudio *ioRight = playback->right;
	struct MsgPort *replyPort = playback->reply;

	/* Now we wait for BOTH to finish.
	   Since they are the same length, waiting for both ensures we don't
	   close the device while one is still wrapping up. */
	//if (ioLeft || ioRight) {
	//	Wait(1 << replyPort->mp_SigBit);
	//}
	if (ioLeft && ioLeft->ioa_Data) {
		WaitIO((struct IORequest *)ioLeft);
	}
	if (ioRight && ioRight->ioa_Data) {
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

