# SH4ZAM as a static library, from the checkout SH4ZAM names, built with the KallistiOS toolchain and the demo's maths flags.
# SH4ZAM is used as a library, never copied: Copyright (C) 2025, 2026 Falco Girgis (MIT). The source list is the checkout's.
SHZ_DIR := $(SH4ZAM)
SHZ_CSRC := $(SHZ_DIR)/source/sh4/shz_complex_sh4.c $(SHZ_DIR)/source/sh4/shz_matrix_sh4.c $(SHZ_DIR)/source/shz_version.c $(SHZ_DIR)/source/sw/shz_complex_sw.c $(SHZ_DIR)/source/sw/shz_matrix_sw.c $(SHZ_DIR)/source/sw/shz_xmtrx_sw.c
SHZ_SSRC := $(SHZ_DIR)/source/sh4/shz_mem_sh4.s $(SHZ_DIR)/source/sh4/shz_xmtrx_sh4.s
SHZ_OBJS := $(patsubst $(SHZ_DIR)/source/%.c,$(SHZ_DIR)/build/obj/%.o,$(SHZ_CSRC)) $(patsubst $(SHZ_DIR)/source/%.s,$(SHZ_DIR)/build/obj/%.s.o,$(SHZ_SSRC))
SHZ_CFLAGS := -O2 -Wall -I$(SHZ_DIR)/include -ffast-math -ffp-contract=fast -mfsrra -mfsca -fno-math-errno
$(SHZ_DIR)/build/libsh4zam.a: $(SHZ_OBJS)
	@mkdir -p $(dir $@)
	sh-elf-ar rcs $@ $(SHZ_OBJS)
$(SHZ_DIR)/build/obj/%.o: $(SHZ_DIR)/source/%.c
	@mkdir -p $(dir $@)
	kos-cc $(SHZ_CFLAGS) -c $< -o $@
$(SHZ_DIR)/build/obj/%.s.o: $(SHZ_DIR)/source/%.s
	@mkdir -p $(dir $@)
	kos-cc -c $< -o $@
