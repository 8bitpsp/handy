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

#include "menu.h"
#include "emulate.h"

#include "lynxdef.h"
#include "System.h"

extern CSystem *LynxSystem;
extern EmulatorOptions Options;

static int TicksPerUpdate;
static u32 TicksPerSecond;
static u64 LastTick;

static int ScreenX;
static int ScreenY;
static int ScreenW;
static int ScreenH;

static int ClearScreen;
static PspFpsCounter Counter;
PspImage *Screen = NULL;

static UBYTE* DisplayCallback(ULONG objref);
void AudioCallback(void *buffer, unsigned int *samples, void *userdata);

/* Initialize emulation */
int InitEmulation()
{
  pspPerfInitFps(&Counter);

	if (!(Screen = pspImageCreateVram(256, HANDY_SCREEN_HEIGHT, PSP_IMAGE_16BPP)))
		return 0;

  return 1;
}

UBYTE* DisplayCallback(ULONG objref)
{
  pspVideoBegin();

  /* Clear the buffer first, if necessary */
  if (ClearScreen >= 0)
  {
    ClearScreen--;
    pspVideoClearScreen();
  }

  pspVideoPutImage(Screen, ScreenX, ScreenY, ScreenW, ScreenH);

  /* Show FPS counter */
  if (Options.ShowFps)
  {
    static char fps_display[32];
    sprintf(fps_display, " %3.02f", pspPerfGetFps(&Counter));

    int width = pspFontGetTextWidth(&PspStockFont, fps_display);
    int height = pspFontGetLineHeight(&PspStockFont);

    pspVideoFillRect(SCR_WIDTH - width, 0, SCR_WIDTH, height, PSP_COLOR_BLACK);
    pspVideoPrint(&PspStockFont, SCR_WIDTH - width, 0, fps_display, PSP_COLOR_WHITE);
  }

  pspVideoEnd();

  /* Wait for V. refresh */
  if (Options.VSync) pspVideoWaitVSync();

  /* Wait */
  if (Options.UpdateFreq)
  {
    u64 current_tick;
    do { sceRtcGetCurrentTick(&current_tick); }
    while (current_tick - LastTick < TicksPerUpdate);
    LastTick = current_tick;
  }

  pspVideoSwapBuffers();

	return (UBYTE*)Screen->Pixels;
}

/* Release emulation resources */
void TrashEmulation()
{
	if (Screen) pspImageDestroy(Screen);
}

/* Run emulation */
void RunEmulation()
{
  gAudioEnabled = TRUE;
  LynxSystem->DisplaySetAttributes(MIKIE_NO_ROTATE, MIKIE_PIXEL_FORMAT_16BPP_5551,
    Screen->Width * Screen->BytesPerPixel, DisplayCallback, 0);
  Screen->Viewport.Width = HANDY_SCREEN_WIDTH;

  float ratio;

  /* Recompute screen size/position */
  switch (Options.DisplayMode)
  {
  default:
  case DISPLAY_MODE_UNSCALED:
    ScreenW = Screen->Viewport.Width;
    ScreenH = Screen->Viewport.Height;
    break;
  case DISPLAY_MODE_FIT_HEIGHT:
    ratio = (float)SCR_HEIGHT / (float)Screen->Viewport.Height;
    ScreenW = (int)((float)Screen->Viewport.Width * ratio) - 2;
    ScreenH = SCR_HEIGHT;
    break;
  case DISPLAY_MODE_FILL_SCREEN:
    ScreenW = SCR_WIDTH - 3;
    ScreenH = SCR_HEIGHT;
    break;
  }

  ScreenX = (SCR_WIDTH / 2) - (ScreenW / 2);
  ScreenY = (SCR_HEIGHT / 2) - (ScreenH / 2);

  ClearScreen = 1;

  ULONG buttons;
	SceCtrlData pad;

	pspPerfInitFps(&Counter);

  /* Recompute update frequency */
  if (Options.UpdateFreq)
  {
    TicksPerSecond = sceRtcGetTickResolution();
    TicksPerUpdate = TicksPerSecond / Options.UpdateFreq;
    sceRtcGetCurrentTick(&LastTick);
  }

  /* Wait for V. refresh */
  pspVideoWaitVSync();

  /* Resume sound */
int foo=0;
//  pspAudioSetChannelCallback(0, AudioCallback, 0);
	while (!ExitPSP)
	{
		for (ULONG loop = 1024; loop; loop--)
			LynxSystem->Update();
if (!foo)
{
  pspAudioSetChannelCallback(0, AudioCallback, 0);
  foo=1;
}

		buttons = LynxSystem->GetButtonData();
		
		if (pspCtrlPollControls(&pad))
		{
      if ((pad.Buttons & PSP_CTRL_LTRIGGER) && (pad.Buttons & PSP_CTRL_RTRIGGER))
        break;
			
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

  /* Stop sound */
  pspAudioSetChannelCallback(0, NULL, 0);
}

static ULONG previous_buffer_pos = 0;

inline void AudioCallback(void *buffer, unsigned int *length, void *userdata)
{
sceKernelDelayThread(1000000/10);
  PspMonoSample *OutBuf = (PspMonoSample*)buffer;
  int i, play_length, played = 0;
  ULONG current_buffer_pos = gAudioBufferPointer;
  
  if (current_buffer_pos >= previous_buffer_pos)
  {
    /* Compute length */
    play_length = current_buffer_pos - previous_buffer_pos;
    *length = PSP_AUDIO_SAMPLE_TRUNCATE(play_length);

    /* If not enough data, render silence */
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

    /* If not enough data, render silence */
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
