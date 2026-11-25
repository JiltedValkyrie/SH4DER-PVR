# SH4DER-PVR: the street driving demo (planar reflection, light pools, halos). Run through build.py, or by hand:
#
#   make SH4ZAM=<checkout> [RTT=1] [DEMO_BUILD=<n>] [MAP=1 GEN=<folder>]   build/pl4nar-demo.elf   (KallistiOS environment sourced)
#
# MAP=1 builds the new map (src/map.c, PL4NAR_MAP) in place of the street: GEN is the folder that holds assets_gen.c
# and tools/map.py's map_gen.c (default $(BUILD)/gen).
# RTT=1 compiles ../../src/sh4der_pl4nar.c with SH4DER_PL4NAR_HAVE_RTT, for a KallistiOS that has pvr_scene_begin_rtt
# (a 256 x 128 target costs 64 KB). Without it the target is 1024 pixels wide (256 KB). The baked assets
# (build/gen/assets_gen.c, assets_gen.h) come from tools/bake.py: build.py runs it first.
ifndef KOS_BASE
$(error KOS_BASE not set: source the KallistiOS environment first)
endif
ifeq ($(SH4ZAM),)
$(error set SH4ZAM to your SH4ZAM checkout)
endif
# ABI guard: environ.sh silently falls back to -m4-single-only when sh-elf-gcc cannot run at source time. Refuse it.
ifneq ($(findstring -m4-single-only,$(KOS_CFLAGS)),)
$(error KOS_CFLAGS has -m4-single-only, not -m4-single: put the toolchain on PATH before sourcing environ.sh)
endif
LIB        ?= ../..
SH4DOW     ?= $(LIB)/sh4dow-dc
SH4ZAM_LIB  = $(SH4ZAM)/build/libsh4zam.a
DEMO_BUILD ?= 0
RTT        ?= 0
BUILD      ?= build
MAP        ?= 0
GEN        ?= $(BUILD)/gen
MATHFLAGS   = -isystem $(SH4ZAM)/include -ffast-math -ffp-contract=fast -mfsrra -mfsca -fno-math-errno
CFLAGS_ALL  = -O2 -std=gnu11 -Wall -Wextra -Wcast-align=strict $(MATHFLAGS) -Isrc -I$(GEN) -I$(LIB)/include -I$(SH4DOW)/include -DDEMO_BUILD=$(DEMO_BUILD) -MD -MP
ifeq ($(RTT),1)
CFLAGS_ALL += -DSH4DER_PL4NAR_HAVE_RTT
endif
ifeq ($(MAP),1)
CFLAGS_ALL += -DPL4NAR_MAP
endif
# AUTORUN=1 TAG=<name>: the measuring autorun (main.c PL4NAR_AUTORUN): stages, then result pages (../psm/psm_page.h),
# back to the suite menu (../psm/psm_suite.h) when started from one.
ifeq ($(AUTORUN),1)
CFLAGS_ALL += -DPL4NAR_AUTORUN -DPL4M_TAG=$(TAG) -I$(LIB)/demo/psm
endif
# EXTRA: more flags, e.g. EXTRA="-DDRAW_BACK=160.0f -DDRAW_FWD=224.0f"
CFLAGS_ALL += $(EXTRA)
# SHZ_LIB=0: link no libsh4zam.a (every SH4ZAM call the demo makes is an inline header function): nothing is built in
# the SH4ZAM tree, which stays read-only.
SHZ_LIB ?= 1
ifeq ($(SHZ_LIB),0)
SH4ZAM_LIB =
endif
TARGET = $(BUILD)/pl4nar-demo.elf
OBJS   = $(BUILD)/main.o $(BUILD)/demo.o $(BUILD)/gfx.o $(BUILD)/gfx_pvr.o $(BUILD)/hud.o $(BUILD)/sh4der_pl4nar.o $(BUILD)/assets_gen.o
ifeq ($(MAP),1)
OBJS  += $(BUILD)/map.o $(BUILD)/map_gen.o
endif
all: $(TARGET)
$(BUILD)/%.o: src/%.c
	@mkdir -p $(BUILD)
	kos-cc $(CFLAGS_ALL) -c $< -o $@
$(BUILD)/sh4der_pl4nar.o: $(LIB)/src/sh4der_pl4nar.c
	@mkdir -p $(BUILD)
	kos-cc $(CFLAGS_ALL) -c $< -o $@
$(BUILD)/assets_gen.o: $(GEN)/assets_gen.c
	@mkdir -p $(BUILD)
	kos-cc $(CFLAGS_ALL) -c $< -o $@
$(BUILD)/map_gen.o: $(GEN)/map_gen.c
	@mkdir -p $(BUILD)
	kos-cc $(CFLAGS_ALL) -c $< -o $@
# main.o carries the build number: it is rebuilt when the number changes
$(BUILD)/main.o: $(BUILD)/build_number.txt
$(BUILD)/build_number.txt: FORCE
	@mkdir -p $(BUILD)
	@echo "$(DEMO_BUILD)" | cmp -s - $@ || echo "$(DEMO_BUILD)" > $@
FORCE:
$(TARGET): $(OBJS) $(SH4ZAM_LIB)
	kos-cc -o $@ $(OBJS) $(SH4ZAM_LIB) -lm
ifneq ($(SHZ_LIB),0)
-include tools/sh4zam_lib.mk
endif
-include $(OBJS:.o=.d)
