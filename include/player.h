#pragma once

#include <exec/types.h>

struct Playback {
	struct MsgPort *reply;
	struct IOAudio *left;
	struct IOAudio *right;
};

void PL_StartStereoPCM(struct Playback *playback, BYTE *leftBuffer, BYTE *rightBuffer, ULONG sizePerChannel, ULONG sampleRate);
void PL_WaitForPlayback(struct Playback *playback);
void PL_CleanupPlayback(struct Playback *playback);
void PL_StartSimple(BYTE *sampleData, ULONG sizeInBytes, ULONG sampleRate);
