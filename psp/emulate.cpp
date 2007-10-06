#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <psptypes.h>
#include <psprtc.h>
#include <pspkernel.h>

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
void AudioCallback(PspSample *buffer, unsigned int *samples, void *userdata);

/* Initialize emulation */
int InitEmulation()
{
  gAudioEnabled = TRUE;

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

int full_semaid;
int empty_semaid;

/* Run emulation */
void RunEmulation()
{
	ULONG buttons;
	SceCtrlData pad;

	pspPerfInitFps(&Counter);

  empty_semaid = sceKernelCreateSema("bufferEmpty", 0, 1, 1, 0);
  full_semaid = sceKernelCreateSema("bufferFull", 0, 0, 1, 0);

  /* Resume sound */
  pspAudioSetChannelCallback(0, AudioCallback, 0);
pspSetClockFrequency(333);
	while (!ExitPSP)
	{
  sceKernelWaitSema(empty_semaid, 1, 0);
		for (ULONG loop=1024; loop; loop--)
			LynxSystem->Update();
if (gAudioBufferPointer > 256)
{
    sceKernelSignalSema(empty_semaid, 0); 
    sceKernelSignalSema(full_semaid, 1); 
}
else
    sceKernelSignalSema(empty_semaid, 1); 

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
pspSetClockFrequency(222);

  /* Stop sound */
  pspAudioSetChannelCallback(0, NULL, 0);
}

void AudioCallback(PspSample *OutBuf, unsigned int *length, void *userdata)
{
  int sample, i, j;
  static u8 leftover[64];
  static int leftover_pos = 0;

  sceKernelWaitSema(full_semaid, 1, 0);
  sceKernelSignalSema(full_semaid, 0); 
/*
  if (leftover_pos + gAudioBufferPointer <= 64)
  {
    for (i = 0; i < *length; i++)
    {
      OutBuf->Left = OutBuf->Right = 0;
      OutBuf++;
    }
  }
  else
  {
*/
    *length = PSP_AUDIO_SAMPLE_ALIGN(gAudioBufferPointer + leftover_pos) - 64;

    for (i = 0, j = 0; j < leftover_pos; i++, j++)
    {
      sample = ((int)leftover[i] << 8) - 32768;
//      sample = (sample > 32768) ? 32768 : (sample < -32767) ? -32767 : sample;

      OutBuf->Left = OutBuf->Right = (short)sample;
      OutBuf++;
    }

    for (i = 0; j < *length; i++, j++)
    {
      sample = ((int)gAudioBuffer[i] << 8) - 32768;
//      sample = (sample > 32768) ? 32768 : (sample < -32767) ? -32767 : sample;

      OutBuf->Left = OutBuf->Right = (short)sample;
      OutBuf++;
    }

    for (leftover_pos = 0; i < gAudioBufferPointer; leftover_pos++, i++)
      leftover[leftover_pos] = gAudioBuffer[i];

    gAudioBufferPointer = 0;
//  }

  sceKernelSignalSema(empty_semaid, 1); 
}

