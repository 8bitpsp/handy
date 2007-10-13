#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pspkernel.h>

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

#include "System.h"

#define SYSTEM_SCRNSHOT     11
#define SYSTEM_RESET        12
#define SYSTEM_ROTATE       13

#define OPTION_DISPLAY_MODE 21
#define OPTION_SYNC_FREQ    22
#define OPTION_FRAMESKIP    23
#define OPTION_VSYNC        24
#define OPTION_CLOCK_FREQ   25
#define OPTION_SHOW_FPS     26
#define OPTION_CONTROL_MODE 27
#define OPTION_ANIMATE      28

#define TAB_QUICKLOAD 0
/*
#define TAB_STATE     1
#define TAB_CONTROL   2
#define TAB_OPTION    3
#define TAB_SYSTEM    4
#define TAB_ABOUT     5
#define TAB_MAX       TAB_SYSTEM
*/
#define TAB_STATE     1
#define TAB_OPTION    2
#define TAB_SYSTEM    3
#define TAB_ABOUT     4
#define TAB_MAX       TAB_SYSTEM

extern PspImage *Screen;

EmulatorOptions Options;
CSystem *LynxSystem = NULL;

static int TabIndex;
static int ResumeEmulation;
static PspImage *Background;
static PspImage *NoSaveIcon;

static const char *QuickloadFilter[] = { "LNX", "ZIP", '\0' },
  PresentSlotText[] = "\026\244\020 Save\t\026\001\020 Load\t\026\243\020 Delete",
  EmptySlotText[] = "\026\244\020 Save",
  ControlHelpText[] = "\026\250\020 Change mapping\t\026\001\020 Save to \271\t\026\243\020 Load defaults";

static const char *ScreenshotDir = "screens";
static const char *SaveStateDir = "savedata";
static const char *ButtonConfigFile = "buttons";
static const char *OptionsFile = "handy.ini";

char *GameName;
char *ScreenshotPath;
static char *SaveStatePath;
static char *GamePath;

/* Tab labels */
static const char *TabLabel[] = 
{
  "Game",
  "Save/Load",
/*
  "Controls",
*/
  "Options",
  "System",
  "About"
};

static void LoadOptions();
static int  SaveOptions();

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
    MENU_OPTION("Disabled", 0),
    MENU_OPTION("Enabled",  1),
    MENU_END_OPTIONS
  },
  ScreenSizeOptions[] = {
    MENU_OPTION("Actual size", DISPLAY_MODE_UNSCALED),
    MENU_OPTION("Fit height",  DISPLAY_MODE_FIT_HEIGHT),
    MENU_OPTION("Fit screen",  DISPLAY_MODE_FILL_SCREEN),
    MENU_END_OPTIONS
  },
  FrameLimitOptions[] = {
    MENU_OPTION("Disabled",      0),
    MENU_OPTION("60 fps (NTSC)", 60),
    MENU_END_OPTIONS
  },
  FrameSkipOptions[] = {
    MENU_OPTION("No skipping",  0),
    MENU_OPTION("Skip 1 frame", 1),
    MENU_OPTION("Skip 2 frames",2),
    MENU_OPTION("Skip 3 frames",3),
    MENU_OPTION("Skip 4 frames",4),
    MENU_OPTION("Skip 5 frames",5),
    MENU_END_OPTIONS
  },
  PspClockFreqOptions[] = {
    MENU_OPTION("222 MHz", 222),
    MENU_OPTION("266 MHz", 266),
    MENU_OPTION("300 MHz", 300),
    MENU_OPTION("333 MHz", 333),
    MENU_END_OPTIONS
  },
  RotationOptions[] = {
    MENU_OPTION("Not rotated", MIKIE_NO_ROTATE),
    MENU_OPTION("Left-rotated", MIKIE_ROTATE_L),
    MENU_OPTION("Right-rotated", MIKIE_ROTATE_R),
    MENU_END_OPTIONS
  },
  ControlModeOptions[] = {
    MENU_OPTION("\026\242\020 cancels, \026\241\020 confirms (US)",    0),
    MENU_OPTION("\026\241\020 cancels, \026\242\020 confirms (Japan)", 1),
    MENU_END_OPTIONS
  };

static const PspMenuItemDef
  OptionMenuDef[] = {
    MENU_HEADER("Video"),
    MENU_ITEM("Screen size", OPTION_DISPLAY_MODE, ScreenSizeOptions, -1,
      "\026\250\020 Change screen size"),
    MENU_HEADER("Performance"),
    MENU_ITEM("Frame limiter", OPTION_SYNC_FREQ, FrameLimitOptions, -1, 
      "\026\250\020 Change screen update frequency"),
    MENU_ITEM("Frame skipping", OPTION_FRAMESKIP, FrameSkipOptions, -1, 
      "\026\250\020 Change number of frames skipped per update"),
    MENU_ITEM("VSync", OPTION_VSYNC, ToggleOptions, -1,
      "\026\250\020 Enable to reduce tearing; disable to increase speed"),
    MENU_ITEM("PSP clock frequency", OPTION_CLOCK_FREQ, PspClockFreqOptions, -1,
      "\026\250\020 Larger values: faster emulation, faster battery depletion (default: 222MHz)"),
    MENU_ITEM("Show FPS counter",    OPTION_SHOW_FPS, ToggleOptions, -1,
      "\026\250\020 Show/hide the frames-per-second counter"),
    MENU_HEADER("Menu"),
    MENU_ITEM("Button mode", OPTION_CONTROL_MODE, ControlModeOptions,  -1, 
      "\026\250\020 Change OK and Cancel button mapping"),
    MENU_ITEM("Animations", OPTION_ANIMATE, ToggleOptions,  -1, 
      "\026\250\020 Enable/disable in-menu animations"),
    MENU_END_ITEMS
  },
  SystemMenuDef[] = {
    MENU_HEADER("Video"),
    MENU_ITEM("Screen orientation", SYSTEM_ROTATE, RotationOptions, -1,
      "\026\001\020 Change screen orientation"),
    MENU_HEADER("System"),
    MENU_ITEM("Reset", SYSTEM_RESET, NULL, -1, "\026\001\020 Reset"),
    MENU_ITEM("Save screenshot",  SYSTEM_SCRNSHOT, NULL, -1, 
      "\026\001\020 Save screenshot"),
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
  pspImageClear(NoSaveIcon, RGB(0x0c,0,0x3f));

  /* Initialize paths */
  SaveStatePath 
    = (char*)malloc(sizeof(char) * (strlen(pspGetAppDirectory()) + strlen(SaveStateDir) + 2));
  sprintf(SaveStatePath, "%s%s/", pspGetAppDirectory(), SaveStateDir);
  ScreenshotPath 
    = (char*)malloc(sizeof(char) * (strlen(pspGetAppDirectory()) + strlen(ScreenshotDir) + 2));
  sprintf(ScreenshotPath, "%s%s/", pspGetAppDirectory(), ScreenshotDir);

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
  UiMetric.SelectedBgColor = COLOR(0xff,0xff,0xff,0x44);
  UiMetric.StatusBarColor = PSP_COLOR_WHITE;
  UiMetric.BrowserFileColor = PSP_COLOR_GRAY;
  UiMetric.BrowserDirectoryColor = PSP_COLOR_YELLOW;
  UiMetric.GalleryIconsPerRow = 5;
  UiMetric.GalleryIconMarginWidth = 16;
  UiMetric.MenuItemMargin = 20;
  UiMetric.MenuSelOptionBg = PSP_COLOR_BLACK;
  UiMetric.MenuOptionBoxColor = PSP_COLOR_GRAY;
  UiMetric.MenuOptionBoxBg = COLOR(0, 0, 33, 0xBB);
  UiMetric.MenuDecorColor = PSP_COLOR_YELLOW;
  UiMetric.DialogFogColor = COLOR(0, 0, 0, 88);
  UiMetric.TitlePadding = 4;
  UiMetric.TitleColor = PSP_COLOR_WHITE;
  UiMetric.MenuFps = 30;
  UiMetric.TabBgColor = COLOR(0x74,0x74,0xbe,0xff);

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
    PSP_APP_NAME" version "PSP_APP_VER" ("__DATE__")",
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
    if (pspUtilSaveVramSeq(ScreenshotPath, "ui"))
      pspUiAlert("Saved successfully");
    else
      pspUiAlert("ERROR: Not saved");
    return 0;
  }
  else return 0;

  return 1;
}

int  OnMenuItemChanged(const struct PspUiMenu *uimenu, PspMenuItem* item,
  const PspMenuOption* option)
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
    UiMetric.OkButton = (!(int)option->Value) ? PSP_CTRL_CROSS
      : PSP_CTRL_CIRCLE;
    UiMetric.CancelButton = (!(int)option->Value) ? PSP_CTRL_CIRCLE
      : PSP_CTRL_CROSS;
    break;
  case OPTION_ANIMATE:
    UiMetric.Animate = (int)option->Value;
    break;
  case SYSTEM_ROTATE:
    Options.Rotation = (int)option->Value;
    break;
  }

  return 1;
}

int OnMenuOk(const void *uimenu, const void* sel_item)
{
  if (uimenu == &SystemUiMenu)
  {
    switch (((const PspMenuItem*)sel_item)->ID)
    {
    case SYSTEM_RESET:

      /* Reset system */
      if (pspUiConfirm("Reset the system?"))
      {
        LynxSystem->Reset();
        ResumeEmulation = 1;
        return 1;
      }
      break;

    case SYSTEM_SCRNSHOT:

      /* Save screenshot */
      if (!pspUtilSavePngSeq(ScreenshotPath, pspFileIoGetFilename(GameName), Screen))
        pspUiAlert("ERROR: Screenshot not saved");
      else
        pspUiAlert("Screenshot saved successfully");
      break;
    }
  }
  
  return 0;
}

int OnMenuButtonPress(const struct PspUiMenu *uimenu, PspMenuItem* sel_item,
  u32 button_mask)
{
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

  if (pspFileIoCheckIfExists(path) && pspUiConfirm("Load state?"))
  {
    if (LoadState(path))
    {
      ResumeEmulation = 1;
      pspMenuFindItemById(((const PspUiGallery*)gallery)->Menu,
        ((const PspMenuItem*)item)->ID);
      free(path);

      return 1;
    }
    pspUiAlert("ERROR: State failed to load");
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
        if (pspFileIoCheckIfExists(path) && !pspUiConfirm("Overwrite existing state?"))
          break;

        pspUiFlashMessage("Saving, please wait ...");

        PspImage *icon;
        int angle_cw = 0;
        ULONG rotation = LynxSystem->DisplayGetRotation();

        /* Rotate icons for rotated screens */
        if (rotation == MIKIE_ROTATE_R) angle_cw = 90;
        else if (rotation == MIKIE_ROTATE_L) angle_cw = 270;

        if (!(icon = SaveState(path, Screen, angle_cw)))
        {
          pspUiAlert("ERROR: State not saved");
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
          sprintf(caption, "ERROR");
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
        if (!pspFileIoCheckIfExists(path) || !pspUiConfirm("Delete state?"))
          break;

        if (!pspFileIoDelete(path))
        {
          pspUiAlert("ERROR: State not deleted");
          break;
        }

        /* Trash the old icon (if any) */
        if (item->Icon && item->Icon != NoSaveIcon)
          pspImageDestroy((PspImage*)item->Icon);

        /* Update icon, caption */
        item->Icon = NoSaveIcon;
        pspMenuSetHelpText(item, EmptySlotText);
        pspMenuSetCaption(item, "Empty");
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

  if (GamePath) pspInitSetString(init, "File", "Game Path", GamePath);

  /* Save INI file */
  int status = pspInitSave(init, path);

  /* Clean up */
  pspInitDestroy(init);
  free(path);

  return status;
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
        sprintf(caption, "ERROR");
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
      pspMenuSetCaption(item, "Empty");
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
  pspImageDestroy(image);

  LynxSystem->ContextLoad(f);
  fclose(f);

  return 1;
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

  /* Save state */
  LynxSystem->ContextSave(f);

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

  /* Trash images */
  if (Background) pspImageDestroy(Background);
  if (NoSaveIcon) pspImageDestroy(NoSaveIcon);

  if (GameName) free(GameName);
  if (GamePath) free(GamePath);

  free(ScreenshotPath);
  free(SaveStatePath);
}
