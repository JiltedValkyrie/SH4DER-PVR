# <project> Dreamcast port. Needs the KOS environment (tools/build.ps1 sets it up from config/config.local). No literal paths here.
# Dependencies: python tools/fresh_deps.py --ensure clones SH4ZAM, SH4DER-PVR, SH4DOW-DC at their pins into third_party/ (git-ignored).
# PROJECT names the ELF (build/$(PROJECT).elf). build.ps1 passes PORT_NAME from config/config.local.
ifndef KOS_BASE
$(error KOS_BASE not set. Use tools/build.ps1)
endif
# ABI guard: environ.sh silently falls back to -m4-single-only when sh-elf-gcc cannot run at source time (MinGW DLLs not on PATH). That build is slower and hid a compiler crash in dc-halo. Refuse it.
ifneq ($(findstring -m4-single-only,$(KOS_CFLAGS)),)
$(error KOS_CFLAGS has -m4-single-only, not the image ABI -m4-single: put MinGW bin and KOS_CC_BASE/bin on PATH before sourcing environ.sh)
endif
# Release KOS: environ.sh should add -DNDEBUG -DDBGLOG_DISABLED (KOS asserts and debug log off) and rebuild KOS once with them (docs/PERFORMANCE.md).
ifeq ($(findstring -DNDEBUG,$(KOS_CFLAGS)),)
$(warning KOS_CFLAGS lacks -DNDEBUG -DDBGLOG_DISABLED: KOS asserts and debug log stay on, slower image. See docs/PERFORMANCE.md)
endif
PROJECT ?= port
SH4ZAM    ?= third_party/sh4zam
SH4DER    ?= third_party/sh4der-pvr
SH4DOW    ?= third_party/sh4dow-dc
SH4ZAM_LIB = $(SH4ZAM)/build/libsh4zam.a
MATHFLAGS = -I$(SH4ZAM)/include -ffast-math -ffp-contract=fast -mfsrra -mfsca -fno-math-errno
# USE_SH4DOW=1 / USE_SH4DER=1: build and link the effect libraries (static archives, only the used parts are pulled in). Off by default: the title screen uses neither.
USE_SH4DOW ?= 0
USE_SH4DER ?= 0
LIBS = $(SH4ZAM_LIB)
# kos-cc already adds KOS_CFLAGS: -O2 -fomit-frame-pointer -m4-single -ml -mfsrra -mfsca -ffunction-sections -fdata-sections (link --gc-sections).
# OPTFLAGS is the game default; per-file EXTRA_CFLAGS come after it, so they win.
OPTFLAGS ?= -O2
CFLAGS_ALL = $(OPTFLAGS) -Wall -Wextra $(MATHFLAGS) $(EXTRA_CFLAGS)
# HOT_O3: hot per-frame files (collision, skinning, mesh draw, particles) at -O3, after an A/B on hardware (dc-halo: collision -O3 +18 KB text, real ms win).
# COLD_OS: cold files (save, settings, menu state, loaders) at -Os to pay for the -O3 text. Add files here, never change OPTFLAGS for everything.
HOT_O3  ?=
COLD_OS ?=
ifneq ($(strip $(HOT_O3)),)
$(HOT_O3): EXTRA_CFLAGS += -O3
endif
ifneq ($(strip $(COLD_OS)),)
$(COLD_OS): EXTRA_CFLAGS += -Os
endif
ifeq ($(USE_SH4DOW),1)
CFLAGS_ALL += -I$(SH4DOW)/include
SH4DOW_SRCS = $(wildcard $(SH4DOW)/src/*.c)
LIBS += build/libsh4dow_dc.a
endif
ifeq ($(USE_SH4DER),1)
CFLAGS_ALL += -I$(SH4DER)/include -I$(SH4DOW)/include   # SH4DER-PVR builds on SH4DOW-DC
SH4DER_SRCS = $(wildcard $(SH4DER)/src/*.c)
LIBS += build/libsh4der_pvr.a
endif
TARGET = build/$(PROJECT).elf
OBJS = build/main.o
all: $(TARGET)
build/%.o: src/%.c
	@mkdir -p build
	kos-cc $(CFLAGS_ALL) -c $< -o $@
build/lib/sh4dow/%.o: $(SH4DOW)/src/%.c
	@mkdir -p $(dir $@)
	kos-cc $(CFLAGS_ALL) -c $< -o $@
build/lib/sh4der/%.o: $(SH4DER)/src/%.c
	@mkdir -p $(dir $@)
	kos-cc $(CFLAGS_ALL) -c $< -o $@
build/libsh4dow_dc.a: $(patsubst $(SH4DOW)/src/%.c,build/lib/sh4dow/%.o,$(SH4DOW_SRCS))
	rm -f $@ && sh-elf-ar rcs $@ $^
build/libsh4der_pvr.a: $(patsubst $(SH4DER)/src/%.c,build/lib/sh4der/%.o,$(SH4DER_SRCS))
	rm -f $@ && sh-elf-ar rcs $@ $^
$(TARGET): $(OBJS) $(LIBS)
	kos-cc -o $@ $(OBJS) $(LIBS)
clean:
	rm -rf build/*.o build/lib build/*.a $(TARGET)
-include tools/sh4zam_lib.mk
