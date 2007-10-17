#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pspkernel.h>
#include <zlib.h>

#include "menu.h"
#include "emulate.h"

#include "video.h"
#include "image.h"
#include "ui.h"
#include "ctrl.h"
#include "psp.h"
#include "util.h"
#include "init.h"
#include "fileio.h"

#include "langres.h"

#include "System.h"

#define SYSTEM_SCRNSHOT     0x11
#define SYSTEM_RESET        0x12
#define SYSTEM_ROTATE       0x13
#define SYSTEM_SOUND        0x14

#define OPTION_DISPLAY_MODE 0x21
#define OPTION_SYNC_FREQ    0x22
#define OPTION_FRAMESKIP    0x23
#define OPTION_VSYNC        0x24
#define OPTION_CLOCK_FREQ   0x25
#define OPTION_SHOW_FPS     0x26
#define OPTION_CONTROL_MODE 0x27
#define OPTION_ANIMATE      0x28

#define TAB_QUICKLOAD 0
#define TAB_STATE     1
#define TAB_CONTROL   2
#define TAB_OPTION    3
#define TAB_SYSTEM    4
#define TAB_ABOUT     5
#define TAB_MAX       TAB_SYSTEM

extern struct ButtonConfig ActiveConfig;
extern PspImage *Screen;

EmulatorOptions Options;
CSystem *LynxSystem = NULL;

static int TabIndex;
static int ResumeEmulation;
static PspImage *Background;
static PspImage *NoSaveIcon;

static const char *QuickloadFilter[] = { "LNX", "ZIP", '\0' },
  PresentSlotText[] = RES_S_PRESENT_SLOT_TEXT,
  EmptySlotText[] = RES_S_EMPTY_SLOT_TEXT,
  ControlHelpText[] = RES_S_CONTROL_HELP_TEXT;

static const char *ScreenshotDir = "screens";
static const char *SaveStateDir = "savedata";
static const char *ButtonConfigFile = "buttons";
static const char *OptionsFile = "emulator.ini";

char *GameName;
char *ScreenshotPath;
static char *SaveStatePath;
static char *GamePath;

/* Tab labels */
static const char *TabLabel[] = 
{ RES_S_GAME_TAB, RES_S_SAVE_LOAD_TAB, RES_S_CONTROL_TAB, RES_S_OPTION_TAB,
RES_S_SYSTEM_TAB, RES_S_ABOUT_TAB };

static void LoadOptions();
static int  SaveOptions();

static void InitButtonConfig();
static int  SaveButtonConfig();
static int  LoadButtonConfig();

static void      DisplayStateTab();
static PspImage* LoadStateIcon(const char *path);
static int       LoadState(const char *path);
static PspImage* SaveState(const char *path, PspImage *icon, int angle_cw);

static int OnSplashButtonPress(const struct PspUiSplash *splash,
  u32 button_mask);
static void OnSplashRender(const void *uiobject, const void *null);

static void OnSystemRender(const void *uiobject, const void *item_obj);

static int OnQuickloadOk(const void *browser, const void *path);

static int OnGenericCancel(const void *uiobject, const void *param);
static void OnGenericRender(const void *uiobject, const void *item_obj);
static int OnGenericButtonPress(const PspUiFileBrowser *browser, 
  const char *path, u32 button_mask);

static int OnSaveStateOk(const void *gallery, const void *item);
static int OnSaveStateButtonPress(const PspUiGallery *gallery, 
  PspMenuItem* item, u32 button_mask);

static int OnMenuItemChanged(const struct PspUiMenu *uimenu, PspMenuItem* item, 
  const PspMenuOption* option);
static int OnMenuOk(const void *uimenu, const void* sel_item);
static int OnMenuButtonPress(const struct PspUiMenu *uimenu, 
  PspMenuItem* sel_item, u32 button_mask);

/* Define various menu options */
static const PspMenuOptionDef
  ToggleOptions[] = {
    MENU_OPTION(RES_S_DISABLED, 0),
    MENU_OPTION(RES_S_ENABLED,  1),
    MENU_END_OPTIONS
  },
  ScreenSizeOptions[] = {
    MENU_OPTION(RES_S_ACTUAL_SIZE, DISPLAY_MODE_UNSCALED),
    MENU_OPTION(RES_S_FIT_HEIGHT,  DISPLAY_MODE_FIT_HEIGHT),
    MENU_OPTION(RES_S_FIT_SCREEN,  DISPLAY_MODE_FILL_SCREEN),
    MENU_END_OPTIONS
  },
  FrameLimitOptions[] = {
    MENU_OPTION(RES_S_DISABLED,   0),
    MENU_OPTION(RES_S_60FPS_NTSC, 60),
    MENU_END_OPTIONS
  },
  FrameSkipOptions[] = {
    MENU_OPTION(RES_S_SKIP_FRAMES_0, 0),
    MENU_OPTION(RES_S_SKIP_FRAMES_1, 1),
    MENU_OPTION(RES_S_SKIP_FRAMES_2, 2),
    MENU_OPTION(RES_S_SKIP_FRAMES_3, 3),
    MENU_OPTION(RES_S_SKIP_FRAMES_4, 4),
    MENU_OPTION(RES_S_SKIP_FRAMES_5, 5),
    MENU_END_OPTIONS
  },
  PspClockFreqOptions[] = {
    MENU_OPTION(RES_S_222_MHZ, 222),
    MENU_OPTION(RES_S_266_MHZ, 266),
    MENU_OPTION(RES_S_300_MHZ, 300),
    MENU_OPTION(RES_S_333_MHZ, 333),
    MENU_END_OPTIONS
  },
  RotationOptions[] = {
    MENU_OPTION(RES_S_NOT_ROTATED,   MIKIE_NO_ROTATE),
    MENU_OPTION(RES_S_LEFT_ROTATED,  MIKIE_ROTATE_L),
    MENU_OPTION(RES_S_RIGHT_ROTATED, MIKIE_ROTATE_R),
    MENU_END_OPTIONS
  },
  ButtonMapOptions[] = {
    /* Unmapped */
    MENU_OPTION(RES_S_NONE, 0),
    /* Special */
    MENU_OPTION(RES_S_SPECIAL_OPEN_MENU, SPC|SPC_MENU),
    /* Joystick */
    MENU_OPTION(RES_S_DIR_PAD_UP,    JOY|BUTTON_UP),
    MENU_OPTION(RES_S_DIR_PAD_DOWN,  JOY|BUTTON_DOWN),
    MENU_OPTION(RES_S_DIR_PAD_LEFT,  JOY|BUTTON_LEFT),
    MENU_OPTION(RES_S_DIR_PAD_RIGHT, JOY|BUTTON_RIGHT),
    MENU_OPTION(RES_S_BUTTON_A,      JOY|BUTTON_A),
    MENU_OPTION(RES_S_BUTTON_B,      JOY|BUTTON_B),
    MENU_OPTION(RES_S_OPTION_1,      JOY|BUTTON_OPT1),
    MENU_OPTION(RES_S_OPTION_2,      JOY|BUTTON_OPT2),
    MENU_OPTION(RES_S_PAUSE,         JOY|BUTTON_PAUSE),
    MENU_END_OPTIONS
  },
  ControlModeOptions[] = {
    MENU_OPTION(RES_S_UI_US,    0),
    MENU_OPTION(RES_S_UI_JAPAN, 1),
    MENU_END_OPTIONS
  };

static const PspMenuItemDef
  ControlMenuDef[] = {
    MENU_ITEM(PSP_CHAR_ANALUP, CONTROL_ANALOG_UP, ButtonMapOptions, -1, ControlHelpText),
    MENU_ITEM(PSP_CHAR_ANALDOWN, CONTROL_ANALOG_DOWN, ButtonMapOptions, -1, ControlHelpText),
    MENU_ITEM(PSP_CHAR_ANALLEFT, CONTROL_ANALOG_LEFT, ButtonMapOptions, -1, ControlHelpText),
    MENU_ITEM(PSP_CHAR_ANALRIGHT, CONTROL_ANALOG_RIGHT, ButtonMapOptions, -1, ControlHelpText),
    MENU_ITEM(PSP_CHAR_UP, CONTROL_BUTTON_UP, ButtonMapOptions, -1, ControlHelpText),
    MENU_ITEM(PSP_CHAR_DOWN, CONTROL_BUTTON_DOWN, ButtonMapOptions, -1, ControlHelpText),
    MENU_ITEM(PSP_CHAR_LEFT, CONTROL_BUTTON_LEFT, ButtonMapOptions, -1, ControlHelpText),
    MENU_ITEM(PSP_CHAR_RIGHT, CONTROL_BUTTON_RIGHT, ButtonMapOptions, -1, ControlHelpText),
    MENU_ITEM(PSP_CHAR_SQUARE, CONTROL_BUTTON_SQUARE, ButtonMapOptions, -1, ControlHelpText),
    MENU_ITEM(PSP_CHAR_CROSS, CONTROL_BUTTON_CROSS, ButtonMapOptions, -1, ControlHelpText),
    MENU_ITEM(PSP_CHAR_CIRCLE, CONTROL_BUTTON_CIRCLE, ButtonMapOptions, -1, ControlHelpText),
    MENU_ITEM(PSP_CHAR_TRIANGLE, CONTROL_BUTTON_TRIANGLE, ButtonMapOptions, -1, ControlHelpText),
    MENU_ITEM(PSP_CHAR_LTRIGGER, CONTROL_BUTTON_LTRIGGER, ButtonMapOptions, -1, ControlHelpText),
    MENU_ITEM(PSP_CHAR_RTRIGGER, CONTROL_BUTTON_RTRIGGER, ButtonMapOptions, -1, ControlHelpText),
    MENU_ITEM(PSP_CHAR_SELECT, CONTROL_BUTTON_SELECT, ButtonMapOptions, -1, ControlHelpText),
    MENU_ITEM(PSP_CHAR_START, CONTROL_BUTTON_START, ButtonMapOptions, -1, ControlHelpText),
    MENU_ITEM(PSP_CHAR_LTRIGGER"+"PSP_CHAR_RTRIGGER, CONTROL_BUTTON_LRTRIGGERS, ButtonMapOptions, -1, ControlHelpText),
    MENU_ITEM(PSP_CHAR_START"+"PSP_CHAR_SELECT, CONTROL_BUTTON_STARTSELECT, ButtonMapOptions, -1, ControlHelpText),
    MENU_END_ITEMS
  },
  OptionMenuDef[] = {
    MENU_HEADER(RES_S_VIDEO),
    MENU_ITEM(RES_S_SCREEN_SIZE, OPTION_DISPLAY_MODE, ScreenSizeOptions, -1,
      RES_S_CHANGE_SCREEN_SIZE),
    MENU_HEADER(RES_S_PERF),
    MENU_ITEM(RES_S_FRAME_LIMITER, OPTION_SYNC_FREQ, FrameLimitOptions, -1,
      RES_S_CHANGE_SCR_UPD_FREQ),
    MENU_ITEM(RES_S_FRAME_LIMITER, OPTION_FRAMESKIP, FrameSkipOptions, -1,
      RES_S_CHANGE_FRAME_SKIP_PER_UPD),
    MENU_ITEM(RES_S_VSYNC, OPTION_VSYNC, ToggleOptions, -1,
      RES_S_ENABLE_TO_REDUCE_TEARING),
    MENU_ITEM(RES_S_PSP_CLOCK_FREQ, OPTION_CLOCK_FREQ, PspClockFreqOptions, -1,
      RES_S_LARGER_FASTER_DEPL),
    MENU_ITEM(RES_S_SHOW_FPS,    OPTION_SHOW_FPS, ToggleOptions, -1,
      RES_S_SHOW_HIDE_FPS),
    MENU_HEADER(RES_S_MENU),
    MENU_ITEM(RES_S_UI_MODE, OPTION_CONTROL_MODE, ControlModeOptions,  -1,
      RES_S_CHANGE_OK_CANCEL),
    MENU_ITEM(RES_S_ANIMATIONS, OPTION_ANIMATE, ToggleOptions,  -1,
      RES_S_ENABLE_DISABLE_ANIM),
    MENU_END_ITEMS
  },
  SystemMenuDef[] = {
    MENU_HEADER(RES_S_AUDIO),
    MENU_ITEM(RES_S_SOUND, SYSTEM_SOUND, ToggleOptions, -1,
      RES_S_ENABLE_DISABLE_SOUND),
    MENU_HEADER(RES_S_VIDEO),
    MENU_ITEM(RES_S_SCREEN_ORIENT, SYSTEM_ROTATE, RotationOptions, -1,
      RES_S_CHANGE_SCREEN_ORIENT),
    MENU_HEADER(RES_S_SYSTEM),
    MENU_ITEM(RES_S_RESET, SYSTEM_RESET, NULL, -1, RES_S_RESET_HELP),
    MENU_ITEM(RES_S_SAVE_SCR,  SYSTEM_SCRNSHOT, NULL, -1, RES_S_SAVE_SCR_HELP),
    MENU_END_ITEMS
  };

PspUiGallery SaveStateGallery = 
{
  NULL,                        /* PspMenu */
  OnGenericRender,             /* OnRender() */
  OnSaveStateOk,               /* OnOk() */
  OnGenericCancel,             /* OnCancel() */
  OnSaveStateButtonPress,      /* OnButtonPress() */
  NULL                         /* Userdata */
};

PspUiMenu OptionUiMenu =
{
  NULL,                  /* PspMenu */
  OnGenericRender,       /* OnRender() */
  OnMenuOk,              /* OnOk() */
  OnGenericCancel,       /* OnCancel() */
  OnMenuButtonPress,     /* OnButtonPress() */
  OnMenuItemChanged,     /* OnItemChanged() */
};

PspUiSplash SplashScreen =
{
  OnSplashRender,
  OnGenericCancel,
  OnSplashButtonPress,
  NULL
};

PspUiFileBrowser QuickloadBrowser = 
{
  OnGenericRender,
  OnQuickloadOk,
  OnGenericCancel,
  OnGenericButtonPress,
  QuickloadFilter,
  0
};

PspUiMenu SystemUiMenu =
{
  NULL,                  /* PspMenu */
  OnSystemRender,        /* OnRender() */
  OnMenuOk,              /* OnOk() */
  OnGenericCancel,       /* OnCancel() */
  OnMenuButtonPress,     /* OnButtonPress() */
  OnMenuItemChanged,     /* OnItemChanged() */
};

PspUiMenu ControlUiMenu =
{
  NULL,                  /* PspMenu */
  OnGenericRender,       /* OnRender() */
  OnMenuOk,              /* OnOk() */
  OnGenericCancel,       /* OnCancel() */
  OnMenuButtonPress,     /* OnButtonPress() */
  OnMenuItemChanged,     /* OnItemChanged() */
};

/* Default configuration */
struct ButtonConfig DefaultConfig =
{
  {
    JOY|BUTTON_UP,    /* Analog Up    */
    JOY|BUTTON_DOWN,  /* Analog Down  */
    JOY|BUTTON_LEFT,  /* Analog Left  */
    JOY|BUTTON_RIGHT, /* Analog Right */
    JOY|BUTTON_UP,    /* D-pad Up     */
    JOY|BUTTON_DOWN,  /* D-pad Down   */
    JOY|BUTTON_LEFT,  /* D-pad Left   */
    JOY|BUTTON_RIGHT, /* D-pad Right  */
    JOY|BUTTON_A,     /* Square       */
    JOY|BUTTON_B,     /* Cross        */
    0,                /* Circle       */
    0,                /* Triangle     */
    0,                /* L Trigger    */
    JOY|BUTTON_PAUSE, /* R Trigger    */
    JOY|BUTTON_OPT2,  /* Select       */
    JOY|BUTTON_OPT1,  /* Start        */
    SPC|SPC_MENU,     /* L+R Triggers */
    0,                /* Start+Select */
  }
};

int InitMenu()
{
  /* Reset variables */
  TabIndex = TAB_ABOUT;
  Background = NULL;
  GameName = NULL;
  GamePath = NULL;

  /* Initialize options */
  LoadOptions();

  if (!InitEmulation())
    return 0;

  /* Load the background image */
  Background = pspImageLoadPng("background.png");

  /* Init NoSaveState icon image */
  NoSaveIcon=pspImageCreate(HANDY_SCREEN_WIDTH, HANDY_SCREEN_HEIGHT,
    PSP_IMAGE_16BPP);
  pspImageClear(NoSaveIcon, RGB(0x85,0x45,0x1a));

  /* Initialize paths */
  SaveStatePath 
    = (char*)malloc(sizeof(char) * (strlen(pspGetAppDirectory()) + strlen(SaveStateDir) + 2));
  sprintf(SaveStatePath, "%s%s/", pspGetAppDirectory(), SaveStateDir);
  ScreenshotPath 
    = (char*)malloc(sizeof(char) * (strlen(pspGetAppDirectory()) + strlen(ScreenshotDir) + 2));
  sprintf(ScreenshotPath, "%s%s/", pspGetAppDirectory(), ScreenshotDir);

  /* Initialize control menu */
  ControlUiMenu.Menu = pspMenuCreate();
  pspMenuLoad(ControlUiMenu.Menu, ControlMenuDef);

  /* Initialize options menu */
  OptionUiMenu.Menu = pspMenuCreate();
  pspMenuLoad(OptionUiMenu.Menu, OptionMenuDef);

  /* Initialize state menu */
  SaveStateGallery.Menu = pspMenuCreate();
  int i;
  PspMenuItem *item;
  for (i = 0; i < 10; i++)
  {
    item = pspMenuAppendItem(SaveStateGallery.Menu, NULL, i);
    pspMenuSetHelpText(item, EmptySlotText);
  }

  /* Initialize system menu */
  SystemUiMenu.Menu = pspMenuCreate();
  pspMenuLoad(SystemUiMenu.Menu, SystemMenuDef);

  /* Load default configuration */
  LoadButtonConfig();

  /* Initialize UI components */
  UiMetric.Background = Background;
  UiMetric.Font = &PspStockFont;
  UiMetric.Left = 8;
  UiMetric.Top = 24;
  UiMetric.Right = 472;
  UiMetric.Bottom = 250;
  UiMetric.OkButton = (!Options.ControlMode) ? PSP_CTRL_CROSS : PSP_CTRL_CIRCLE;
  UiMetric.CancelButton = (!Options.ControlMode) ? PSP_CTRL_CIRCLE : PSP_CTRL_CROSS;
  UiMetric.ScrollbarColor = PSP_COLOR_GRAY;
  UiMetric.ScrollbarBgColor = 0x44ffffff;
  UiMetric.ScrollbarWidth = 10;
  UiMetric.TextColor = PSP_COLOR_GRAY;
  UiMetric.SelectedColor = PSP_COLOR_YELLOW;
  UiMetric.SelectedBgColor = COLOR(0xff,0xff,0xff,0x66);
  UiMetric.StatusBarColor = PSP_COLOR_WHITE;
  UiMetric.BrowserFileColor = PSP_COLOR_GRAY;
  UiMetric.BrowserDirectoryColor = PSP_COLOR_YELLOW;
  UiMetric.GalleryIconsPerRow = 5;
  UiMetric.GalleryIconMarginWidth = 16;
  UiMetric.MenuItemMargin = 20;
  UiMetric.MenuSelOptionBg = COLOR(0x85,0x45,0x1a,0xee);
  UiMetric.MenuOptionBoxColor = PSP_COLOR_GRAY;
  UiMetric.MenuOptionBoxBg = COLOR(0x85,0x45,0x1a,0xdd);
  UiMetric.MenuDecorColor = PSP_COLOR_YELLOW;
  UiMetric.DialogFogColor = UiMetric.MenuOptionBoxBg;
  UiMetric.TitlePadding = 4;
  UiMetric.TitleColor = PSP_COLOR_WHITE;
  UiMetric.MenuFps = 30;
  UiMetric.TabBgColor = COLOR(0xf2,0xdc,0xcd,0xff);

  return 1;
}

void DisplayMenu()
{
  int i;
  PspMenuItem *item;

  /* Menu loop */
  do
  {
    ResumeEmulation = 0;

    /* Set normal clock frequency */
    pspSetClockFrequency(222);
    /* Set buttons to autorepeat */
    pspCtrlSetPollingMode(PSP_CTRL_AUTOREPEAT);

    /* Display appropriate tab */
    switch (TabIndex)
    {
    case TAB_QUICKLOAD:
      pspUiOpenBrowser(&QuickloadBrowser, (GameName) ? GameName : GamePath);
      break;
    case TAB_SYSTEM:
      item = pspMenuFindItemById(SystemUiMenu.Menu, SYSTEM_ROTATE);
      pspMenuSelectOptionByValue(item, (void*)Options.Rotation);
      item = pspMenuFindItemById(SystemUiMenu.Menu, SYSTEM_SOUND);
      pspMenuSelectOptionByValue(item, (void*)Options.SoundEnabled);

      pspUiOpenMenu(&SystemUiMenu, NULL);
      break;
    case TAB_STATE:
      DisplayStateTab();
      break;
    case TAB_OPTION:
      /* Init menu options */
      item = pspMenuFindItemById(OptionUiMenu.Menu, OPTION_DISPLAY_MODE);
      pspMenuSelectOptionByValue(item, (void*)Options.DisplayMode);
      item = pspMenuFindItemById(OptionUiMenu.Menu, OPTION_SYNC_FREQ);
      pspMenuSelectOptionByValue(item, (void*)Options.UpdateFreq);
      item = pspMenuFindItemById(OptionUiMenu.Menu, OPTION_FRAMESKIP);
      pspMenuSelectOptionByValue(item, (void*)(int)Options.Frameskip);
      item = pspMenuFindItemById(OptionUiMenu.Menu, OPTION_VSYNC);
      pspMenuSelectOptionByValue(item, (void*)Options.VSync);
      item = pspMenuFindItemById(OptionUiMenu.Menu, OPTION_CLOCK_FREQ);
      pspMenuSelectOptionByValue(item, (void*)Options.ClockFreq);
      item = pspMenuFindItemById(OptionUiMenu.Menu, OPTION_SHOW_FPS);
      pspMenuSelectOptionByValue(item, (void*)Options.ShowFps);
      item = pspMenuFindItemById(OptionUiMenu.Menu, OPTION_CONTROL_MODE);
      pspMenuSelectOptionByValue(item, (void*)Options.ControlMode);
      item = pspMenuFindItemById(OptionUiMenu.Menu, OPTION_ANIMATE);
      pspMenuSelectOptionByValue(item, (void*)UiMetric.Animate);

      pspUiOpenMenu(&OptionUiMenu, NULL);
      break;
    case TAB_CONTROL:
      /* Load current button mappings */
      for (item = ControlUiMenu.Menu->First, i = 0; item; item = item->Next, i++)
        pspMenuSelectOptionByValue(item, (void*)ActiveConfig.ButtonMap[i]);
      pspUiOpenMenu(&ControlUiMenu, NULL);
      break;
    case TAB_ABOUT:
      pspUiSplashScreen(&SplashScreen);
      break;
    }

    if (!ExitPSP)
    {
      /* Set clock frequency during emulation */
      pspSetClockFrequency(Options.ClockFreq);
      /* Set buttons to normal mode */
      pspCtrlSetPollingMode(PSP_CTRL_NORMAL);

      /* Resume emulation */
      if (ResumeEmulation)
      {
        if (UiMetric.Animate) pspUiFadeout();
        RunEmulation();
        if (UiMetric.Animate) pspUiFadeout();
      }
    }
  } while (!ExitPSP);
}

int OnGenericCancel(const void *uiobject, const void* param)
{
  if (GameName) ResumeEmulation = 1;
  return 1;
}

void OnSplashRender(const void *splash, const void *null)
{
  int fh, i, x, y, height;
  const char *lines[] = 
  { 
    RES_S_APP_NAME,
    "\026http://psp.akop.org/handy",
    " ",
    "2007 Akop Karapetyan (port)",
    "1996-2004 Keith Wilkins (emulation)",
    NULL
  };

  fh = pspFontGetLineHeight(UiMetric.Font);

  for (i = 0; lines[i]; i++);
  height = fh * (i - 1);

  /* Render lines */
  for (i = 0, y = SCR_HEIGHT / 2 - height / 2; lines[i]; i++, y += fh)
  {
    x = SCR_WIDTH / 2 - pspFontGetTextWidth(UiMetric.Font, lines[i]) / 2;
    pspVideoPrint(UiMetric.Font, x, y, lines[i], PSP_COLOR_GRAY);
  }

  /* Render PSP status */
  OnGenericRender(splash, null);
}

/* Handles any special drawing for the system menu */
void OnSystemRender(const void *uiobject, const void *item_obj)
{
  int w, h, x, y;
  w = Screen->Viewport.Width;
  h = Screen->Viewport.Height;
  x = SCR_WIDTH - w - 8;
  y = (SCR_HEIGHT/2) - (h/2);

  /* Draw a small representation of the screen */
  pspVideoShadowRect(x, y, x + w - 1, y + h - 1, PSP_COLOR_BLACK, 3);
  pspVideoPutImage(Screen, x, y, w, h);
  pspVideoDrawRect(x, y, x + w - 1, y + h - 1, PSP_COLOR_GRAY);

  OnGenericRender(uiobject, item_obj);
}

int  OnSplashButtonPress(const struct PspUiSplash *splash, 
  u32 button_mask)
{
  return OnGenericButtonPress(NULL, NULL, button_mask);
}

/* Handles drawing of generic items */
void OnGenericRender(const void *uiobject, const void *item_obj)
{
  /* Draw tabs */
  int height = pspFontGetLineHeight(UiMetric.Font);
  int width;
  int i, x;
  for (i = 0, x = 5; i <= TAB_MAX; i++, x += width + 10)
  {
    width = -10;

    if (!GameName && (i == TAB_STATE || i == TAB_SYSTEM))
      continue;

    /* Determine width of text */
    width = pspFontGetTextWidth(UiMetric.Font, TabLabel[i]);

    /* Draw background of active tab */
    if (i == TabIndex)
      pspVideoFillRect(x - 5, 0, x + width + 5, height + 1, UiMetric.TabBgColor);

    /* Draw name of tab */
    pspVideoPrint(UiMetric.Font, x, 0, TabLabel[i], PSP_COLOR_WHITE);
  }
}

int OnGenericButtonPress(const PspUiFileBrowser *browser, 
  const char *path, u32 button_mask)
{
  int tab_index;

  /* If L or R are pressed, switch tabs */
  if (button_mask & PSP_CTRL_LTRIGGER)
  {
    TabIndex--;
    do
    {
      tab_index = TabIndex;
      if (!GameName && (TabIndex == TAB_STATE || TabIndex == TAB_SYSTEM)) TabIndex--;
      if (TabIndex < 0) TabIndex = TAB_MAX;
    } while (tab_index != TabIndex);
  }
  else if (button_mask & PSP_CTRL_RTRIGGER)
  {
    TabIndex++;
    do
    {
      tab_index = TabIndex;
      if (!GameName && (TabIndex == TAB_STATE || TabIndex == TAB_SYSTEM)) TabIndex++;
      if (TabIndex > TAB_MAX) TabIndex = 0;
    } while (tab_index != TabIndex);
  }
  else if ((button_mask & (PSP_CTRL_START | PSP_CTRL_SELECT)) 
    == (PSP_CTRL_START | PSP_CTRL_SELECT))
  {
    if (pspUtilSaveVramSeq(ScreenshotPath, "ui")) pspUiAlert(RES_S_SAVED_SUCC);
    else pspUiAlert(RES_S_ERROR_NOT_SAVED);
    return 0;
  }
  else return 0;

  return 1;
}

int  OnMenuItemChanged(const struct PspUiMenu *uimenu, PspMenuItem* item,
  const PspMenuOption* option)
{
  if (uimenu == &ControlUiMenu)
  {
    ActiveConfig.ButtonMap[item->ID] = (unsigned int)option->Value;
  }
  else
  {
    switch(item->ID)
    {
    case OPTION_DISPLAY_MODE:
      Options.DisplayMode = (int)option->Value;
      break;
    case OPTION_SYNC_FREQ:
      Options.UpdateFreq = (int)option->Value;
      break;
    case OPTION_FRAMESKIP:
      Options.Frameskip = (int)option->Value;
      break;
    case OPTION_VSYNC:
      Options.VSync = (int)option->Value;
      break;
    case OPTION_CLOCK_FREQ:
      Options.ClockFreq = (int)option->Value;
      break;
    case OPTION_SHOW_FPS:
      Options.ShowFps = (int)option->Value;
      break;
    case OPTION_CONTROL_MODE:
      Options.ControlMode = (int)option->Value;
      UiMetric.CancelButton = ((int)option->Value) ? PSP_CTRL_CROSS : PSP_CTRL_CIRCLE;
      UiMetric.OkButton = ((int)option->Value) ? PSP_CTRL_CIRCLE : PSP_CTRL_CROSS;
      break;
    case OPTION_ANIMATE:
      UiMetric.Animate = (int)option->Value;
      break;
    case SYSTEM_ROTATE:
      Options.Rotation = (int)option->Value;
      break;
    case SYSTEM_SOUND:
      Options.SoundEnabled = (int)option->Value;
      break;
    }
  }

  return 1;
}

int OnMenuOk(const void *uimenu, const void* sel_item)
{
  if (uimenu == &ControlUiMenu)
  {
    /* Save to MS */
    if (SaveButtonConfig()) pspUiAlert(RES_S_SAVED_SUCC);
    else pspUiAlert(RES_S_ERROR_NOT_SAVED);
  }
  else if (uimenu == &SystemUiMenu)
  {
    switch (((const PspMenuItem*)sel_item)->ID)
    {
    case SYSTEM_RESET:

      /* Reset system */
      if (pspUiConfirm(RES_S_RESET_SYSTEM))
      {
        LynxSystem->Reset();
        ResumeEmulation = 1;
        return 1;
      }
      break;

    case SYSTEM_SCRNSHOT:

      /* Save screenshot */
      if (pspUtilSavePngSeq(ScreenshotPath, pspFileIoGetFilename(GameName), Screen))
        pspUiAlert(RES_S_SAVED_SUCC);
      else pspUiAlert(RES_S_ERROR_NOT_SAVED);
      break;
    }
  }
  
  return 0;
}

int OnMenuButtonPress(const struct PspUiMenu *uimenu, PspMenuItem* sel_item,
  u32 button_mask)
{
  if (uimenu == &ControlUiMenu)
  {
    if (button_mask & PSP_CTRL_TRIANGLE)
    {
      PspMenuItem *item;
      int i;

      /* Load default mapping */
      InitButtonConfig();

      /* Modify the menu */
      for (item = ControlUiMenu.Menu->First, i = 0; item; item = item->Next, i++)
        pspMenuSelectOptionByValue(item, (void*)DefaultConfig.ButtonMap[i]);

      return 0;
    }
  }

  return OnGenericButtonPress(NULL, NULL, button_mask);
}

int OnSaveStateOk(const void *gallery, const void *item)
{
  if (!GameName) { TabIndex++; return 0; }

  char *path;
  const char *config_name = pspFileIoGetFilename(GameName);

  path = (char*)malloc(strlen(SaveStatePath) + strlen(config_name) + 8);
  sprintf(path, "%s%s.s%02i", SaveStatePath, config_name,
    ((const PspMenuItem*)item)->ID);

  if (pspFileIoCheckIfExists(path) && pspUiConfirm(RES_S_LOAD_STATE))
  {
    if (LoadState(path))
    {
      ResumeEmulation = 1;
      pspMenuFindItemById(((const PspUiGallery*)gallery)->Menu,
        ((const PspMenuItem*)item)->ID);
      free(path);

      return 1;
    }

    pspUiAlert(RES_S_ERROR_LOADING_STATE);
  }

  free(path);
  return 0;
}

int OnSaveStateButtonPress(const PspUiGallery *gallery, 
      PspMenuItem *sel, u32 button_mask)
{
  if (!GameName) { TabIndex++; return 0; }

  if (button_mask & PSP_CTRL_SQUARE 
    || button_mask & PSP_CTRL_TRIANGLE)
  {
    char *path;
    char caption[32];
    const char *config_name = pspFileIoGetFilename(GameName);
    PspMenuItem *item = pspMenuFindItemById(gallery->Menu, sel->ID);

    path = (char*)malloc(strlen(SaveStatePath) + strlen(config_name) + 8);
    sprintf(path, "%s%s.s%02i", SaveStatePath, config_name, item->ID);

    do /* not a real loop; flow control construct */
    {
      if (button_mask & PSP_CTRL_SQUARE)
      {
        if (pspFileIoCheckIfExists(path) && !pspUiConfirm(RES_S_OVERWRITE_STATE))
          break;

        pspUiFlashMessage(RES_S_SAVING_WAIT);

        PspImage *icon;
        int angle_cw = 0;
        ULONG rotation = LynxSystem->DisplayGetRotation();

        /* Rotate icons for rotated screens */
        if (rotation == MIKIE_ROTATE_R) angle_cw = 90;
        else if (rotation == MIKIE_ROTATE_L) angle_cw = 270;

        if (!(icon = SaveState(path, Screen, angle_cw)))
        {
          pspUiAlert(RES_S_ERROR_NOT_SAVED);
          break;
        }

        SceIoStat stat;

        /* Trash the old icon (if any) */
        if (item->Icon && item->Icon != NoSaveIcon)
          pspImageDestroy((PspImage*)item->Icon);

        /* Update icon, help text */
        item->Icon = icon;
        pspMenuSetHelpText(item, PresentSlotText);

        /* Get file modification time/date */
        if (sceIoGetstat(path, &stat) < 0)
          sprintf(caption, RES_S_ERROR);
        else
          sprintf(caption, "%02i/%02i/%02i %02i:%02i", 
            stat.st_mtime.month,
            stat.st_mtime.day,
            stat.st_mtime.year - (stat.st_mtime.year / 100) * 100,
            stat.st_mtime.hour,
            stat.st_mtime.minute);

        pspMenuSetCaption(item, caption);
      }
      else if (button_mask & PSP_CTRL_TRIANGLE)
      {
        if (!pspFileIoCheckIfExists(path) || !pspUiConfirm(RES_S_DELETE_STATE))
          break;

        if (!pspFileIoDelete(path))
        {
          pspUiAlert(RES_S_ERROR_STATE_NOT_DEL);
          break;
        }

        /* Trash the old icon (if any) */
        if (item->Icon && item->Icon != NoSaveIcon)
          pspImageDestroy((PspImage*)item->Icon);

        /* Update icon, caption */
        item->Icon = NoSaveIcon;
        pspMenuSetHelpText(item, EmptySlotText);
        pspMenuSetCaption(item, RES_S_EMPTY);
      }
    } while (0);

    if (path) free(path);
    return 0;
  }

  return OnGenericButtonPress(NULL, NULL, button_mask);
}

int OnQuickloadOk(const void *browser, const void *path)
{
  char *system_rom
    = (char*)malloc(sizeof(char) * (strlen(pspGetAppDirectory()) + 13));
  sprintf(system_rom, "%slynxboot.img", pspGetAppDirectory());

  CSystem *system;

  try
  {
    system = new CSystem((char*)(const char*)path, system_rom);
  }
  catch(CLynxException &err)
  {
    free(system_rom);
    pspUiAlert(err.mMsg);
    return 0;
  }

  if (GameName) free(GameName);
  GameName = strdup((const char*)path);
  if (GamePath) free(GamePath);
  GamePath = pspFileIoGetParentDirectory(GameName);

  free(system_rom);
  if (LynxSystem) delete LynxSystem;
  LynxSystem = system;

  ResumeEmulation = 1;
  return 1;
}

/* Load options */
void LoadOptions()
{
  char *path = (char*)malloc(sizeof(char) * (strlen(pspGetAppDirectory()) + strlen(OptionsFile) + 1));
  sprintf(path, "%s%s", pspGetAppDirectory(), OptionsFile);

  /* Initialize INI structure */
  PspInit *init = pspInitCreate();

  /* Read the file */
  pspInitLoad(init, path);

  /* Load values */
  Options.DisplayMode = pspInitGetInt(init, "Video", "Display Mode", DISPLAY_MODE_UNSCALED);
  Options.UpdateFreq = pspInitGetInt(init, "Video", "Update Frequency", 60);
  Options.Frameskip = pspInitGetInt(init, "Video", "Frameskip", 1);
  Options.VSync = pspInitGetInt(init, "Video", "VSync", 0);
  Options.ClockFreq = pspInitGetInt(init, "Video", "PSP Clock Frequency", 222);
  Options.ShowFps = pspInitGetInt(init, "Video", "Show FPS", 0);
  Options.ControlMode = pspInitGetInt(init, "Menu", "Control Mode", 0);
  UiMetric.Animate = pspInitGetInt(init, "Menu", "Animate", 1);
  
  Options.SoundEnabled = pspInitGetInt(init, "System", "Sound Enabled", 1);
  Options.Rotation = pspInitGetInt(init, "System", "Rotation", MIKIE_NO_ROTATE);

  if (GamePath) free(GamePath);
  GamePath = pspInitGetString(init, "File", "Game Path", NULL);

  /* Clean up */
  free(path);
  pspInitDestroy(init);
}

/* Save options */
static int SaveOptions()
{
  char *path = (char*)malloc(sizeof(char) * (strlen(pspGetAppDirectory()) + strlen(OptionsFile) + 1));
  sprintf(path, "%s%s", pspGetAppDirectory(), OptionsFile);

  /* Initialize INI structure */
  PspInit *init = pspInitCreate();

  /* Set values */
  pspInitSetInt(init, "Video", "Display Mode", Options.DisplayMode);
  pspInitSetInt(init, "Video", "Update Frequency", Options.UpdateFreq);
  pspInitSetInt(init, "Video", "Frameskip", Options.Frameskip);
  pspInitSetInt(init, "Video", "VSync", Options.VSync);
  pspInitSetInt(init, "Video", "PSP Clock Frequency",Options.ClockFreq);
  pspInitSetInt(init, "Video", "Show FPS", Options.ShowFps);
  
  pspInitSetInt(init, "Menu", "Control Mode", Options.ControlMode);
  pspInitSetInt(init, "Menu", "Animate", UiMetric.Animate);

  pspInitSetInt(init, "System", "Rotation", Options.Rotation);
  pspInitSetInt(init, "System", "Sound Enabled", Options.SoundEnabled);

  if (GamePath) pspInitSetString(init, "File", "Game Path", GamePath);

  /* Save INI file */
  int status = pspInitSave(init, path);

  /* Clean up */
  pspInitDestroy(init);
  free(path);

  return status;
}

/* Initialize game configuration */
static void InitButtonConfig()
{
  memcpy(&ActiveConfig, &DefaultConfig, sizeof(struct ButtonConfig));
}

/* Load game configuration */
static int LoadButtonConfig()
{
  char *path;
  if (!(path = (char*)malloc(sizeof(char) * (strlen(pspGetAppDirectory()) + strlen(ButtonConfigFile) + 6))))
    return 0;
  sprintf(path, "%s%s.cnf", pspGetAppDirectory(), ButtonConfigFile);

  /* Open file for reading */
  FILE *file = fopen(path, "r");
  free(path);

  /* If no configuration, load defaults */
  if (!file)
  {
    InitButtonConfig();
    return 1;
  }

  /* Read contents of struct */
  int nread = fread(&ActiveConfig, sizeof(struct ButtonConfig), 1, file);
  fclose(file);

  if (nread != 1)
  {
    InitButtonConfig();
    return 0;
  }

  return 1;
}

/* Save game configuration */
static int SaveButtonConfig()
{
  char *path;
  if (!(path = (char*)malloc(sizeof(char) * (strlen(pspGetAppDirectory()) + strlen(ButtonConfigFile) + 6))))
    return 0;
  sprintf(path, "%s%s.cnf", pspGetAppDirectory(), ButtonConfigFile);

  /* Open file for writing */
  FILE *file = fopen(path, "w");
  free(path);
  if (!file) return 0;

  /* Write contents of struct */
  int nwritten = fwrite(&ActiveConfig, sizeof(struct ButtonConfig), 1, file);
  fclose(file);

  return (nwritten == 1);
}

static void DisplayStateTab()
{
  if (!GameName) { TabIndex++; return; }

  PspMenuItem *item;
  SceIoStat stat;
  char caption[32];

  const char *config_name = pspFileIoGetFilename(GameName);
  char *path = (char*)malloc(strlen(SaveStatePath) + strlen(config_name) + 8);
  char *game_name = strdup(config_name);
  char *dot = strrchr(game_name, '.');
  if (dot) *dot='\0';

  /* Initialize icons */
  for (item = SaveStateGallery.Menu->First; item; item = item->Next)
  {
    sprintf(path, "%s%s.s%02i", SaveStatePath, config_name, item->ID);

    if (pspFileIoCheckIfExists(path))
    {
      if (sceIoGetstat(path, &stat) < 0)
        sprintf(caption, RES_S_ERROR);
      else
        sprintf(caption, "%02i/%02i/%02i %02i:%02i",
          stat.st_mtime.month,
          stat.st_mtime.day,
          stat.st_mtime.year - (stat.st_mtime.year / 100) * 100,
          stat.st_mtime.hour,
          stat.st_mtime.minute);

      pspMenuSetCaption(item, caption);
      item->Icon = LoadStateIcon(path);
      pspMenuSetHelpText(item, PresentSlotText);
    }
    else
    {
      pspMenuSetCaption(item, RES_S_EMPTY);
      item->Icon = NoSaveIcon;
      pspMenuSetHelpText(item, EmptySlotText);
    }
  }

  free(path);
  pspUiOpenGallery(&SaveStateGallery, game_name);
  free(game_name);

  /* Destroy any icons */
  for (item = SaveStateGallery.Menu->First; item; item = item->Next)
    if (item->Icon != NULL && item->Icon != NoSaveIcon)
      pspImageDestroy((PspImage*)item->Icon);
}

/* Load state icon */
PspImage* LoadStateIcon(const char *path)
{
  /* Open file for reading */
  FILE *f = fopen(path, "r");
  if (!f) return NULL;

  /* Load image */
  PspImage *image = pspImageLoadPngFd(f);
  fclose(f);

  return image;
}

/* Load state */
int LoadState(const char *path)
{
  /* Open file for reading */
  FILE *f = fopen(path, "r");
  if (!f) return 0;

  /* Load image into temporary object */
  PspImage *image = pspImageLoadPngFd(f);
  if (!image) return 0;
  pspImageDestroy(image);

  /* Flush the file stream */
  fflush(f);

  gzFile z = gzdopen(fileno(f), "r");
  int status = LynxSystem->ContextLoad((FILE*)z);
  gzclose(z);
  fclose(f);

  return status;
}

/* Save state */
PspImage* SaveState(const char *path, PspImage *icon, int angle_cw)
{
  /* Open file for writing */
  FILE *f;
  if (!(f = fopen(path, "w")))
    return NULL;

  /* Create thumbnail */
  PspImage *rotated = pspImageRotate(icon, angle_cw);
  if (!rotated) { fclose(f); return NULL; }

  /* Write the thumbnail */
  if (!pspImageSavePngFd(f, rotated))
  {
    pspImageDestroy(rotated);
    fclose(f);
    return NULL;
  }

  /* Flush the file stream */
  fflush(f);

  /* Save state */
  gzFile z = gzdopen(fileno(f), "w");
  int status = LynxSystem->ContextSave((FILE*)z);
  if (!status)
  {
    pspImageDestroy(rotated);
    rotated = NULL;
  }

  gzclose(z);
  fclose(f);

  return rotated;
}

void TrashMenu()
{
  /* Save options */
  SaveOptions();

  /* Free emulation-specific resources */
  TrashEmulation();

  /* Destroy the emulator */
  if (LynxSystem) delete LynxSystem;

  /* Trash menus */
  pspMenuDestroy(OptionUiMenu.Menu);
  pspMenuDestroy(SystemUiMenu.Menu);
  pspMenuDestroy(SaveStateGallery.Menu);
  pspMenuDestroy(ControlUiMenu.Menu);

  /* Trash images */
  if (Background) pspImageDestroy(Background);
  if (NoSaveIcon) pspImageDestroy(NoSaveIcon);

  if (GameName) free(GameName);
  if (GamePath) free(GamePath);

  free(ScreenshotPath);
  free(SaveStatePath);
}
