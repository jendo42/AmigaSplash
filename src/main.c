/*

	amiga-splash, simple program for displaying boot splash screen

	Original code by:
		Wojciech Jeczmien (jeczmien@panda.bg.univ.gda.pl)
		Tak Tang          (tst92@ecs.soton.ac.uk)
		Michael Becker    (nf198@fim.uni-erlangen.de)

	Reworked by:
		Michal 'Jendo' Jenikovsky (jendo@jmsystems.sk)

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

#include "system.h"
#include "iff.h"
#include "player.h"
#include "log.h"
#include "wav.h"
#include "macros.h"

#define LOG(x) Write(Output(), x "\n", sizeof(x))

uint16_t __chip sprite_data[] = {
	0x0000, 0x0000,
	0x0000, 0x0000
};

struct ColorSpec colors_zero[33] = { 0 };


// NOTE: there is no need to call the intuition locks,
// because whole program is running in Disable()
// ULONG lock = LockIBase(0);
// ...
// UnlockIBase(lock);
uint16_t screenscnt()
{
	uint16_t cnt = 0;
	struct Screen * scr = IntuitionBase->FirstScreen;
	while (scr) {
		cnt++;
		scr = scr->NextScreen;
	}

	return cnt;
}

bool hasfocus(struct Screen *scr)
{
	bool result = IntuitionBase->FirstScreen == scr;
	return result;
}

void fade(struct Screen *scr, const uint16_t totalTicks, uint16_t *cmap, const uint16_t cmap_count, bool fadeOut)
{
	uint16_t tick = fadeOut ? totalTicks : 0;
	uint16_t temp[cmap_count];
	while (fadeOut ? tick-- : tick++ < totalTicks) {
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
		sys_vblank();
		LoadRGB4(&scr->ViewPort, temp, cmap_count);
	}
}

int main(int argc, char *argv[])
{
	struct Playback playback = {0};
	struct Window *mywin = NULL;
	struct Screen *scr = NULL;
	struct Screen *wbScreen = NULL;
	struct Screen *wbScreenLock = NULL;
	bool filterDisabled = false;
	const uint16_t cmap_zero[64] = { 0 };
	uint16_t cmap[64] = { 0 };

	(void)argc, (void)argv;

	LOG("amiga-splash " XSTR(GIT_VERSION));

	if (SysBase->LibNode.lib_Version < 36) {
		LOG("This program requires OS 2.0+");
		return ERROR_INVALID_RESIDENT_LIBRARY;
	}

	if (!sys_init()) {
		LOG("Failed to init the program, maybe not enough memory?");
		return ERROR_INVALID_RESIDENT_LIBRARY;
	}

	// If this program run, disable interrupts to minimize flicker
	Forbid();
	Disable();

	// send signal to existing AmigaSplash program to close it
	if (strstr(sys_commandline(), "signal")) {
		struct Task *targetTask = FindTask("AmigaSplash");
		if (targetTask) {
			Signal(targetTask, SIGBREAKF_CTRL_C);
		}

		Enable();
		Permit();
		sys_cleanup();
		return 0;
	}

	{ // rename self task
		struct Task *me = FindTask(NULL);
		me->tc_Node.ln_Name = "AmigaSplash";
	}

	// initialize empty color table
	colors_zero[32].ColorIndex = -1;
	for (uint16_t i = 0; i < 32; i++) {
		colors_zero[i].ColorIndex = i;
	}

	struct ImageInfo imageInfo = {0};
	if (!IFF_LoadImage("S:splash.iff", &imageInfo)) {
		LOG("No splash image.");
		goto end;
	}

	IFF_DeInterleave(&imageInfo);

	struct WavInfo wavInfo = {0};
	if (WAV_LoadFile("S:splash.wav", &wavInfo)) {
		filterDisabled = wavInfo.rate > 11025;
		LOG_DEBUG("Got sound at (%X,%X) of size %u; %u Hz", (uint32_t)wavInfo.left, (uint32_t)wavInfo.right, wavInfo.size, wavInfo.rate);
	} else {
		LOG("No sound file.");
	}

	struct Image image = {
		0,0,
		imageInfo.header.w,			/* Bitmap width - rounded up to multiples of 64 */
		imageInfo.header.h,			/* Bitmap height */
		imageInfo.header.nPlanes,	/* Image Depth : 8 bitplanes -> 256 colours */
		imageInfo.bitmap,
		0xff, 0x0,					/* PlanePick, PlaneOnOff */
		NULL
	};

	LOG_DEBUG("Got image at %X: %dx%d; %d planes", (uint32_t)image.ImageData, image.Width, image.Height, image.Depth);

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

	bool aga_present = sys_isaga();
	bool im_lace = image.Height > 256;
	bool im_ntsc = GfxBase->DisplayFlags & NTSC;

	uint32_t modeNormal = imageInfo.isham ? HAM_KEY : HIRES_KEY;
	uint32_t modeLaced = imageInfo.isham ? HAMLACE_KEY : HIRESLACE_KEY;
	uint32_t displayId = im_lace ? modeLaced : modeNormal;
	uint16_t type = CUSTOMSCREEN;
	if (imageInfo.isham) {
		type |= HAM;
	}

	int swh = imageInfo.isham ? 160 : 320;
	int shh = im_lace ? 256 : 128;
	if (im_ntsc) {
		shh -= (im_lace ? 56 : 28);
	}

	LOG_DEBUG("AGA: %s; LACED: %s; NTSC: %s; HAM: %s", YESNO(aga_present), YESNO(im_lace), YESNO(im_ntsc), YESNO(imageInfo.isham));
	LOG_DEBUG("Screen size: %ux%u", swh * 2, shh * 2);

	// create screen for splash, force all colors to black
	scr = OpenScreenTags(NULL,
		SA_Left, swh - (image.Width / 2),
		SA_Top, shh - (image.Height / 2),
		SA_Width, image.Width,
		SA_Height, image.Height,
		SA_Depth, image.Depth,
		SA_Title, (uint32_t)"SplashScreen",
		SA_Quiet, TRUE,
		SA_ShowTitle, FALSE,
		SA_Type, type,
		SA_DisplayID, displayId,
		SA_Draggable, FALSE,
		SA_Colors, (uint32_t)colors_zero,
		SA_Exclusive, TRUE,
		TAG_DONE
	);

	if (!scr) {
		LOG_DEBUG("Can't open screen");
		goto end;
	}

	// create workbench screen behind the splash screen
	wbScreen = OpenScreenTags(NULL,
		SA_Type, WBENCHSCREEN,
		SA_Title, (uint32_t)"Workbench",
		SA_PubName, (uint32_t)"Workbench",
		SA_Behind, TRUE,
		SA_LikeWorkbench, TRUE,
		TAG_DONE
	);

	// lock the wb screen to prevent changing order
	wbScreenLock = LockPubScreen(NULL);

	mywin = OpenWindowTags(NULL,
		WA_Left, 0,
		WA_Top, 0,
		WA_Width, image.Width,
		WA_Height, image.Height,
		WA_Flags, WFLG_SMART_REFRESH | WFLG_BORDERLESS | WFLG_RMBTRAP,
		WA_CustomScreen, (uint32_t)scr,
		WA_IDCMP, IDCMP_RAWKEY | IDCMP_CLOSEWINDOW | IDCMP_INACTIVEWINDOW | IDCMP_INTUITICKS | IDCMP_NEWPREFS | IDCMP_NEWSIZE,
		TAG_DONE
	);

	if (!mywin) {
		LOG_DEBUG("Can't open window");
		goto end;
	}

	if (imageInfo.isham) {
		// for HAM we need to just load the palette
		// fading will be turned off
		// NOTE: if you want border color black,
		// you need to make sure that the zeroth color
		// register is set to black color
		LoadRGB4(&scr->ViewPort, cmap, cmap_size);
	}

	SetPointer(mywin, sprite_data, 0, 0, 0, 0);
	DrawImage(mywin->RPort, &image, 0, 0);
	ActivateWindow(mywin);
	if (!imageInfo.isham) {
		fade(scr, 64, cmap, cmap_size, false);
	} else {
		Delay(64);
	}

	// program main loop
	uint32_t timer = 0;
	struct IntuiMessage *msg;
	uint32_t signalMask = (1UL << mywin->UserPort->mp_SigBit) | SIGBREAKF_CTRL_C;
	bool run = true;
	do {
		if (Wait(signalMask) & SIGBREAKF_CTRL_C) {
			LOG_DEBUG("Exit by: Signal");
			run = false;
			break;
		}
		while ((msg = (struct IntuiMessage *)GetMsg(mywin->UserPort))) {
			switch (msg->Class) {
				case IDCMP_RAWKEY:
					if (msg->Code == 0x45) {
						// ESC key press
						LOG_DEBUG("Exit by: ESC");
						run = false;
					}
					break;
				case IDCMP_NEWPREFS:
					LOG_DEBUG("Exit by: IDCMP_NEWPREFS");
					run = false;
					break;
				case IDCMP_NEWSIZE:
					LOG_DEBUG("Exit by: IDCMP_NEWSIZE");
					// redraw image to center
					//QueryOverscan(displayId, &rect, OSCAN_STANDARD);
					//ScreenPosition(scr, SPOS_ABSOLUTE, rect.MinX, rect.MinY, rect.MaxX, rect.MaxY);
					run = false;
					break;
				case IDCMP_CLOSEWINDOW:
					LOG_DEBUG("Exit by: IDCMP_CLOSEWINDOW");
					run = false;
					break;
				case IDCMP_INACTIVEWINDOW:
					LOG_DEBUG("Exit by: IDCMP_INACTIVEWINDOW");
					run = false;
					break;
				case IDCMP_INTUITICKS:
					if (timer > 15) {
						if (!hasfocus(scr)) {
							LOG_DEBUG("Exit by: screen change");
							run = false;
						}
					} else {
						timer++;
					}
					break;
			}
			ReplyMsg((struct Message *)msg);
		}
	} while (run);

	// start the jingle if available
	if (wavInfo.size) {
		if (filterDisabled) {
			sys_setfilter(false);
		}
		PL_StartStereoPCM(&playback, wavInfo.left, wavInfo.right ? wavInfo.right : wavInfo.left, wavInfo.size, wavInfo.rate);
	}

	// fade out
	if (!imageInfo.isham) {
		fade(scr, 64, cmap, cmap_size, true);
	} else {
		Delay(64);
	}

	// switch to WB screen and fade in
	if (wbScreen) {
		// save the original palette
		uint16_t cmap_wb[64] = { 0 };
		for (uint16_t i = wbScreen->ViewPort.ColorMap->Count, *src = (uint16_t *)wbScreen->ViewPort.ColorMap->ColorTable, *dst = cmap_wb; i; i--) {
			*dst++ = *src++;
		}

		// make it black
		LoadRGB4(&wbScreen->ViewPort, cmap_zero, wbScreen->ViewPort.ColorMap->Count);

		sys_vblank();
		ScreenToFront(wbScreen);
		fade(wbScreen, 64, cmap_wb, wbScreen->ViewPort.ColorMap->Count, false);
	}

end:
	// interrupts can be enabled now
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
	if (wbScreenLock){
		UnlockPubScreen(NULL, wbScreenLock);
	}
	if (wbScreen) {
		CloseScreen(wbScreen);
	}
	if (wavInfo.size) {
		LOG_DEBUG("Waiting for sound to end...");
		PL_WaitForPlayback(&playback);
		if (filterDisabled) {
			sys_setfilter(true);
		}

		PL_CleanupPlayback(&playback);
		WAV_Cleanup(&wavInfo);
	}
	sys_cleanup();
	return EXIT_SUCCESS;
}
