# SH4DER-PVR: host reference models of the pass recipes (demo/psm/RECIPES_NOTES.md R1, R3, R4, R5). Host only; not
# part of the library build. Run from this folder:
#
#   make test                        the plain C path
#   make test-shz SH4ZAM=<checkout>  the SH4ZAM path (SH4ZAM's software back end), default ../../../third_party/sh4zam
#   make clean

ROOT    = ../../..
CC     ?= cc
CFLAGS ?= -O2
BUILD   = $(ROOT)/build/recipes
SH4ZAM ?= $(ROOT)/third_party/sh4zam
INC     = -I. -I$(ROOT)/include
TESTS   = recipe_cel_test recipe_dither_test recipe_fogpulse_test recipe_softplane_test

.PHONY: test test-shz clean

test: $(addprefix $(BUILD)/,$(TESTS))
	@for t in $(TESTS); do $(BUILD)/$$t || exit 1; done

test-shz: $(addprefix $(BUILD)/shz/,$(TESTS))
	@for t in $(TESTS); do $(BUILD)/shz/$$t || exit 1; done

$(BUILD)/%: %.c recipes.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -std=c99 -Wall -Wextra -DSH4DER_CORE_NO_SH4ZAM $(INC) $< -lm -o $@

$(BUILD)/shz/%: %.c recipes.h
	@mkdir -p $(BUILD)/shz
	$(CC) $(CFLAGS) -std=gnu11 -Wall -Wextra -DSH4DER_TEST_TOL=0.0005f $(INC) -I$(SH4ZAM)/include $< $(SH4ZAM)/source/sw/shz_xmtrx_sw.c -lm -o $@

clean:
	rm -rf $(BUILD)
