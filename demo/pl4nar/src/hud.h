/* HUD: text from the baked 8 x 16 font, bars, a map square. Translucent list only. */
#ifndef HUD_H
#define HUD_H
#include <stdint.h>

void hud_text(float x, float y, float scale, uint32_t argb, const char *s);     /* with a 1 px drop shadow */
void hud_bar(float x, float y, float w, float h, float frac, uint32_t argb, const char *label);
void hud_map(float x, float y, float size, float nx, float nz, float heading);  /* nx -1 .. 1 across the street, nz 0 .. 1 along the loop */
#endif
