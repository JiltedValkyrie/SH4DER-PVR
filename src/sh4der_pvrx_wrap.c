/* SH4DER-PVR: pvrx, the optional link-time wrap of pvr_scene_finish

   src/sh4der_pvrx_wrap.c
   Version: v0.1 PROTOTYPE, NOT RUN on a console
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   Why: the polled render-done wait (sh4der_pvrx_wait_render_done_poll) waits for one render-done event a scene that
   was counted, and only sh4der_pvrx_scene_finish counts. A scene ended with a plain pvr_scene_finish is not counted,
   so the polled wait returns while its render still runs (DERIVED from reading src/sh4der_pvrx.c, NOT run). With this
   file in the program and -Wl,--wrap=pvr_scene_finish on the link line, every pvr_scene_finish in the binary (a game's,
   another library's) is counted. KallistiOS is not edited: the linker renames the references.

   Use: add this file to the program's sources and -Wl,--wrap=pvr_scene_finish to its link flags, both or neither:
   one without the other fails to link (an undefined __real_ or __wrap_ symbol), it never half works. Not in the
   library archive's default objects. The count is only used while sh4der_pvrx_wait_hook(1) is on.
   Dreamcast only. */

#ifdef _arch_dreamcast

#include <kos.h>

#include "sh4der_pvrx.h"

int __real_pvr_scene_finish(void);

int __wrap_pvr_scene_finish(void) {
    const int r = __real_pvr_scene_finish();

    sh4der_pvrx_scene_noted();
    return r;
}

#endif /* _arch_dreamcast */
