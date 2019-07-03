Handy PSP
=========

Atari Lynx emulator

&copy; 2007 Akop Karapetyan  
&copy; 1994-2004 Keith Wilkins

New Features
------------

#### Version 0.95.1 (October 17 2007)

*   Initial release

Installation
------------

**PLEASE NOTE** Before you can play Lynx games, you will need to obtain `lynxboot.img`.

Unzip `handy.zip` into `/PSP/GAME/` folder on the memory stick. Copy `lynxboot.img` to `/PSP/GAME/HANDYPSP`.

Game ROM’s may reside anywhere, and can be ZIP\-compressed. The system boot ROM (`lynxboot.img`) must reside in the same directory as the main EBOOT.

Controls
--------

During emulation:

| PSP controls                    | Emulated controls            |
| ------------------------------- | ---------------------------- |
| Analog stick up/down/left/right | D-pad up/down/left/right     |
| D-pad up/down/left/right        | D-pad up/down/left/right     |
| [ ] (square)                    | Button A                     |
| X (cross)                       | Button B                     |
| Start                           | Option 1                     |
| Select                          | Option 2                     |
| [R]                             | Pause                        |
| [L] + [R]                       | Return to the emulator menu  |

By default, button configuration changes are not retained after button mapping is modified. To save changes, press X (cross) after desired mapping is configured. To load the default mapping press ^ (triangle).

Known issues
------------

Sound isn’t perfect—in some games - it may skip or pop occasionally.

Compiling
---------

To compile, ensure that [zlib](svn://svn.pspdev.org/psp/trunk/zlib) and [libpng](svn://svn.pspdev.org/psp/trunk/libpng) are installed, and run make:

`make -f Makefile.psp`

Version History
---------------

Initial release

Credits
-------

Keith Wilkins (Handy)
