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
#include "util.h"
#include "fileio.h"

#include "menu.h"
#include "emulate.h"

#include "lynxdef.h"
#include "System.h"

extern CSystem *LynxSystem;
extern EmulatorOptions Options;
extern char *GameName;
extern char *ScreenshotPath;

/* Control configuration */
struct ButtonConfig ActiveConfig;
PspImage *Screen = NULL;

/* Button masks */
static const u64 ButtonMask[] = 
{
  PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER, 
  PSP_CTRL_START    | PSP_CTRL_SELECT,
  PSP_CTRL_ANALUP,    PSP_CTRL_ANALDOWN,
  PSP_CTRL_ANALLEFT,  PSP_CTRL_ANALRIGHT,
  PSP_CTRL_UP,        PSP_CTRL_DOWN,
  PSP_CTRL_LEFT,      PSP_CTRL_RIGHT,
  PSP_CTRL_SQUARE,    PSP_CTRL_CROSS,
  PSP_CTRL_CIRCLE,    PSP_CTRL_TRIANGLE,
  PSP_CTRL_LTRIGGER,  PSP_CTRL_RTRIGGER,
  PSP_CTRL_SELECT,    PSP_CTRL_START,
  0 /* End */
};

static unsigned int TicksPerUpdate;
static u32 TicksPerSecond;
static u64 LastTick;
static int ScreenX;
static int ScreenY;
static int ScreenW;
static int ScreenH;
static int VSyncDelay;
static int ClearScreen;
static PspFpsCounter Counter;
static int Frame;

static inline int ParseInput();
static UBYTE* DisplayCallback(ULONG objref);
static void AudioCallback(void *buffer, unsigned int *samples, void *userdata);

/* Initialize emulation */
int InitEmulation()
{
  pspPerfInitFps(&Counter);

	if (!(Screen = pspImageCreateVram(256,
    max(HANDY_SCREEN_WIDTH, HANDY_SCREEN_HEIGHT), PSP_IMAGE_16BPP)))
		  return 0;

  return 1;
}

UBYTE* DisplayCallback(ULONG objref)
{
  /* Frame skipping */
  if (++Frame <= Options.Frameskip) return 0;
  else Frame = 0;

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

/* Toggle emulated buttons */
int ParseInput()
{
  static SceCtrlData pad;
  ULONG temp_buttons, buttons;

  temp_buttons = LynxSystem->GetButtonData();

  /* Unset the buttons */
  buttons = temp_buttons &= ~(BUTTON_UP|BUTTON_DOWN|BUTTON_LEFT|BUTTON_RIGHT
    |BUTTON_PAUSE|BUTTON_A|BUTTON_B|BUTTON_OPT1|BUTTON_OPT2);

  /* Check the input */
  if (pspCtrlPollControls(&pad))
  {
#ifdef PSP_DEBUG
    if ((pad.Buttons & (PSP_CTRL_SELECT | PSP_CTRL_START))
      == (PSP_CTRL_SELECT | PSP_CTRL_START))
        pspUtilSaveVramSeq(ScreenshotPath, pspFileIoGetFilename(GameName));
#endif

    /* Parse input */
    int i, on, code;
    for (i = 0; ButtonMapId[i] >= 0; i++)
    {
      code = ActiveConfig.ButtonMap[ButtonMapId[i]];
      on = (pad.Buttons & ButtonMask[i]) == ButtonMask[i];

      /* Check to see if a button set is pressed. If so, unset it, so it */
      /* doesn't trigger any other combination presses. */
      if (on) pad.Buttons |= ButtonMask[i];

      if (code & JOY)
      {
        if (on) temp_buttons |= CODE_MASK(code);
      }
      else if (code & SPC)
      {
        switch (CODE_MASK(code))
        {
        case SPC_MENU:
          if (on) return 1;
          break;
        }
      }
    }

    /* Flip controls, if rotated */
    switch (LynxSystem->DisplayGetRotation())
    {
    case MIKIE_ROTATE_L:
      if (temp_buttons & BUTTON_UP) buttons |= BUTTON_LEFT;
      else if (temp_buttons & BUTTON_DOWN) buttons |= BUTTON_RIGHT;
      if (temp_buttons & BUTTON_LEFT) buttons |= BUTTON_DOWN;
      else if (temp_buttons & BUTTON_RIGHT) buttons |= BUTTON_UP;
      break;
    case MIKIE_ROTATE_R:
      if (temp_buttons & BUTTON_UP) buttons |= BUTTON_RIGHT;
      else if (temp_buttons & BUTTON_DOWN) buttons |= BUTTON_LEFT;
      if (temp_buttons & BUTTON_LEFT) buttons |= BUTTON_UP;
      else if (temp_buttons & BUTTON_RIGHT) buttons |= BUTTON_DOWN;
      break;
    default:
      if (temp_buttons & BUTTON_UP) buttons |= BUTTON_UP;
      else if (temp_buttons & BUTTON_DOWN) buttons |= BUTTON_DOWN;
      if (temp_buttons & BUTTON_LEFT) buttons |= BUTTON_LEFT;
      else if (temp_buttons & BUTTON_RIGHT) buttons |= BUTTON_RIGHT;
      break;
    }

    /* Set the non-directional controls */
    buttons |= temp_buttons
      & (BUTTON_PAUSE|BUTTON_A|BUTTON_B|BUTTON_OPT1|BUTTON_OPT2);
  }

  LynxSystem->SetButtonData(buttons);

  return 0;
}

/* Run emulation */
void RunEmulation()
{
  switch(Options.Rotation)
  {
  case MIKIE_NO_ROTATE:
    Screen->Viewport.Width = HANDY_SCREEN_WIDTH;
    Screen->Viewport.Height = HANDY_SCREEN_HEIGHT;
    break;
  case MIKIE_ROTATE_L:
  case MIKIE_ROTATE_R:
    Screen->Viewport.Width = HANDY_SCREEN_HEIGHT;
    Screen->Viewport.Height = HANDY_SCREEN_WIDTH;
    break;
  }

  LynxSystem->DisplaySetAttributes(Options.Rotation, MIKIE_PIXEL_FORMAT_16BPP_5551,
    Screen->Width * Screen->BytesPerPixel, DisplayCallback, 0);

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
  Frame = 0;

	pspPerfInitFps(&Counter);

  /* Recompute update frequency */
  TicksPerSecond = sceRtcGetTickResolution();
  sceRtcGetCurrentTick(&LastTick);
  VSyncDelay = pspVideoGetVSyncFreq() * TicksPerSecond;

  if (Options.UpdateFreq) TicksPerUpdate = TicksPerSecond
    / (Options.UpdateFreq / (Options.Frameskip + 1));

  if ((gAudioEnabled = Options.SoundEnabled))
    pspAudioSetChannelCallback(0, AudioCallback, 0);

  /* Wait for V. refresh */
  pspVideoWaitVSync();

  /* Main emulation loop */
	while (!ExitPSP)
	{
		for (ULONG loop = 1024; loop; loop--)
			LynxSystem->Update();

    if (ParseInput()) break;
	}

  /* Stop sound */
  if (Options.SoundEnabled)
    pspAudioSetChannelCallback(0, NULL, 0);
}

// inline?
void AudioCallback(void *buffer, unsigned int *length, void *userdata)
{
  PspMonoSample *OutBuf = (PspMonoSample*)buffer;
  int i;
  int len = *length >> 1;

  if(((int)gAudioBufferPointer >= len)
    && (gAudioBufferPointer != 0) && (!gSystemHalt) )
  {
    for (i = 0; i < len; i++)
    {
      short sample = (short)(((int)gAudioBuffer[i] << 8) - 32768);
      (OutBuf++)->Channel = sample;
      (OutBuf++)->Channel = sample;
    }
    gAudioBufferPointer = 0;
  }
  else
  {
    *length = 64;
    for (i = 0; i < (int)*length; i+=2)
    {
      (OutBuf++)->Channel = 0;
      (OutBuf++)->Channel = 0;
    }
  }
}
