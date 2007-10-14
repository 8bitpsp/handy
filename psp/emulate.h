#ifndef _EMULATE_H
#define _EMULATE_H

int  InitEmulation();
void RunEmulation();
void TrashEmulation();

#define MAPPABLE_BUTTONS 18

struct ButtonConfig
{
  unsigned int ButtonMap[MAPPABLE_BUTTONS];
};

#endif

