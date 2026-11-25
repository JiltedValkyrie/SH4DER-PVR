# demo/psm: an ARM compiler for the AICA's ARM7DI

Research only. Nothing was installed, built or run. Read 2026-11-25.

**Marks.**

- **FACT**: read in a primary document, or in source code that we have here.
- **REPORTED**: a third party says it (a homebrew project, an emulator, a package page). Flycast is the emulator authors' model, never evidence about the hardware.
- **UNKNOWN**: not found.

**Paths.**

- `KOS` = `D:\Workspace-Claude\dc-homebrew\dc-halo\third_party\KallistiOS`, at commit 568c3404 (2026-10-11).
- `SysArc` = Sega, *Dreamcast/Dev.Box System Architecture*, last update 99/09/03 (https://segaretro.org/images/7/78/DreamcastDevBoxSystemArchitecture.pdf). Page numbers below are the document's printed ones.

## The one thing to know first

The core is an **ARM7DI**, which is **ARMv3**, not ARMv4T. Lead's brief says ARMv4T; the sources below disagree.

| # | Claim | Mark | Source |
|---|---|---|---|
| 1 | "Dedicated sound processor (ARM7DI) on chip; provides seven interrupts with priority levels" | FACT | SysArc §4.2.2, p. 227 |
| 2 | binutils' assembler maps the name `arm7di` to the ARMv3 architecture. | FACT | `KOS/utils/kos-chain/binutils-2.45.1/gas/config/tc-arm.c:30567`, `ARM_CPU_OPT ("arm7di", NULL, ARM_ARCH_V3, …)` |
| 3 | "GCC dropping support for the AICA's ARM7DI core after GCC 8.x" | FACT (KOS changelog, 2020-04-03) | `KOS/utils/kos-chain/doc/CHANGELOG.md:111` |
| 4 | GCC 9 removed Armv2 and Armv3 and every `-mcpu` built on them. | FACT | GCC 9 changes, https://gcc.gnu.org/gcc-9/changes.html ; patch https://gcc.gnu.org/legacy-ml/gcc-patches/2018-06/msg00468.html |
| 5 | GCC 15.2 has no `arm7di` entry; its oldest Arm-mode targets are `armv4` and `armv4t`. | FACT | `KOS/utils/kos-chain/gcc-15.2.0/gcc/config/arm/arm-cpus.in:354-366` |
| 6 | On the console there is no LDRH, STRH or LDRSB, no UMULL, SMULL, UMLAL or SMLAL, no Thumb and no BX. Its tests that ran there still use MUL, MLA, SWP, LDRB, LDM, STM, MRS and MSR. | REPORTED (a test that ran on hardware, by its own author) | dc-arm7wrestler: README and `arm7/source/armwrestler-ds.s` comments, for example "UMULL and SMULL were both removed because they don't exist on Dreamcast's ARM", "LDRH testcases were removed because we don't have that on Dreamcast's ARM", "apparently we don't have the bx instruction". https://github.com/snickerbockers/dc-arm7wrestler |

**What this means for a modern GCC.** This is inference, not something we tested. GCC 9 to 16 can be told `-march=armv4 -marm`. ARMv4 includes halfword loads, and GCC assumes it includes long multiply. So it will freely emit:

- `ldrh` / `strh` for every `uint16_t`;
- `ldrsb` for every `int8_t`;
- `umull` to divide by a constant (for example `x / 10`) and for 64-bit multiplies.

On this core those are undefined instructions. The prebuilt `libgcc` and `newlib` in those packages are built for armv4t or later.

**A cheap guard.** This is inference, not tested: assemble the compiler's `-S` output with `as -mcpu=arm7di`, which accepts ARMv3 only (row 2). Any forbidden instruction then fails at build time. Or disassemble the final binary and search it for `ldrh|strh|ldrs|mull|mlal|bx`.

## (1) Getting a compiler on Windows 11

| Option | Version | Licence | Size | Hash source | Fits ARM7DI? |
|---|---|---|---|---|---|
| **A. KOS kos-chain, `aica` profile** (built from source in MSYS2, like our `sh-elf`) | GCC **8.5.0**, binutils 2.47, newlib 4.6.0.20260123; target `arm-eabi`; configure `--with-arch=armv4 --with-mode=arm`; target libraries built with `-mcpu=arm7di`. FACT: `KOS/utils/kos-chain/profiles/aica/stable.mk` | GPL-3.0 (GCC with its runtime library exception), newlib mixed (UNKNOWN in detail) | UNKNOWN: build time and disk not measured | **None.** `scripts/download.mk` has no sha, md5 or signature check (FACT, grep). The sources come from GNU mirrors. | **Yes.** This is what KOS uses. A MinGW host patch exists: `patches/hosts/x86_64-w64-mingw32/gcc-8.5.0.diff` (FACT). Install path `/opt/toolchains/dc/arm-eabi` (FACT, `Makefile.aica.cfg`). |
| **B. MSYS2 pacman** `mingw-w64-ucrt-x86_64-arm-none-eabi-gcc` | 16.2.0-1; newlib package 4.6.0.20260123-1 | GPL-3.0-or-later; newlib: GPL/LGPL/BSD mix | 58.63 MB download, 743 MB installed (+ newlib 18.55 MB / 281 MB) | pacman signatures; page sha256 `e9fcbc34…f34464` | **No ARMv3.** Usable only with the guard above. REPORTED: https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-arm-none-eabi-gcc |
| **C. Arm GNU Toolchain** 15.3.rel2 zip, `arm-gnu-toolchain-15.3.rel2-mingw-w64-x86_64-arm-none-eabi.zip` | GCC 15.3, binutils 2.45, newlib trunk | GPL v3 / LGPL (EULA.txt); "community supported … no warranty" | 289,240,072 bytes | a `.zip.sha256asc` file per archive; registry sha256 `0daff414…db8cb9e` | No ARMv3. REPORTED: https://gitlab.arm.com/tooling/gnu-toolchains-for-arm (developer.arm.com now redirects here). The release notes name an `.msi` that the 15.3.rel2 registry does not list. |
| **D. xPack** v15.2.1-1.1, `xpack-arm-none-eabi-gcc-15.2.1-1.1-win32-x64.zip` | GCC 15.2.1, binutils 2.45, newlib 4.5.0 (from Arm 15.2.Rel1) | no statement on the page | UNKNOWN (asset list did not load) | blog sha256 `bae6a3d1…fd93ea` | No ARMv3. Its multilibs start at `arm/v5te`. REPORTED: https://xpack-dev-tools.github.io/arm-none-eabi-gcc-xpack/blog/2026/03/03/arm-none-eabi-gcc-v15-2-1-1-1-released/ |

The hashes above are shortened. Copy the full values from the pages before checking a download.

**Pick.**

- **A** gives C that runs on the core: GCC 8.5 is the last GCC that knows `-mcpu=arm7di`.
- **B** is the fastest install, but its output must pass the guard.
- **Trap with A.** Once `DC_ARM_CC` points at a real file, the KOS kernel build compiles `stream.drv` from source instead of copying `stream.drv.prebuilt` (FACT, `KOS/kernel/arch/dreamcast/sound/arm/Makefile`). That changes our KOS build output. The prebuilt driver is 3,360 bytes, sha256 `d9402138324f32553ba103ed84a84dbc89b40301ed140468ff2624c36fb9beab`; the current `stream.drv` is identical. To keep the prebuilt driver, leave `DC_ARM_CC` unset in the shell that builds KOS.

## (2) How KOS loads and starts an ARM program

- **Flags** (FACT, `KOS/environ_dreamcast.sh:48-59`):
  - CFLAGS `-mcpu=arm7di -Wall -O2 -fno-strict-aliasing -Wl,--fix-v4bx -Wa,--fix-v4bx`
  - LDFLAGS `-Wl,-Ttext=0x00000000,-N -nostartfiles -nostdlib -e reset`
  - libraries `-lgcc` only; start file `sound/arm/crt0.s`
- **Image** (FACT, `sound/arm/Makefile`): `prog.elf` is linked at 0, `objcopy -O binary` makes `stream.drv`, and `bin2c` embeds it in the SH-4 program.
- **crt0** (FACT, `sound/arm/crt0.s`):
  - the vector table starts at 0;
  - the stack is `mov sp,#0xb000`;
  - `bl arm_main`;
  - the FIQ handler reads `0x00802d00` (the interrupt number L), handles timer A (2) and bus request (5), and clears through `0x00802d04` (M);
  - the timer ticks `AICA_MEM_CLOCK` at `0x21000`.
- **Load** (FACT, `sound/snd_iface.c:38-64`): `spu_disable()`, `spu_memset_sq(0, 0, AICA_RAM_START)`, `spu_memload_sq(0, drv, size rounded up to 4)`, `spu_enable()`, `thd_sleep(10)`.
- **Reset bit** (FACT, `hardware/spu.c:290-304`): `spu_enable` clears bit 0 of `SNDREGADDR(0x2c00)` and `spu_disable` sets it. Sega: "ARMRST … 0: Reset clear, 1: Reset … can only be controlled by the system (SH4)" (FACT, SysArc §8.4.5, p. 385). `spu_init` writes an idle loop `0xeafffff8` at word 0 (`spu.c:371`).
- **Memory map** (FACT, SysArc Table 4-8, p. 228):
  - the ARM sees sound RAM at `0x00000000`, the channel registers at `0x00800000`, the common registers at `0x00802800` and the DSP at `0x00803000`;
  - the SH-4 sees them at `0x00800000` and `0x00700000`;
  - the SH-4 may access the AICA area in **4 bytes only**;
  - after 8 writes it must wait for the G2 FIFO (SysArc §4.2.2.3, p. 230).
- **KOS command layout** (FACT, `sound/arm/aica_cmd_iface.h`):
  - command queue at `0x10000`;
  - response queue at `0x18000`;
  - channels at `0x20000`;
  - clock at `0x21000`;
  - free RAM from `0x30000` to `0x200000`.
- **Interrupt back to the SH-4.** All FACT, SysArc §8.4.5, pp. 383-385:
  - MCIPD bit 5: "This interrupt request to the system (SH4) is written by the CPU; only a '1' can be written … can be set by the system (SH4) or by the ARM."
  - MCIEB enables the interrupt and MCIRE clears it.
  - "Interrupt levels cannot be specified for interrupts to the system (SH4)."
  - On the SH-4 side it arrives as SB_ISTEXT `0x005F6904` bit 1, "AICA interrupt" (SysArc p. 294).

  The register offsets are MCIEB `0x28B4`, MCIPD `0x28B8`, MCIRE `0x28BC`, SCIEB `0x289C`, SCIPD `0x28A0` and SCIRE `0x28A4`. These are REPORTED from KOS `sound/arm/aica.c:20-25`. SysArc's register table does not survive text extraction (p. 382), so the offsets have not been checked against the page.

  NetBSD's driver writes `0x28b4 = 0x0020` "INT Enable to SH4" (REPORTED: https://ftp.kaist.ac.kr/NetBSD/NetBSD-release-11/src/sys/arch/dreamcast/dev/microcode/aica_arm.c). KOS defines `ASIC_EVT_SPU_IRQ 0x0101` (`include/dc/asic.h:110`) but nothing in KOS hooks it: KOS **polls** its response queue (`snd_poll_resp`). FACT, grep.
- **Examples with source:**
  - KOS `sound/arm/` (crt0.s, main.c, aica.c): BSD-style KOS licence; attribution required (`KOS/README.md:55`).
  - NetBSD `sys/arch/dreamcast/dev/microcode/aica_arm.c`: avoids `udivsi3`.
  - Linux `sound/sh/aica.c`: loads `aica_firmware.bin` at offset 0, writes `0xea000002` as an idle stub, and uses ARM_RESET_REGISTER `0xA0702C00` bit 0. https://codebrowser.dev/linux/linux/sound/sh/aica.c.html
  - dc-arm7wrestler: assembled with `arm-linux-gnueabi-as -EL -mcpu=arm7di`, linked at 0.

## (3) Limits

- **Clock.** The audio block runs at 22.5792 MHz, from the GD-ROM's 33.8688 MHz through a PLL; the host interface runs at 25 MHz (FACT, SysArc p. 227). That the ARM core runs at exactly 22.5792 MHz is REPORTED (Flycast `arm7.h`) and is not stated by SysArc. 22,579,200 / 44,100 = 512 ARM clocks a sample.
- **Sound RAM.** 2 MB of SDRAM on a 16-bit bus at 67.7376 MHz, CAS latency 2, burst read and single write. It is shared by the ARM, the sound source, the DSP and the SH-4 (FACT, SysArc §4.2.2.4 Table 4-9, p. 231). The ARM's wait states per access are **UNKNOWN**: no source read gives a number. Flycast charges 1 tick an access (emulator model).
- **ISA.**
  - ARM mode only. There is no Thumb (rows 1, 2 and 6).
  - Byte and word loads only. Read a 16-bit sample as two `ldrb`, or as a word and a shift.
  - No 64-bit multiply.
  - Return with `mov pc, lr`; `--fix-v4bx` rewrites `bx` (FACT: KOS flags).
- **Unaligned word load.** On ARMv3/v4 an unaligned LDR rotates the word. That this happens on the AICA is REPORTED: arm7wrestler has "non word-aligned load" test cases, and Flycast rotates. A store's behaviour is UNKNOWN.
- **No FPU.** Floats are soft-float `libgcc` calls. This is REPORTED/INF: no source read says it outright, KOS links `-lgcc` only, and the ARM7 family has no FPU.
- **Integer multiply cost.** UNKNOWN for this core. The ARM7 family's MUL takes "1S + m×I", with m = 1 to 4 by the bits of the multiplier. That comes from ARM's ARM7TDMI manual, which was **not read**, so treat it as REPORTED from memory and check it on the console. There is no divide instruction: `/` and `%` call `libgcc` routines.

## (4) The AICA datasheet with wait states

- **What Junior named: UNKNOWN.** Junior's message is not in this session.
- **The primary doc we have.** It is Sega's SysArc, §4.2.2 and §8.4.5 (the AICA register descriptions, pp. 373-385). It has the clocks, the memory type and the interrupt bits, but **no ARM wait-state figure**.
- **No Yamaha AICA datasheet was found or read.** If Junior has a title or link, it should be checked against SysArc §4.2.2.4.

## Console tests, if one is built

1. Run the ARM in a loop of N `ldr` from sound RAM. The ARM timer gives clocks per load; also run it with the SH-4 hammering G2.
2. Write MCIPD bit 5 from the ARM with MCIEB bit 5 set, and see SB_ISTEXT bit 1 on the SH-4.
3. Execute one `ldrh` and one `umull` behind an undefined-instruction vector that logs. This confirms row 6 on our own console.
