/*

	amiga-splash, simple program for displaying boot splash screen

	Original code by:
		Wojciech Jeczmien (jeczmien@panda.bg.univ.gda.pl)
		Tak Tang          (tst92@ecs.soton.ac.uk)
		Michael Becker    (nf198@fim.uni-erlangen.de)

	Reworked by:
		Michal 'Jendo' Jenikovsky (jendo@jmsystems.sk)

*/

/*
#define IM_WIDTH	403
#define IM_HEIGHT	258
#define IM_DEPTH	4
#define IM_NUMCOLS	(1 << IM_DEPTH)
#define IM_LACE		1
#define IM_DELAY	50
*/

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include <exec/types.h>
#include <graphics/gfxbase.h>
#include <intuition/intuitionbase.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/iffparse.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <hardware/custom.h>

#include "system.h"
#include "iff.h"

#include "test_image.h"

extern struct Custom custom;

uint16_t __chip sprite_data[] = {
	0x0000, 0x0000,
	0x0000, 0x0000
};

void vblankDirect()
{
	/* 2. Wait until we are OUT of the Vertical Blank (if we are currently in it) 
          We read the combined 32-bit vposr/vhposr register. */
    while (*(volatile ULONG *)&custom.vposr & 0x00000100) {
        /* Busy wait... */
    }

    /* 3. Wait until we ENTER the next Vertical Blank */
    while (!(*(volatile ULONG *)&custom.vposr & 0x00000100)) {
        /* Busy wait... */
    }
}

void fadeIn(struct Screen *scr, const uint16_t totalTicks, uint16_t *cmap, const uint16_t cmap_count)
{
	Forbid();
	Disable();
	uint16_t tick = 0;
	uint16_t temp[cmap_count];
	while (tick < totalTicks) {
		++tick;
		for (uint16_t i = 0, *p = &temp[0], *q = cmap; i < cmap_count; i++, p++, q++) {
			uint16_t col = *q;
			uint8_t r = (col >> 8) & 0x0F;
			uint8_t g = (col >> 4) & 0x0F;
			uint8_t b = (col >> 0) & 0x0F;
			r = tick * r / totalTicks;
			g = tick * g / totalTicks;
			b = tick * b / totalTicks;
			*p = (r << 8) | (g << 4) | (b << 0);
		}
		vblankDirect();
		LoadRGB4(&scr->ViewPort, temp, cmap_count);
	}
}

void fadeOut(struct Screen *scr, const uint16_t totalTicks, uint16_t *cmap, const uint16_t cmap_count)
{
	Forbid();
	Disable();
	uint16_t tick = totalTicks;
	uint16_t temp[cmap_count];
	while (tick) {
		--tick;
		for (uint16_t i = 0, *p = &temp[0], *q = cmap; i < cmap_count; i++, p++, q++) {
			uint16_t col = *q;
			uint8_t r = (col >> 8) & 0x0F;
			uint8_t g = (col >> 4) & 0x0F;
			uint8_t b = (col >> 0) & 0x0F;
			r = tick * r / totalTicks;
			g = tick * g / totalTicks;
			b = tick * b / totalTicks;
			*p = (r << 8) | (g << 4) | (b << 0);
		}
		vblankDirect();
		LoadRGB4(&scr->ViewPort, temp, cmap_count);
	}
}

int main(int argc, char *argv[])
{
	struct Window *mywin = NULL;
	struct Screen *scr = NULL;
	struct Screen *wbScreen = IntuitionBase->FirstScreen;
	const uint16_t cmap_zero[64] = { 0 };
	uint16_t cmap[64] = { 0 };

	(void)argc, (void)argv;

	if (!sys_init()) {
		return ERROR_INVALID_RESIDENT_LIBRARY;
	}

	// to minimize flicker
	Forbid();
	Disable();

	struct ImageInfo imageInfo = {0};
	if (!IFF_LoadImage("S:splash.iff", &imageInfo)) {
		goto end;
	}

	IFF_DeInterleave(&imageInfo);

	struct Image image = {
		0,0,
		imageInfo.header.w,			/* Bitmap width - rounded up to multiples of 64 */
		imageInfo.header.h,			/* Bitmap height */
		imageInfo.header.nPlanes,	/* Image Depth : 8 bitplanes -> 256 colours */
		imageInfo.bitmap,
		0xff, 0x0,					/* PlanePick, PlaneOnOff */
		NULL
	};

	// convert the CMAP into OCS/ECS format
	UWORD cmap_size = imageInfo.cmsize / 3;
	if (cmap_size > 32) {
		cmap_size = 32;
	}
	for (UWORD i = 0; i < cmap_size; i++) {
		// Extract the 8-bit R, G, B values
		UBYTE red   = imageInfo.cmap[(i * 3) + 0];
        UBYTE green = imageInfo.cmap[(i * 3) + 1];
        UBYTE blue  = imageInfo.cmap[(i * 3) + 2];

		// Scale them down to 4-bit
		red   >>= 4;
        green >>= 4;
        blue  >>= 4;

		// Pack them into the Amiga 0x0RGB UWORD format
		cmap[i] = (red << 8) | (green << 4) | blue;
	}

	if (!wbScreen) {
		// do this only when booting
		wbScreen = LockPubScreen("Workbench");
		if (wbScreen) {
			// hide workbench screen by clearing color registers
			// also reset mouse colors
			LoadRGB4(&wbScreen->ViewPort, (UWORD *)cmap_zero, 32);
		}
	} else {
		// we are already booted so do not hijack screens!
		wbScreen = NULL;
	}

	bool aga_present = sys_isaga();
	bool im_lace = image.Height > 256;

	int swh = 320;
	int shh = im_lace ? 256 : 128;

	int scr_ntsc = 0;
	if (GfxBase->DisplayFlags & NTSC) {
		scr_ntsc = im_lace ? 56 : 28;
	}
	
	uint32_t modeNormal = imageInfo.isham ? HAM_KEY : HIRES_KEY;
	uint32_t modeLaced = imageInfo.isham ? HAMLACE_KEY : HIRESLACE_KEY;
	uint32_t displayId = im_lace ? modeLaced : modeNormal;
	uint16_t type = CUSTOMSCREEN;
	if (imageInfo.isham) {
		type |= HAM;
	}

	Delay(50);

	scr = OpenScreenTags(NULL,
		SA_Left, swh - (image.Width / 2),
		SA_Top, shh - (image.Height / 2) - scr_ntsc,
		SA_Width, image.Width,
		SA_Height, image.Height,
		SA_Depth, image.Depth,
		SA_Title, (uint32_t)"SplashScreen",
		SA_Quiet, TRUE,
		SA_ShowTitle, FALSE,
		SA_Type, type,
		SA_DisplayID, displayId,
		SA_Draggable, FALSE,
		TAG_DONE
	);

	if (!scr) {
		goto end;
	}

	mywin = OpenWindowTags(NULL,
		WA_Left, 0,
		WA_Top, 0,
		WA_Width, image.Width,
		WA_Height, image.Height,
		WA_Flags, WFLG_SMART_REFRESH | WFLG_BORDERLESS | ACTIVATE | WFLG_RMBTRAP,
		WA_CustomScreen, (uint32_t)scr,
		WA_IDCMP, IDCMP_RAWKEY | IDCMP_CLOSEWINDOW | IDCMP_INACTIVEWINDOW | IDCMP_INTUITICKS | IDCMP_NEWPREFS | IDCMP_NEWSIZE,
		TAG_DONE
	);

	if (!mywin) {
		goto end;
	}

	// hide pointer
	SetPointer(mywin, sprite_data, 0, 0, 0, 0);

	// prepare fade-in: zero the palette
	LoadRGB4(&scr->ViewPort, cmap_zero, 32);
	// draw the image
	DrawImage(mywin->RPort, &image, 0, 0);

	// wait for vsync and start fade in
	vblankDirect();
	fadeIn(scr, 64, cmap, cmap_size);

	// program main loop
	struct IntuiMessage *msg;
	struct Rectangle rect;
	uint32_t signalMask = 1UL << mywin->UserPort->mp_SigBit;
	bool run = true;
	do {
		Wait(signalMask);
		while ((msg = (struct IntuiMessage *)GetMsg(mywin->UserPort))) {
			switch (msg->Class) {
				case IDCMP_RAWKEY:
					if (msg->Code == 0x45) {
						// ESC key press
						run = false;
					}
					break;
				case IDCMP_NEWPREFS:
				case IDCMP_NEWSIZE:
					// redraw image to center
					//QueryOverscan(displayId, &rect, OSCAN_STANDARD);
					//ScreenPosition(scr, SPOS_ABSOLUTE, rect.MinX, rect.MinY, rect.MaxX, rect.MaxY);
					break;
				case IDCMP_CLOSEWINDOW:
				case IDCMP_INACTIVEWINDOW:
					run = false;
					break;
				case IDCMP_INTUITICKS:
					if (IntuitionBase->FirstScreen != scr) {
						run = false;
					}
					break;
			}
			ReplyMsg((struct Message *)msg);
		}
	} while (run);

	ScreenToFront(scr);
	Delay(50);
	fadeOut(scr, 64, cmap, cmap_size);

end:
	Enable();
	Permit();

	// cleanup
	IFF_FreeImage(&imageInfo);
	if (mywin) {
		CloseWindow(mywin);
	}
	if (scr) {
		CloseScreen(scr);
	}	
	if (wbScreen) {
		UnlockPubScreen("Workbench", wbScreen);
	}
	sys_cleanup();
	return EXIT_SUCCESS;
}
