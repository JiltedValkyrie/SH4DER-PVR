# SH4DER-PVR (SH4DOW-DC is included, in the folder sh4dow-dc; SH4DOW= names another checkout)
#
#   make test                       host check of the plain C path: test/core_test, test/blade_test, test/holo_test, test/bloom_test, test/look_test, test/taa_test, test/frame_test, test/glow_test, test/warp_test, test/trail_test, test/lens_test, test/focus_test, test/visor_test, test/torch_test, test/ao_test, example/blade
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
SRC      = src/sh4der_core.c src/sh4der_blade.c src/sh4der_holo.c src/sh4der_bloom.c src/sh4der_glow.c src/sh4der_frame.c src/sh4der_warp.c src/sh4der_trail.c src/sh4der_lens.c src/sh4der_focus.c src/sh4der_visor.c src/sh4der_torch.c src/sh4der_ca.c src/sh4der_shafts.c
HDR      = include/sh4der_core.h include/sh4der_blade.h include/sh4der_holo.h include/sh4der_bloom.h include/sh4der_look.h include/sh4der_taa.h include/sh4der_frame.h include/sh4der_glow.h include/sh4der_warp.h include/sh4der_trail.h include/sh4der_lens.h include/sh4der_focus.h include/sh4der_visor.h include/sh4der_torch.h include/sh4der_ao.h include/sh4der_ca.h include/sh4der_shafts.h

# kos-cc and kos-ar are the KallistiOS wrappers: they add its flags, its include paths and, when linking, its libraries.
# Not named KOS_CC / KOS_AR: the KallistiOS environment already sets those to the bare compiler and archiver.
DC_CC   ?= kos-cc
DC_AR   ?= kos-ar
SH4ZAM  ?=
# SH4DOW-DC ships in this repository: the soft shadows, and the column feather every effect here uses
SH4DOW  ?= sh4dow-dc
DC_FLAGS = -O3 -Wall -Wextra -Iinclude -I$(SH4DOW)/include -I$(SH4ZAM)/include -ffast-math -ffp-contract=fast -mfsrra -mfsca -fno-math-errno
TESTS    = core_test blade_test holo_test bloom_test look_test taa_test frame_test glow_test warp_test trail_test lens_test focus_test visor_test torch_test ao_test ca_test shafts_test

.PHONY: test test-shz dc test-dc demo clean

test: $(addprefix $(BUILD)/,$(TESTS)) $(BUILD)/blade
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

$(BUILD)/shz/%: test/%.c $(SRC) $(HDR)
	@test -n "$(SH4ZAM)" || { echo "set SH4ZAM to your SH4ZAM checkout"; exit 1; }
	@mkdir -p $(BUILD)/shz
	$(CC) $(CFLAGS) -std=gnu11 -Wall -Wextra -DSH4DER_TEST_TOL=0.0005f -Iinclude -I$(SH4DOW)/include -I$(SH4ZAM)/include $(filter %.c,$^) $(SH4ZAM)/source/sw/shz_xmtrx_sw.c -lm -o $@

dc: $(BUILD)/libsh4der_core.a

$(BUILD)/libsh4der_core.a: $(SRC) $(HDR)
	@test -n "$(SH4ZAM)" || { echo "set SH4ZAM to your SH4ZAM checkout"; exit 1; }
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
	$(DC_AR) rcs $@ $(BUILD)/sh4der_ca.o $(BUILD)/sh4der_shafts.o $(BUILD)/sh4der_frame.o $(BUILD)/sh4der_core.o $(BUILD)/sh4der_blade.o $(BUILD)/sh4der_holo.o $(BUILD)/sh4der_bloom.o $(BUILD)/sh4der_glow.o $(BUILD)/sh4der_warp.o $(BUILD)/sh4der_trail.o $(BUILD)/sh4der_lens.o $(BUILD)/sh4der_focus.o $(BUILD)/sh4der_visor.o $(BUILD)/sh4der_torch.o

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

$(BUILD)/%.elf: test/%.c $(BUILD)/libsh4der_core.a
	$(DC_CC) $(DC_FLAGS) -DSH4DER_TEST_TOL=0.0005f $< $(BUILD)/libsh4der_core.a -o $@

clean:
	rm -rf $(BUILD)
