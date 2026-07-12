#ifndef F15_SE2_EGTYPES
#define F15_SE2_EGTYPES
/* egame.exe hardware/comm/overlay constants. */

/* DOS calling-convention / pointer-size decorations (far/near/pascal/__cdecl/
 * __far/FAR/…) come from the single source, compat64/dos.h; many egame TUs
 * pick them up transitively through this header. */
#include <dos.h>
#include "inttype.h"

#define AIRCRAFT_MODELS_OFFSET 0xADD4
/* Total size of the g_world3dData region (main + photo models + appended
 * aircraft models); see the definition in egfarbuf.c. */
#define WORLD3D_DATA_SIZE (AIRCRAFT_MODELS_OFFSET + 0x520C)
#define IRQ_VIDEO 0x10
#define UNIT_STATE_COUNT 0x64

enum ViewMode : int16 {
    VIEW_COCKPIT = 0,
    VIEW_REAR = 0x41,
    VIEW_LEFT = 0x42,
    VIEW_RIGHT = 0x43,
    VIEW_FORWARD = 0x44, /* fullscreen forward, no cockpit */
    VIEW_EXT_DYNAMIC = 0x84,
    VIEW_EXT_SIDE = 0x85,
    VIEW_EXT_UNUSED = 0x86, /* external, similar to 0x88 */
    VIEW_EXT_FOLLOW = 0x87,
    VIEW_EXT_TARGET = 0x88, /* player aircraft towards target? */
    VIEW_MISSILE = 0x89,
    VIEW_TARGET = 0x8b,
    VIEW_EJECT = 0x8c
};

#endif /* F15_SE2_EGTYPES */
