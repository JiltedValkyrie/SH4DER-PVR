/* <Game> on Dreamcast, title screen skeleton: a VQ still and the title music (AICA ADPCM, streamed from disc).
 * Assets come from tools/build_assets.py (the owner's own disc); nothing from the disc is in this repo.
 * Cost (example numbers from a reference title screen; measure yours): RAM ~144 KB (128 KB read-ahead ring + 16 KB stage), AICA ~64 KB (2 x 32 KB ring), VRAM ~130 KB (1024x512 VQ), disc 4.4 MB + 130 KB.
 * KOS path: PVR direct poly submit (one textured quad, pvr_prim), snd_stream with ADPCM (hardware decode, KOS DMA split), controller via maple. */
#include <kos.h>
#include <dc/sound/stream.h>
#include <dc/sound/sound.h>
#include <dc/maple/controller.h>
#include <dc/pvr.h>

KOS_INIT_FLAGS(INIT_DEFAULT);

#define MUSIC_PATH   "/cd/TITLE.ADP"
#define STILL_PATH   "/cd/TITLE.DT"
#define MUSIC_RATE   48000
#define STAGE_BYTES  (16 * 1024)        /* one disc read; CD sector multiple */
#define RING_BYTES   (SND_STREAM_BUFFER_MAX_ADPCM)   /* per channel in AICA RAM, 32 KB - 64 */

static file_t music_fd = FILEHND_INVALID;
static uint8_t *stage;                          /* what the fill callback hands to snd_stream (copy of ring bytes, never a disc read) */
static volatile int quit;

/* Read-ahead ring (Halo lesson, docs/AUDIO.md): a reader thread owns every disc read, the snd_stream poll only copies from RAM.
 * Single producer (reader) / single consumer (poll thread): the block counters are the only shared state. */
#define RING_BLOCKS 8                           /* 8 x 16 KB = 128 KB RAM = 0.7 s of 48 kHz stereo ADPCM */
static uint8_t *ring;
static volatile uint32_t blk_wr, blk_rd;        /* blocks produced / fully consumed */
static uint32_t cons_off;                       /* bytes already taken from block blk_rd (consumer only) */
static volatile uint32_t underruns;

static void *reader_thread(void *arg) {
    (void)arg;
    while (!quit) {
        if (blk_wr - blk_rd >= RING_BLOCKS) { thd_sleep(10); continue; }
        uint8_t *dst = ring + (blk_wr % RING_BLOCKS) * STAGE_BYTES;
        uint32_t have = 0;
        while (have < STAGE_BYTES) {
            ssize_t n = fs_read(music_fd, dst + have, STAGE_BYTES - have);
            if (n <= 0) {
                if (fs_seek(music_fd, 0, SEEK_SET) < 0) return NULL;   /* wrap: the loop point is the file start */
                continue;
            }
            have += (uint32_t)n;
        }
        __sync_synchronize();
        blk_wr = blk_wr + 1;
    }
    return NULL;
}

/* fill callback: ADPCM stereo interleaved, bytes asked for both channels. Copies from the ring only. */
static void *music_fill(snd_stream_hnd_t hnd, int req, int *got) {
    (void)hnd;
    if (req > STAGE_BYTES) req = STAGE_BYTES;
    int have = 0;
    while (have < req && blk_rd != blk_wr) {
        uint32_t take = STAGE_BYTES - cons_off;
        if (take > (uint32_t)(req - have)) take = (uint32_t)(req - have);
        memcpy(stage + have, ring + (blk_rd % RING_BLOCKS) * STAGE_BYTES + cons_off, take);
        have += (int)take;
        cons_off += take;
        if (cons_off == STAGE_BYTES) { cons_off = 0; __sync_synchronize(); blk_rd = blk_rd + 1; }
    }
    if (have < req) ++underruns;                /* ring ran dry: the stream gets fewer bytes (the rest is silence) */
    *got = have;
    return stage;
}

/* The stream poll runs on its own thread; it never touches the disc. */
static void *music_thread(void *arg) {
    snd_stream_hnd_t h = *(snd_stream_hnd_t *)arg;
    while (!quit) {
        snd_stream_poll(h);
        thd_sleep(20);
    }
    return NULL;
}

/* DT file (tools/build_assets.py, pvrtex): 32-byte header (header_size at +9, codebook+index data after it), see KOS utils/pvrtex/file_dctex.h */
static pvr_ptr_t still_load(uint32_t *mode) {
    file_t f = fs_open(STILL_PATH, O_RDONLY);
    if (f < 0) return NULL;
    size_t total = fs_total(f);
    uint8_t *buf = memalign(32, total);
    if (!buf || fs_read(f, buf, total) != (ssize_t)total) { free(buf); fs_close(f); return NULL; }
    fs_close(f);
    uint32_t off = ((uint32_t)buf[9] + 1) * 32;
    memcpy(mode, buf + 16, 4);
    pvr_ptr_t tex = pvr_mem_malloc(total - off);
    if (tex) pvr_txr_load(buf + off, tex, total - off);
    free(buf);
    return tex;
}

static void quad(pvr_poly_hdr_t *hdr, float x0, float y0, float x1, float y1, uint32_t argb) {
    pvr_prim(hdr, sizeof(*hdr));
    pvr_vertex_t v = { .flags = PVR_CMD_VERTEX, .z = 1.0f, .argb = argb, .oargb = 0 };
    v.x = x0; v.y = y0; v.u = 0.0f; v.v = 0.0f; pvr_prim(&v, sizeof(v));
    v.x = x1; v.y = y0; v.u = 1.0f; v.v = 0.0f; pvr_prim(&v, sizeof(v));
    v.x = x0; v.y = y1; v.u = 0.0f; v.v = 1.0f; pvr_prim(&v, sizeof(v));
    v.flags = PVR_CMD_VERTEX_EOL;
    v.x = x1; v.y = y1; v.u = 1.0f; v.v = 1.0f; pvr_prim(&v, sizeof(v));
}

int main(void) {
    pvr_init_defaults();
    vid_set_mode(DM_640x480, PM_RGB565);

    uint32_t mode = 0;
    pvr_ptr_t tex = still_load(&mode);
    printf("TITLE: still %s, VRAM free %lu KB\n", tex ? "loaded" : "MISSING", (unsigned long)(pvr_mem_available() / 1024));

    pvr_poly_cxt_t cxt;
    pvr_poly_hdr_t hdr;
    pvr_poly_cxt_txr(&cxt, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_VQ_ENABLE | PVR_TXRFMT_TWIDDLED, 1024, 512, tex, PVR_FILTER_BILINEAR);
    cxt.txr.env = PVR_TXRENV_MODULATE;
    cxt.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&hdr, &cxt);

    snd_stream_hnd_t hnd = SND_STREAM_INVALID;
    stage = memalign(32, STAGE_BYTES);
    ring = memalign(32, RING_BLOCKS * STAGE_BYTES);
    music_fd = fs_open(MUSIC_PATH, O_RDONLY);
    if (stage && ring && music_fd >= 0) {          /* prime the ring before the stream starts: its first fill must not wait on the disc */
        thd_create(true, reader_thread, NULL);
        while (blk_wr < RING_BLOCKS && !quit) thd_sleep(5);
    }
    if (stage && ring && music_fd >= 0 && snd_stream_init_ex(2, RING_BYTES) >= 0) {
        hnd = snd_stream_alloc(music_fill, RING_BYTES);
        if (hnd != SND_STREAM_INVALID) {
            snd_stream_start_adpcm(hnd, MUSIC_RATE, 1);
            thd_create(true, music_thread, &hnd);
        }
    }
    printf("TITLE: music %s\n", hnd != SND_STREAM_INVALID ? "streaming" : "UNAVAILABLE");

    uint32_t frame = 0;
    while (!quit) {
        maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        if (dev) {
            cont_state_t *st = (cont_state_t *)maple_dev_status(dev);
            if (st && (st->buttons & CONT_START)) quit = 1;
        }
        /* fade in over 90 frames: modulate colour 0 -> 1 */
        float k = frame < 90 ? frame / 90.0f : 1.0f;
        uint32_t c = (uint32_t)(k * 255.0f);
        uint32_t argb = 0xff000000u | (c << 16) | (c << 8) | c;
        pvr_wait_ready();
        pvr_scene_begin();
        pvr_list_begin(PVR_LIST_OP_POLY);
        if (tex) quad(&hdr, 0.0f, 60.0f, 640.0f, 420.0f, argb);   /* 16:9 still, letterboxed on 640x480 */
        pvr_list_finish();
        pvr_scene_finish();
        ++frame;
    }

    quit = 1;
    printf("TITLE: music underruns %lu\n", (unsigned long)underruns);
    if (hnd != SND_STREAM_INVALID) { snd_stream_stop(hnd); snd_stream_destroy(hnd); }
    snd_stream_shutdown();
    if (music_fd >= 0) fs_close(music_fd);
    if (tex) pvr_mem_free(tex);
    return 0;
}
