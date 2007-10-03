#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <psptypes.h>
#include <psprtc.h>

#include "audio.h"
#include "video.h"
#include "psp.h"
#include "ctrl.h"
#include "perf.h"
#include "image.h"
#include "kybd.h"

#include "emulate.h"

#include "lynxdef.h"
#include "System.h"

static CSystem *LynxSystem = NULL;

static PspFpsCounter Counter;
PspImage *Screen = NULL;

static UBYTE* DisplayCallback(ULONG objref);

/* Initialize emulation */
int InitEmulation()
{
	pspPerfInitFps(&Counter);

	if (!(Screen = pspImageCreateVram(256, HANDY_SCREEN_HEIGHT, PSP_IMAGE_16BPP)))
		return 0;

	Screen->Viewport.Width = HANDY_SCREEN_WIDTH;

	char *rom_file = (char*)malloc(sizeof(char) * (strlen(pspGetAppDirectory()) + 13));
  sprintf(rom_file, "%slynxboot.img", pspGetAppDirectory());
  char *game_file = "flack.lnx";

	try
	{
		LynxSystem = new CSystem(game_file, rom_file);
	}
	catch(CLynxException &err)
	{
		pspImageDestroy(Screen);
		free(rom_file);

		/* TODO */
		FILE *foo=fopen("ms0:/log.txt","w");
		fprintf(foo, "%s: %s\n", err.mDesc, err.mMsg);
		fclose(foo);

		return 0;
	}

	free(rom_file);

	LynxSystem->DisplaySetAttributes(MIKIE_NO_ROTATE, MIKIE_PIXEL_FORMAT_16BPP_5551, 
	  Screen->Width * Screen->BytesPerPixel, DisplayCallback, 0);

  return 1;
}

UBYTE* DisplayCallback(ULONG objref)
{
  pspVideoBegin();

  pspVideoPutImage(Screen, 0, 0, Screen->Viewport.Width, Screen->Viewport.Height);

	static char fps_text[32];
	sprintf(fps_text, "%.02f", pspPerfGetFps(&Counter));
  pspVideoPrint(&PspStockFont, 0, 0, fps_text, PSP_COLOR_WHITE);

  pspVideoEnd();
  pspVideoSwapBuffers();

	return (UBYTE*)Screen->Pixels;
}

/* Release emulation resources */
void TrashEmulation()
{
	if (LynxSystem) delete LynxSystem;
	if (Screen) pspImageDestroy(Screen);
}

/* Run emulation */
void RunEmulation()
{
	ULONG buttons;
	SceCtrlData pad;

	pspPerfInitFps(&Counter);

	while (!ExitPSP)
	{
		for (ULONG loop=1024; loop; loop--)
			LynxSystem->Update();

		buttons = LynxSystem->GetButtonData();
		
		if (pspCtrlPollControls(&pad))
		{
			if (pad.Buttons & PSP_CTRL_SQUARE) buttons |= BUTTON_B;
			else buttons &= BUTTON_B ^ 0xffffffff;

			if (pad.Buttons & PSP_CTRL_TRIANGLE) buttons |= BUTTON_A;
			else buttons &= BUTTON_A ^ 0xffffffff;

			if (pad.Buttons & PSP_CTRL_SELECT) buttons |= BUTTON_OPT1;
			else buttons &= BUTTON_OPT1 ^ 0xffffffff;

			if (pad.Buttons & PSP_CTRL_START) buttons |= BUTTON_OPT2;
			else buttons &= BUTTON_OPT2 ^ 0xffffffff;

			buttons &= BUTTON_UP ^ 0xffffffff;
			buttons &= BUTTON_DOWN ^ 0xffffffff;
			buttons &= BUTTON_LEFT ^ 0xffffffff;
			buttons &= BUTTON_RIGHT ^ 0xffffffff;

			if (pad.Buttons & PSP_CTRL_ANALUP)
				buttons |= BUTTON_UP;
			else if (pad.Buttons & PSP_CTRL_ANALDOWN)
				buttons |= BUTTON_DOWN;

			if (pad.Buttons & PSP_CTRL_ANALLEFT)
				buttons |= BUTTON_LEFT;
			else if (pad.Buttons & PSP_CTRL_ANALRIGHT)
				buttons |= BUTTON_RIGHT;
		}

		LynxSystem->SetButtonData(buttons);
	}
}

