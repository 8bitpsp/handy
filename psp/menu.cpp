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

int InitMenu()
{
  if (!InitEmulation())
    return 0;

  return 1;
}

void DisplayMenu()
{
	RunEmulation();
}

void TrashMenu()
{
  /* Free emulation-specific resources */
  TrashEmulation();
}

