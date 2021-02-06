#pragma once

#if defined(GC9307)

// Data specific to the ILI9341 controller (unsure if needed for GC9307)
#define DISPLAY_SET_CURSOR_X 0x2A
#define DISPLAY_SET_CURSOR_Y 0x2B
#define DISPLAY_WRITE_PIXELS 0x2C

// set width and height (might experience some incorrect offset)
#define DISPLAY_NATIVE_WIDTH 240
#define DISPLAY_NATIVE_HEIGHT 210

// Not sure what this does
#define DISPLAY_NATIVE_COVERED_LEFT_SIDE 2
#define DISPLAY_NATIVE_COVERED_TOP_SIDE 1

//definine init function
#define InitSPIDisplay InitGC9307
void InitGC9307(void);

void TurnDisplayOn(void);
void TurnDisplayOff(void);

// Not needed
//#define DISPLAY_NEEDS_CHIP_SELECT_SIGNAL

#endif
