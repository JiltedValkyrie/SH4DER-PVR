# SH4DER-PVR (SH4DOW-DC is included, in the folder sh4dow-dc; SH4DOW= names another checkout)
#
#   make test                       host check of the plain C path: test/core_test, test/blade_test, test/holo_test, test/bloom_test, test/look_test, test/taa_test, test/frame_test, test/glow_test, test/warp_test, test/trail_test, test/lens_test, test/focus_test, test/visor_test, test/torch_test, test/ao_test, test/guard_test, test/sss_test, test/spark_test, test/bolt_test, test/motes_test, test/deform_test, test/pl4nar_test, test/pool_test, test/psm_test, test/psm_raster_test, test/psm_pvr_test, example/blade
#   make test-shz SH4ZAM=<checkout> host check of the SH4ZAM path (SH4ZAM's software back-end, 0.10 or later): every test
#   make dc SH4ZAM=<checkout>       Dreamcast library build/libsh4der_core.a (KallistiOS environment sourced)
#   make test-dc SH4ZAM=<checkout>  build/*_test.elf: every test on the Dreamcast path
#   make demo SH4ZAM=<checkout>     the demo disc build/demo/sh4der-demo.cdi (KallistiOS environment sourced; needs python and mkdcdisc)
#   make dc DEMO=1 SH4ZAM=<checkout> the library, and with the flag DEMO=1 the demo disc as well
#   make clean

CC      ?= cc
CFLAGS  ?= -O3
WARN     = -std=c99 -Wall -Wextra
BUILD    = build
SRC      = src/sh4der_core.c src/sh4der_blade.c src/sh4der_holo.c src/sh4der_bloom.c src/sh4der_glow.c src/sh4der_frame.c src/sh4der_warp.c src/sh4der_trail.c src/sh4der_lens.c src/sh4der_focus.c src/sh4der_visor.c src/sh4der_torch.c src/sh4der_ca.c src/sh4der_shafts.c src/sh4der_spark.c src/sh4der_pl4nar.c src/sh4der_bolt.c src/sh4der_motes.c src/sh4der_pvrx.c
HDR      = include/sh4der_core.h include/sh4der_pvrx.h include/sh4der_blade.h include/sh4der_holo.h include/sh4der_bloom.h include/sh4der_look.h include/sh4der_taa.h include/sh4der_frame.h include/sh4der_glow.h include/sh4der_warp.h include/sh4der_trail.h include/sh4der_lens.h include/sh4der_focus.h include/sh4der_visor.h include/sh4der_torch.h include/sh4der_ao.h include/sh4der_ca.h include/sh4der_shafts.h include/sh4der_guard.h include/sh4der_uvwarp.h include/sh4der_sss.h include/sh4der_spark.h include/sh4der_pl4nar.h include/sh4der_pool.h include/sh4der_psm.h include/sh4der_psm_raster.h include/sh4der_psm_pvr.h include/sh4der_bolt.h include/sh4der_motes.h include/sh4der_deform.h include/sh4der_mist.h include/sh4der_rise.h include/sh4der_film.h include/sh4der_cloudvolume.h demo/matchlight/matchlight_scene.h

# kos-cc and kos-ar are the KallistiOS wrappers: they add its flags, its include paths and, when linking, its libraries.
# Not named KOS_CC / KOS_AR: the KallistiOS environment already sets those to the bare compiler and archiver.
DC_CC   ?= kos-cc
DC_AR   ?= kos-ar
# SH4ZAM (MIT, Falco Girgis; a git clone of its master, 0.10 or later, kept current with git pull) is not shipped: git ignores third_party/sh4zam, `make check-sh4zam` says how to fetch it.
# Override SH4ZAM to use another checkout.
SH4ZAM  ?= third_party/sh4zam
# SH4ZAM-DCE (header-only, on top of SH4ZAM) is included in third_party/sh4zam-dce; the ocean demo needs it. Override SH4ZAM_DCE to use another checkout.
SH4ZAM_DCE ?= third_party/sh4zam-dce
DCE_INC  = $(if $(SH4ZAM_DCE),-I$(SH4ZAM_DCE)/include)
# SH4DOW-DC ships in this repository: the soft shadows, and the column feather every effect here uses
SH4DOW  ?= sh4dow-dc
DC_FLAGS = -O3 -Wall -Wextra -Iinclude -I$(SH4DOW)/include -I$(SH4ZAM)/include -ffast-math -ffp-contract=fast -mfsrra -mfsca -fno-math-errno
TESTS    = core_test blade_test holo_test bloom_test look_test taa_test frame_test glow_test warp_test trail_test lens_test focus_test visor_test torch_test ao_test ca_test shafts_test guard_test pvrx_test uvwarp_test water_test ocean_scene_test sss_test spark_test pl4nar_test pool_test psm_test psm_raster_test psm_pvr_test psm_scene_test psm_mapsrc_test psm_town_test psm_fault_test bolt_test motes_test deform_test mist_test rise_test film_test cloudvolume_test matchlight_scene_test

.PHONY: test test-shz dc test-dc demo clean pl4nar-demo psm-demo check-sh4zam check-tuning test-demo

# Fails, with the git clone line, when SH4ZAM is missing or older than 0.10; also runs the script's own self-test.
check-sh4zam:
	$(PYTHON) tools/check_sh4zam.py --self-test
	$(PYTHON) tools/check_sh4zam.py $(SH4ZAM)

# Fails when a README tuning table row is not its header's SH4DER_* value, names nothing, or is not a number; a macro with no
# row fails too (--strict). Also runs the script's own self-test. Standard library python only.
check-tuning:
	$(PYTHON) tools/check_tuning.py --self-test
	$(PYTHON) tools/check_tuning.py --strict

# The street driving demo (demo/pl4nar): planar reflection, light pools and halos. One script from a clean checkout to an image:
# it fetches CC0 art, cuts it in Blender, bakes, compiles and runs mkdcdisc. PL4NAR_ARGS: e.g. --rtt --pin (see demo/pl4nar/README.md).
pl4nar-demo:
	test -n "$(SH4ZAM)" || { echo "set SH4ZAM to your SH4ZAM checkout"; exit 1; }
	$(PYTHON) demo/pl4nar/build.py --sh4zam $(SH4ZAM) $(PL4NAR_ARGS)

# The software perspective shadow map demo (demo/psm): host only. Fetches Spot (CC0) once, renders a uniform map and a PSM
# with the same map size to demo/psm/out, and joins them (needs python and Pillow for the sheet).
psm-demo: $(BUILD)/psm_demo
	$(PYTHON) demo/psm/tools/fetch.py
	mkdir -p demo/psm/out
	$(BUILD)/psm_demo $(PSM_ARGS)
	$(PYTHON) demo/psm/tools/sheet.py demo/psm/out 2

$(BUILD)/psm_demo: demo/psm/psm_demo.c include/sh4der_psm.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(WARN) -DSH4DER_CORE_NO_SH4ZAM -Iinclude -Ish4dow-dc/include $< src/sh4der_core.c -lm -o $@

test: check-tuning test-demo $(addprefix $(BUILD)/,$(TESTS)) $(BUILD)/blade
	$(BUILD)/core_test
	$(BUILD)/blade_test
	$(BUILD)/holo_test
	$(BUILD)/bloom_test
	$(BUILD)/look_test
	$(BUILD)/taa_test
	$(BUILD)/frame_test
	$(BUILD)/glow_test
	$(BUILD)/warp_test
	$(BUILD)/trail_test
	$(BUILD)/lens_test
	$(BUILD)/focus_test
	$(BUILD)/visor_test
	$(BUILD)/torch_test
	$(BUILD)/ao_test
	$(BUILD)/ca_test
	$(BUILD)/shafts_test
	$(BUILD)/guard_test
	$(BUILD)/pvrx_test
	$(BUILD)/water_test
	$(BUILD)/ocean_scene_test
	$(BUILD)/sss_test
	$(BUILD)/uvwarp_test
	$(BUILD)/spark_test
	$(BUILD)/pl4nar_test
	$(BUILD)/pool_test
	$(BUILD)/psm_test
	$(BUILD)/psm_raster_test
	$(BUILD)/psm_pvr_test
	$(BUILD)/psm_scene_test
	$(BUILD)/psm_mapsrc_test
	$(BUILD)/psm_town_test
	$(BUILD)/psm_fault_test
	$(BUILD)/bolt_test
	$(BUILD)/motes_test
	$(BUILD)/deform_test
	$(BUILD)/mist_test
	$(BUILD)/rise_test
	$(BUILD)/film_test
	$(BUILD)/cloudvolume_test
	$(BUILD)/matchlight_scene_test
	$(BUILD)/blade

$(BUILD)/%: $(SRC) $(HDR)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(WARN) -DSH4DER_CORE_NO_SH4ZAM -Iinclude -I$(SH4DOW)/include $(filter %.c,$^) -lm -o $@

$(BUILD)/core_test: test/core_test.c
$(BUILD)/blade_test: test/blade_test.c
$(BUILD)/holo_test: test/holo_test.c
$(BUILD)/bloom_test: test/bloom_test.c
$(BUILD)/look_test: test/look_test.c
$(BUILD)/taa_test: test/taa_test.c
$(BUILD)/frame_test: test/frame_test.c
$(BUILD)/glow_test: test/glow_test.c
$(BUILD)/warp_test: test/warp_test.c
$(BUILD)/trail_test: test/trail_test.c
$(BUILD)/lens_test: test/lens_test.c
$(BUILD)/focus_test: test/focus_test.c
$(BUILD)/visor_test: test/visor_test.c
$(BUILD)/torch_test: test/torch_test.c
$(BUILD)/ao_test: test/ao_test.c
$(BUILD)/ca_test: test/ca_test.c
$(BUILD)/shafts_test: test/shafts_test.c
$(BUILD)/guard_test: test/guard_test.c
$(BUILD)/pvrx_test: test/pvrx_test.c include/sh4der_pvrx.h
$(BUILD)/water_test: test/water_test.c include/sh4der_water.h include/sh4der_sss.h $(SH4DOW)/include/sh4dow_heightfield.h
$(BUILD)/sss_test: test/sss_test.c include/sh4der_sss.h
$(BUILD)/uvwarp_test: test/uvwarp_test.c
$(BUILD)/spark_test: test/spark_test.c
$(BUILD)/pl4nar_test: test/pl4nar_test.c include/sh4der_water.h include/sh4der_sss.h
$(BUILD)/pool_test: test/pool_test.c
$(BUILD)/psm_test: test/psm_test.c
$(BUILD)/psm_raster_test: test/psm_raster_test.c include/sh4der_psm_raster.h
$(BUILD)/psm_pvr_test: test/psm_pvr_test.c include/sh4der_psm_pvr.h include/sh4der_psm_raster.h
$(BUILD)/psm_scene_test: test/psm_scene_test.c demo/psm/psm_scene.h demo/psm/psm_town.h include/sh4der_psm_pvr.h include/sh4der_psm_raster.h include/sh4der_guard.h
$(BUILD)/psm_mapsrc_test: test/psm_mapsrc_test.c demo/psm/psm_mapsrc.h demo/psm/psm_scene.h demo/psm/psm_town.h include/sh4der_psm_pvr.h include/sh4der_psm_raster.h include/sh4der_guard.h
$(BUILD)/psm_fault_test: test/psm_fault_test.c demo/psm/psm_fault.h
# The town (demo/psm/psm_town.h) needs its mesh: demo/psm/tools/bake_town.py writes it into the build folder, from the
# Modular Village zip (CC0, demo/psm/fetch_table.json) when demo/psm/tools/fetch.py has fetched it, from stand-in boxes when
# not. The kit's props are cut by third_party/dc-forerunner/tools/lod/qem_decimate.py, imported from there.
# SQUARE=square: the walled square in its place (demo/psm/tools/bake_square.py, the same header; its pieces from the
# Fantasy Town Kit, CC0, fetch_table.json; stand-ins when the zip is missing), into $(BUILD)/psm_square. The default is
# the town. PSM_SQUARE_ZIP= points at a copy elsewhere. The square's build also starts psm_dc.c with the lazy shed plan
# and all three placement options (PSM_PLAN_LAZY 1, PSM_PLACE_DEFAULT 7: psm_dc.c); PSM_DC_EXTRA= adds more flags.
SQUARE       ?= town
PSM_DC_EXTRA ?=
PSM_SQUARE_ZIP ?= demo/psm/assets/_dl/kenney_fantasy-town-kit_2.0.zip
ifeq ($(SQUARE),square)
PSM_TOWN_ZIP  = $(PSM_SQUARE_ZIP)
PSM_TOWN_DIR  = $(BUILD)/psm_square
PSM_TOWN_BAKE = demo/psm/tools/bake_square.py
PSM_SQUARE_FLAGS = -DPSM_PLAN_LAZY=1 -DPSM_PLACE_DEFAULT=7u
else
PSM_TOWN_ZIP  = demo/psm/assets/_dl/modular_village_collection.zip
PSM_TOWN_DIR  = $(BUILD)/psm_town
PSM_TOWN_BAKE = demo/psm/tools/bake_town.py
endif
PSM_TOWN_H    = $(PSM_TOWN_DIR)/psm_town_mesh.h
# Its air (demo/psm/psm_sky.h): the sky band's texture, baked by demo/psm/tools/bake_sky.py from the Poly Haven sky (CC0,
# fetch_table.json) when it has been fetched, a gradient stand-in when not.
PSM_SKY_HDR   = demo/psm/assets/_dl/kloofendal_48d_partly_cloudy_puresky_2k.hdr
PSM_SKY_H     = $(BUILD)/psm_sky/psm_sky_tex.h
PSM_TOWN_DEPS = demo/psm/psm_scene.h demo/psm/psm_town.h demo/psm/psm_sky.h demo/psm/psm_send.h include/sh4der_psm_pvr.h include/sh4der_psm_raster.h include/sh4der_guard.h $(PSM_TOWN_H) $(PSM_SKY_H)
PSM_TOWN_INC  = -DPSM_HAVE_TOWN -I$(PSM_TOWN_DIR) -I$(BUILD)/psm_sky
$(PSM_TOWN_H): $(PSM_TOWN_BAKE) demo/psm/tools/bake_town.py $(wildcard $(PSM_TOWN_ZIP))
	@mkdir -p $(PSM_TOWN_DIR)
	$(PYTHON) -I $(PSM_TOWN_BAKE) $(if $(wildcard $(PSM_TOWN_ZIP)),$(PSM_TOWN_ZIP),-) $@ --json $(PSM_TOWN_DIR)/psm_town_mesh.json
$(PSM_SKY_H): demo/psm/tools/bake_sky.py $(wildcard $(PSM_SKY_HDR))
	@mkdir -p $(BUILD)/psm_sky
	$(PYTHON) -I demo/psm/tools/bake_sky.py $(if $(wildcard $(PSM_SKY_HDR)),$(PSM_SKY_HDR),-) $@ --json $(BUILD)/psm_sky/psm_sky_tex.json
$(BUILD)/psm_town_test: test/psm_town_test.c $(PSM_TOWN_DEPS) $(SRC) $(HDR)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(WARN) -DSH4DER_CORE_NO_SH4ZAM $(PSM_TOWN_INC) -Iinclude -I$(SH4DOW)/include $(filter %.c,$^) -lm -o $@

# The demos' own host tests (their headers and build tools, not the library's): `make test` runs them through test-demo.
# Plain C, and python's standard library only (no Pillow, no Blender, no KallistiOS). The two town tests use the town's
# mesh and sky as psm_town_test does (stand-ins when nothing was fetched). The hero street's replay needs its set, baked
# from the Space Kit zip (CC0, a download by hand, sha256 checked by demo/psm/hero/tools/bake_strip.py): with no zip at
# HERO_STRIP_ZIP it is not run and the last line says so. Not wired, it needs Pillow: demo/psm/tools/suite_skin.py.
DEMO_T          = $(BUILD)/demo_test
DEMO_CC         = $(CC) $(CFLAGS) $(WARN) -DSH4DER_CORE_NO_SH4ZAM -Iinclude -I$(SH4DOW)/include
DEMO_TESTS      = heat_test psm_ground_test suite_ui_test tile_cut_test psm_colour_test psm_plan_test
HERO_STRIP_ZIP ?= demo/pl4nar/assets/_dl/spacekit_2.0.zip
HERO_STRIP_TEST = $(if $(wildcard $(HERO_STRIP_ZIP)),$(DEMO_T)/hero_strip_test)
test-demo: $(addprefix $(DEMO_T)/,$(DEMO_TESTS)) $(HERO_STRIP_TEST)
	$(PYTHON) demo/pl4nar/tools/grit_gen.py --self-test
	$(PYTHON) demo/pl4nar/tools/check_coupe.py --self-test
	$(PYTHON) demo/psm/hero/tools/bake_strip.py --self-test
	$(DEMO_T)/heat_test
	$(DEMO_T)/psm_ground_test
	$(DEMO_T)/suite_ui_test
	$(DEMO_T)/tile_cut_test
	$(DEMO_T)/psm_colour_test
	$(DEMO_T)/psm_plan_test
	$(if $(HERO_STRIP_TEST),$(HERO_STRIP_TEST),@echo "hero_strip_test: NOT RUN, no $(HERO_STRIP_ZIP)")

$(DEMO_T)/heat_test: demo/pl4nar/host/heat_test.c demo/pl4nar/src/heat.h include/sh4der_core.h
$(DEMO_T)/psm_ground_test: demo/psm/test/psm_ground_test.c demo/psm/psm_ground.h
$(DEMO_T)/suite_ui_test: demo/psm/suite/test/suite_ui_test.c demo/psm/suite/suite_ui.h demo/psm/suite/suite_skin.h demo/psm/suite/suite_nav.h demo/psm/psm_credits.h
$(DEMO_T)/tile_cut_test: demo/psm/ao/test/tile_cut_test.c demo/psm/ao/tile_cut.h
$(DEMO_T)/heat_test $(DEMO_T)/psm_ground_test $(DEMO_T)/suite_ui_test $(DEMO_T)/tile_cut_test:
	@mkdir -p $(DEMO_T)
	$(DEMO_CC) $(filter %.c,$^) -lm -o $@
$(DEMO_T)/psm_colour_test: demo/psm/test/psm_colour_test.c
$(DEMO_T)/psm_plan_test: demo/psm/test/psm_plan_test.c
$(DEMO_T)/psm_colour_test $(DEMO_T)/psm_plan_test: src/sh4der_core.c $(PSM_TOWN_DEPS) include/sh4der_core.h
	@mkdir -p $(DEMO_T)
	$(DEMO_CC) $(PSM_TOWN_INC) $(filter %.c,$^) -lm -o $@
$(DEMO_T)/hero/hero_strip_data.h: demo/psm/hero/tools/bake_strip.py $(wildcard $(HERO_STRIP_ZIP))
	@mkdir -p $(DEMO_T)/hero
	$(PYTHON) -I demo/psm/hero/tools/bake_strip.py $(DEMO_T)/hero --zip $(HERO_STRIP_ZIP)
$(DEMO_T)/hero/hero_hens.h: demo/psm/hero/tools/hero_hens.py $(PSM_TOWN_H)
	@mkdir -p $(DEMO_T)/hero
	$(PYTHON) -I demo/psm/hero/tools/hero_hens.py $(PSM_TOWN_H) $(DEMO_T)/hero
$(DEMO_T)/hero_strip_test: demo/psm/hero/test/hero_strip_test.c demo/psm/hero/hero_strip.h demo/psm/hero/hero_view.h demo/psm/hero/hero_model.h $(DEMO_T)/hero/hero_strip_data.h $(DEMO_T)/hero/hero_hens.h
	@mkdir -p $(DEMO_T)
	$(DEMO_CC) -Idemo/psm/hero -I$(DEMO_T)/hero $(filter %.c,$^) -lm -o $@
$(BUILD)/bolt_test: test/bolt_test.c
$(BUILD)/motes_test: test/motes_test.c
$(BUILD)/deform_test: test/deform_test.c
$(BUILD)/mist_test: test/mist_test.c
$(BUILD)/rise_test: test/rise_test.c
$(BUILD)/film_test: test/film_test.c
$(BUILD)/cloudvolume_test: test/cloudvolume_test.c
$(BUILD)/matchlight_scene_test: test/matchlight_scene_test.c
$(BUILD)/blade: example/blade.c

test-shz: $(addprefix $(BUILD)/shz/,$(TESTS))
	$(BUILD)/shz/core_test
	$(BUILD)/shz/blade_test
	$(BUILD)/shz/holo_test
	$(BUILD)/shz/bloom_test
	$(BUILD)/shz/look_test
	$(BUILD)/shz/taa_test
	$(BUILD)/shz/frame_test
	$(BUILD)/shz/glow_test
	$(BUILD)/shz/warp_test
	$(BUILD)/shz/trail_test
	$(BUILD)/shz/lens_test
	$(BUILD)/shz/focus_test
	$(BUILD)/shz/visor_test
	$(BUILD)/shz/torch_test
	$(BUILD)/shz/ao_test
	$(BUILD)/shz/ca_test
	$(BUILD)/shz/shafts_test
	$(BUILD)/shz/guard_test
	$(BUILD)/shz/pvrx_test
	$(BUILD)/shz/water_test
	$(BUILD)/shz/ocean_scene_test
	$(BUILD)/shz/sss_test
	$(BUILD)/shz/uvwarp_test
	$(BUILD)/shz/spark_test
	$(BUILD)/shz/pl4nar_test
	$(BUILD)/shz/pool_test
	$(BUILD)/shz/psm_test
	$(BUILD)/shz/psm_raster_test
	$(BUILD)/shz/psm_pvr_test
	$(BUILD)/shz/psm_scene_test
	$(BUILD)/shz/psm_mapsrc_test
	$(BUILD)/shz/psm_town_test
	$(BUILD)/shz/psm_fault_test
	$(BUILD)/shz/bolt_test
	$(BUILD)/shz/motes_test
	$(BUILD)/shz/deform_test
	$(BUILD)/shz/mist_test
	$(BUILD)/shz/rise_test
	$(BUILD)/shz/film_test
	$(BUILD)/shz/cloudvolume_test
	$(BUILD)/shz/matchlight_scene_test

# The demo headers a SH4ZAM-path test includes (the pattern rule below knows only the library's): a change to one rebuilds it.
$(BUILD)/shz/psm_scene_test: demo/psm/psm_scene.h demo/psm/psm_town.h
$(BUILD)/shz/psm_mapsrc_test: demo/psm/psm_mapsrc.h demo/psm/psm_scene.h demo/psm/psm_town.h
$(BUILD)/shz/psm_fault_test: demo/psm/psm_fault.h
$(BUILD)/shz/%: test/%.c $(SRC) $(HDR)
	@$(PYTHON) tools/check_sh4zam.py $(SH4ZAM)
	@mkdir -p $(BUILD)/shz
	$(CC) $(CFLAGS) -std=gnu11 -Wall -Wextra -DSH4DER_TEST_TOL=0.0005f -Iinclude -I$(SH4DOW)/include -I$(SH4ZAM)/include $(DCE_INC) $(filter %.c,$^) $(SH4ZAM)/source/sw/shz_xmtrx_sw.c -lm -o $@

dc: $(BUILD)/libsh4der_core.a

$(BUILD)/libsh4der_core.a: $(SRC) $(HDR)
	@$(PYTHON) tools/check_sh4zam.py $(SH4ZAM)
	@mkdir -p $(BUILD)
	$(DC_CC) $(DC_FLAGS) -c src/sh4der_core.c -o $(BUILD)/sh4der_core.o
	$(DC_CC) $(DC_FLAGS) -c src/sh4der_blade.c -o $(BUILD)/sh4der_blade.o
	$(DC_CC) $(DC_FLAGS) -c src/sh4der_holo.c -o $(BUILD)/sh4der_holo.o
	$(DC_CC) $(DC_FLAGS) -c src/sh4der_bloom.c -o $(BUILD)/sh4der_bloom.o
	$(DC_CC) $(DC_FLAGS) -c src/sh4der_glow.c -o $(BUILD)/sh4der_glow.o
	$(DC_CC) $(DC_FLAGS) -c src/sh4der_frame.c -o $(BUILD)/sh4der_frame.o
	$(DC_CC) $(DC_FLAGS) -c src/sh4der_warp.c -o $(BUILD)/sh4der_warp.o
	$(DC_CC) $(DC_FLAGS) -c src/sh4der_trail.c -o $(BUILD)/sh4der_trail.o
	$(DC_CC) $(DC_FLAGS) -c src/sh4der_lens.c -o $(BUILD)/sh4der_lens.o
	$(DC_CC) $(DC_FLAGS) -c src/sh4der_focus.c -o $(BUILD)/sh4der_focus.o
	$(DC_CC) $(DC_FLAGS) -c src/sh4der_visor.c -o $(BUILD)/sh4der_visor.o
	$(DC_CC) $(DC_FLAGS) -c src/sh4der_torch.c -o $(BUILD)/sh4der_torch.o
	$(DC_CC) $(DC_FLAGS) -c src/sh4der_ca.c -o $(BUILD)/sh4der_ca.o
	$(DC_CC) $(DC_FLAGS) -c src/sh4der_shafts.c -o $(BUILD)/sh4der_shafts.o
	$(DC_CC) $(DC_FLAGS) -c src/sh4der_pl4nar.c -o $(BUILD)/sh4der_pl4nar.o
	$(DC_CC) $(DC_FLAGS) -c src/sh4der_spark.c -o $(BUILD)/sh4der_spark.o
	$(DC_CC) $(DC_FLAGS) -c src/sh4der_pvrx.c -o $(BUILD)/sh4der_pvrx.o
	$(DC_AR) rcs $@ $(BUILD)/sh4der_ca.o $(BUILD)/sh4der_shafts.o $(BUILD)/sh4der_frame.o $(BUILD)/sh4der_core.o $(BUILD)/sh4der_blade.o $(BUILD)/sh4der_holo.o $(BUILD)/sh4der_bloom.o $(BUILD)/sh4der_glow.o $(BUILD)/sh4der_warp.o $(BUILD)/sh4der_trail.o $(BUILD)/sh4der_lens.o $(BUILD)/sh4der_focus.o $(BUILD)/sh4der_visor.o $(BUILD)/sh4der_torch.o $(BUILD)/sh4der_spark.o $(BUILD)/sh4der_pl4nar.o $(BUILD)/sh4der_pvrx.o

# The hardware probe disc (demo/probe/): one PowerVR2 mode a screen (modifier volumes, two-volume polygons, fog table mode 2,
# the bump texture format, the colour clamp registers), each with a read-back verdict. For a real console. See demo/probe/README.md.
# PROBE_CDI names the image; an existing image is never replaced (a new build takes a new number).
.PHONY: probe
PROBE_CDI ?= $(BUILD)/probe/sh4der-probe-b3.cdi
probe: $(PROBE_CDI)
$(BUILD)/probe/sh4der-probe.elf: demo/probe/probe.c include/sh4der_guard.h include/sh4der_core.h
	@mkdir -p $(BUILD)/probe
	$(DC_CC) $(DC_FLAGS) -Werror demo/probe/probe.c -o $@
$(PROBE_CDI): $(BUILD)/probe/sh4der-probe.elf
	@test ! -e "$@" || { echo "refuse existing image: $@"; exit 1; }
	$(MKDCDISC) -e $< -o $@ -N -n SH4DER-PROBE

# The short film (demo/matchlight/): a 120 second snowy night street in twelve shots and 32 seconds of end credits, built
# from the snow depth grid, the fog bank, the rising lights, the spark pools and the motes. See demo/matchlight/README.md.
# Needs SH4ZAM=<checkout>, as every Dreamcast target does.
# MATCHLIGHT_CDI names the image; an existing image is never replaced (a new build takes a new number).
.PHONY: matchlight
MATCHLIGHT_CDI ?= $(BUILD)/matchlight/sh4der-matchlight.cdi
# The sound files (demo/matchlight/tools/audio.py writes them here); the image is made without sound when the folder is not there.
MATCHLIGHT_DISC ?= $(BUILD)/matchlight/disc
matchlight: $(MATCHLIGHT_CDI)
$(BUILD)/matchlight/sh4der-matchlight.elf: demo/matchlight/matchlight.c demo/matchlight/matchlight_scene.h src/sh4der_spark.c src/sh4der_motes.c src/sh4der_ca.c $(HDR)
	@test -n "$(SH4ZAM)" || { echo "set SH4ZAM to your SH4ZAM checkout"; exit 1; }
	@mkdir -p $(BUILD)/matchlight
	$(DC_CC) $(DC_FLAGS) -Werror demo/matchlight/matchlight.c src/sh4der_spark.c src/sh4der_motes.c src/sh4der_ca.c -o $@
$(MATCHLIGHT_CDI): $(BUILD)/matchlight/sh4der-matchlight.elf
	@test ! -e "$@" || { echo "refuse existing image: $@"; exit 1; }
	$(MKDCDISC) -e $< -o $@ -N -n MATCHLIGHT $(if $(wildcard $(MATCHLIGHT_DISC)),-D $(MATCHLIGHT_DISC))

# The demo disc (demo/): one program that shows the effects, as a .cdi image for a Dreamcast, a GDEMU or an emulator.
# PYTHON turns the models into a C header; MKDCDISC writes the image (https://gitlab.com/simulant/mkdcdisc).
# SH4ZAM_LIB is linked when the checkout has been built (build/libsh4zam.a); SH4ZAM is mostly inline.
PYTHON     ?= python3
MKDCDISC   ?= mkdcdisc
SH4ZAM_LIB ?= $(wildcard $(SH4ZAM)/build/libsh4zam.a)
DEMO_DIR    = $(BUILD)/demo
DEMO_MODELS = demo/models/dude.obj demo/models/wagon.obj demo/models/jet.obj demo/models/crate.obj demo/models/lamp.obj
DEMO_SH4DOW = $(SH4DOW)/src/sh4dow_dc.c $(SH4DOW)/src/sh4dow_foot.c

demo: $(DEMO_DIR)/sh4der-demo.cdi

$(DEMO_DIR)/models.h: $(DEMO_MODELS) demo/tools/obj2c.py
	@mkdir -p $(DEMO_DIR)
	$(PYTHON) demo/tools/obj2c.py $@ $(DEMO_MODELS)

$(DEMO_DIR)/sh4der-demo.elf: demo/main.c $(DEMO_DIR)/models.h $(BUILD)/libsh4der_core.a $(DEMO_SH4DOW)
	$(DC_CC) $(DC_FLAGS) -I$(DEMO_DIR) demo/main.c $(DEMO_SH4DOW) $(BUILD)/libsh4der_core.a $(SH4ZAM_LIB) -o $@

$(DEMO_DIR)/sh4der-demo.cdi: $(DEMO_DIR)/sh4der-demo.elf
	$(MKDCDISC) -e $< -o $@ -N -n SH4DER-PVR-DEMO

# The flag: `make dc DEMO=1` builds the demo disc after the library.
ifeq ($(DEMO),1)
dc: demo
endif

test-dc: $(addprefix $(BUILD)/,$(addsuffix .elf,$(TESTS)))

$(BUILD)/shz/psm_town_test: test/psm_town_test.c $(PSM_TOWN_DEPS) $(SRC) $(HDR)
	@$(PYTHON) tools/check_sh4zam.py $(SH4ZAM)
	@mkdir -p $(BUILD)/shz
	$(CC) $(CFLAGS) -std=gnu11 -Wall -Wextra -DSH4DER_TEST_TOL=0.0005f $(PSM_TOWN_INC) -Iinclude -I$(SH4DOW)/include -I$(SH4ZAM)/include $(DCE_INC) $(filter %.c,$^) $(SH4ZAM)/source/sw/shz_xmtrx_sw.c -lm -o $@
$(BUILD)/psm_town_test.elf: test/psm_town_test.c $(PSM_TOWN_DEPS) $(BUILD)/libsh4der_core.a
	$(DC_CC) $(DC_FLAGS) $(DCE_INC) -DSH4DER_TEST_TOL=0.0005f $(PSM_TOWN_INC) $< $(BUILD)/libsh4der_core.a -o $@
$(BUILD)/%.elf: test/%.c $(BUILD)/libsh4der_core.a
	$(DC_CC) $(DC_FLAGS) $(DCE_INC) -DSH4DER_TEST_TOL=0.0005f $< $(BUILD)/libsh4der_core.a -o $@

clean:
	rm -rf $(BUILD)

# Separate sunset scene; original effects gallery remains `make demo`.
.PHONY: ocean ocean-preview test-water
OCEAN_CDI ?= $(BUILD)/ocean/sh4der-ocean-b9.cdi
OCEAN_HEADERS = demo/ocean_scene.h include/sh4der_water.h include/sh4der_sss.h $(SH4DOW)/include/sh4dow_heightfield.h
ocean: $(OCEAN_CDI)
$(BUILD)/ocean/sh4der-ocean.elf: demo/ocean.c $(OCEAN_HEADERS) $(BUILD)/libsh4der_core.a
	@test -n "$(SH4ZAM_DCE)" || { echo "set SH4ZAM_DCE to your SH4ZAM-DCE checkout"; exit 1; }
	@mkdir -p $(BUILD)/ocean
	$(DC_CC) $(DC_FLAGS) $(DCE_INC) demo/ocean.c $(BUILD)/libsh4der_core.a $(SH4ZAM_LIB) -o $@
$(OCEAN_CDI): $(BUILD)/ocean/sh4der-ocean.elf
	@test ! -e "$@" || { echo "refuse existing image: $@"; exit 1; }
	$(MKDCDISC) -e $< -o $@ -N -n SH4DER-OCEAN
$(BUILD)/ocean-preview: demo/ocean.c $(OCEAN_HEADERS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 -Wall -Wextra -DSH4DER_CORE_NO_SH4ZAM -Iinclude -I$(SH4DOW)/include $< -lm -o $@
ocean-preview: $(BUILD)/ocean-preview
test-water: $(BUILD)/water_test
	$(BUILD)/water_test

$(BUILD)/shz/water_test $(BUILD)/water_test.elf $(BUILD)/shz/pl4nar_test $(BUILD)/pl4nar_test.elf: include/sh4der_water.h include/sh4der_sss.h $(SH4DOW)/include/sh4dow_heightfield.h

$(BUILD)/ocean_scene_test: test/ocean_scene_test.c $(OCEAN_HEADERS)
$(BUILD)/shz/ocean_scene_test $(BUILD)/ocean_scene_test.elf: $(OCEAN_HEADERS)

# Torch walk: a first-person walk over a dark floor with the torch, to see the normal map (highlight power 2: add passes of grey maps made from the normals, then the picture
# twice) lit by it (demo/torchwalk_ab.c, include/sh4der_bump.h). The R trigger fires.
# TORCHWALK_STAGE names a folder whose files go on the disc (the demo reads TWTEX.BIN from it if it is there); without it the demo makes its own textures.
.PHONY: torchwalk-ab
TORCHWALK_AB_CDI ?= $(BUILD)/torchwalk_ab/sh4der-torchwalk-ab.cdi
TORCHWALK_STAGE ?=
torchwalk-ab: $(TORCHWALK_AB_CDI)
$(BUILD)/torchwalk_ab/sh4der-torchwalk-ab.elf: demo/torchwalk_ab.c include/sh4der_bump.h $(BUILD)/libsh4der_core.a
	@mkdir -p $(BUILD)/torchwalk_ab
	$(DC_CC) $(DC_FLAGS) $(DCE_INC) demo/torchwalk_ab.c $(BUILD)/libsh4der_core.a $(SH4ZAM_LIB) -o $@
$(TORCHWALK_AB_CDI): $(BUILD)/torchwalk_ab/sh4der-torchwalk-ab.elf
	@test ! -e "$@" || { echo "refuse existing image: $@"; exit 1; }
	$(MKDCDISC) -e $< $(if $(TORCHWALK_STAGE),-D "$(TORCHWALK_STAGE)") -o $@ -N -n SH4DER-TORCHWALK-AB

# Perspective shadow maps on the Dreamcast, two routes side by side (demo/psm/psm_dc.c): route A, the PVR draws the map into
# a render target (needs a KallistiOS with pvr_scene_begin_rtt); route B, the SH-4 draws a depth map in RAM. The scene and the
# frame logic are demo/psm/psm_scene.h (host test: build/psm_scene_test, in make test). The cow is Spot (CC0) when
# demo/psm/tools/fetch.py has fetched it, cut to PSM_COW_TRIS triangles into the build folder; a stand-in of boxes when not.
# The image boots into the town square (demo/psm/psm_town.h, the SCENE row), whose mesh is the bake rule above.
# An existing image is never replaced: a new build takes a new PSM_DC_CDI name.
.PHONY: psm-dc
PSM_DC_CDI   ?= $(BUILD)/psm_dc/sh4der-psm-ab-b1.cdi
PSM_COW_TRIS ?= 400
PSM_COW_OBJ   = demo/psm/assets/_dl/spot.obj
PSM_COW_H     = $(BUILD)/psm_dc/psm_cow.h
PSM_COW_FLAGS = $(if $(wildcard $(PSM_COW_OBJ)),-DPSM_HAVE_COW -I$(BUILD)/psm_dc)
psm-dc: $(PSM_DC_CDI)
$(PSM_COW_H): $(PSM_COW_OBJ) demo/psm/tools/bake_cow.py
	@mkdir -p $(BUILD)/psm_dc
	$(PYTHON) -I demo/psm/tools/bake_cow.py $(PSM_COW_OBJ) $@ --tris $(PSM_COW_TRIS)
$(BUILD)/psm_dc/sh4der-psm-ab.elf: demo/psm/psm_dc.c demo/psm/psm_mapsrc.h demo/psm/psm_fault.h $(PSM_TOWN_DEPS) include/sh4der_psm.h $(BUILD)/libsh4der_core.a $(if $(wildcard $(PSM_COW_OBJ)),$(PSM_COW_H))
	@mkdir -p $(BUILD)/psm_dc
	$(DC_CC) $(DC_FLAGS) $(DCE_INC) -Idemo/psm $(PSM_COW_FLAGS) $(PSM_TOWN_INC) $(PSM_SQUARE_FLAGS) $(PSM_DC_EXTRA) demo/psm/psm_dc.c $(BUILD)/libsh4der_core.a $(SH4ZAM_LIB) -o $@
$(PSM_DC_CDI): $(BUILD)/psm_dc/sh4der-psm-ab.elf
	@test ! -e "$@" || { echo "refuse existing image: $@"; exit 1; }
	$(MKDCDISC) -e $< -o $@ -N -n SH4DER-PSM-AB

# Cloud fly-through (include/sh4der_cloudvolume.h): the host preview of demo/cloudvolume.c. It marches the clouds into the small
# buffer and does on the host what the PVR will do (palette, filtered quad, sky, sea), writing a 640 x 480 PPM:
#   build/cloudvolume-preview out.ppm t=12 res=128x96 steps=16 ...   (the keys are in the file's first comment)
.PHONY: cloudvolume-preview
$(BUILD)/cloudvolume-preview: demo/cloudvolume.c include/sh4der_cloudvolume.h include/sh4der_core.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=gnu11 -Wall -Wextra -DSH4DER_CORE_NO_SH4ZAM -Iinclude -I$(SH4DOW)/include $< -lm -o $@
cloudvolume-preview: $(BUILD)/cloudvolume-preview

# The Dreamcast program of the same file: the clouds, a PVR sky and the ocean demo's water under them, with rows of settings.
# CLOUDVOLUME_CDI names the image; an existing image is never replaced (a new build takes a new number).
.PHONY: cloudvolume
CLOUDVOLUME_CDI ?= $(BUILD)/cloudvolume/sh4der-cloudvolume-b1.cdi
cloudvolume: $(CLOUDVOLUME_CDI)
$(BUILD)/cloudvolume/sh4der-cloudvolume.elf: demo/cloudvolume.c include/sh4der_cloudvolume.h $(OCEAN_HEADERS) $(BUILD)/libsh4der_core.a
	@test -n "$(SH4ZAM_DCE)" || { echo "set SH4ZAM_DCE to your SH4ZAM-DCE checkout"; exit 1; }
	@mkdir -p $(BUILD)/cloudvolume
	$(DC_CC) $(DC_FLAGS) $(DCE_INC) demo/cloudvolume.c $(BUILD)/libsh4der_core.a $(SH4ZAM_LIB) -o $@
$(CLOUDVOLUME_CDI): $(BUILD)/cloudvolume/sh4der-cloudvolume.elf
	@test ! -e "$@" || { echo "refuse existing image: $@"; exit 1; }
	$(MKDCDISC) -e $< -o $@ -N -n SH4DER-CLOUDS
