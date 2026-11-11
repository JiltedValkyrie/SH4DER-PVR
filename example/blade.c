/* SH4DER-PVR: the blade effect

   example/blade.c
   Copyright (c) 2026 Jilted Valkyrie

   One sword blade, one frame: the shipped tuning at the top of a pulse,
   three vertices and their packed colours.
*/

#include <stdio.h>
#include "sh4der_blade.h"

int main(void) {
    /* blade vertices in the blade node's space: x along the blade */
    static const float blade[3][3] = {{0.20f, 0.0f, 0.06f}, {0.20f, 0.0f, -0.05f}, {0.33f, 0.0f, 0.01f}};
    static const float cyan[3] = {0.35f, 0.82f, 1.0f};   /* the blade texture's average, brightest channel 1 */
    sh4der_blade_params_t params;
    sh4der_blade_frame_t frame;
    uint32_t argb[3];
    unsigned i;

    sh4der_blade_params_default(&params);
    /* tick 48 + this sword's phase (0 here); fade = 1 - active camouflage */
    if (!sh4der_blade_prepare(&frame, &params, 48, 1.0f))
        return 1;
    sh4der_blade_argb(&frame, cyan, 0xff, blade[0], sizeof blade[0], argb, sizeof argb[0], 3);
    for (i = 0; i < 3; i++)
        printf("vertex %u: intensity %.4f argb %08lx\n", i, sh4der_blade_intensity(&frame, blade[i][0], blade[i][1], blade[i][2]),
               (unsigned long)argb[i]);
    printf("point light %.4f\n", frame.light);
    return 0;
}
