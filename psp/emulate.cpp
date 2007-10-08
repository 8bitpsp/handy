#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pspkernel.h>
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

static int TicksPerUpdate;
static u32 TicksPerSecond;
static u64 LastTick;
static u64 CurrentTick;

static PspFpsCounter Counter;
PspImage *Screen = NULL;

static UBYTE* DisplayCallback(ULONG objref);
void AudioCallback(void *buffer, unsigned int *samples, void *userdata);

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
  
  /* Wait */
  do { sceRtcGetCurrentTick(&CurrentTick); }
  while (CurrentTick - LastTick < TicksPerUpdate);
  LastTick = CurrentTick;

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

  /* Recompute update frequency */
  TicksPerSecond = sceRtcGetTickResolution();
  TicksPerUpdate = TicksPerSecond / 60;
  sceRtcGetCurrentTick(&LastTick);

  /* Resume sound */
  pspAudioSetChannelCallback(0, AudioCallback, 0);
pspSetClockFrequency(333);
	while (!ExitPSP)
	{
		for (ULONG loop = 1024; loop; loop--)
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
pspSetClockFrequency(222);

  /* Stop sound */
  pspAudioSetChannelCallback(0, NULL, 0);
}

inline void AudioCallback(void *buffer, unsigned int *length, void *userdata)
{
  PspMonoSample *OutBuf = (PspMonoSample*)buffer;
  int i, play_length, played = 0;
  static ULONG previous_buffer_pos = 0;
//sceKernelDelayThread(1000000/20);
//  while (gAudioBufferPointer == previous_buffer_pos);
  ULONG current_buffer_pos = gAudioBufferPointer;

  if (current_buffer_pos == previous_buffer_pos)
  {
    for (*length = 0; *length < 64; *length++)
      OutBuf[*length].Channel = 0;
    return;
  }
  else
  if (current_buffer_pos > previous_buffer_pos)
  {
    /* Compute length */
    play_length = current_buffer_pos - previous_buffer_pos;
    *length = PSP_AUDIO_SAMPLE_TRUNCATE(play_length);

    if (*length < 64)
    {
      for (*length = 0; *length < 64; *length++)
        OutBuf[*length].Channel = 0;
      return;
    }

    /* Copy data from audio buffer */
    for (i = previous_buffer_pos; played < *length; i++, played++)
      (OutBuf++)->Channel = (short)(((int)gAudioBuffer[i] << 8) - 32768);
  }
  else
  {
    /* Compute length */
    play_length = (HANDY_AUDIO_BUFFER_SIZE - previous_buffer_pos) + current_buffer_pos;
    *length = PSP_AUDIO_SAMPLE_TRUNCATE(play_length);

    if (*length < 64)
    {
      for (*length = 0; *length < 64; *length++)
        OutBuf[*length].Channel = 0;
      return;
    }

    /* Copy data from audio buffer */
    for (i = previous_buffer_pos; i < HANDY_AUDIO_BUFFER_SIZE; i++, played++)
      (OutBuf++)->Channel = (short)(((int)gAudioBuffer[i] << 8) - 32768);

    /* Copy data from audio buffer */
    for (i = 0; played < *length; i++, played++)
      (OutBuf++)->Channel = (short)(((int)gAudioBuffer[i] << 8) - 32768);
  }

  previous_buffer_pos += played;
  previous_buffer_pos %= HANDY_AUDIO_BUFFER_SIZE; /* wraparound */
}
