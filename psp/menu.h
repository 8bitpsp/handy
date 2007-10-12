#ifndef _MENU_H
#define _MENU_H

int  InitMenu();
void DisplayMenu();
void TrashMenu();

#define DISPLAY_MODE_UNSCALED    0
#define DISPLAY_MODE_FIT_HEIGHT  1
#define DISPLAY_MODE_FILL_SCREEN 2

typedef struct
{
  int ShowFps;
  int ControlMode;
  int ClockFreq;
  int DisplayMode;
  int VSync;
  int UpdateFreq;
  int Frameskip;
  int Rotation;
} EmulatorOptions;

#endif // _MENU_H
