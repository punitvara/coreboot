/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2012 Google Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <drivers/intel/gma/i915_reg.h>
#include <drivers/intel/gma/drm_dp_helper.h>

/* things that are, strangely, not defined anywhere? */
#define PCH_PP_UNLOCK 0xabcd0000
#define WMx_LP_SR_EN (1<<31)

/* Google Link-specific defines */
/* how many 4096-byte pages do we need for the framebuffer?
 * There are 32 bits per pixel, or 4 bytes,
 * which means 1024 pixels per page.
 * HencetThere are 4250 GTTs on Link:
 * 2650 (X) * 1700 (Y) pixels / 1024 pixels per page.
 */
#define FRAME_BUFFER_PAGES ((2560*1700)/1024)
#define FRAME_BUFFER_BYTES (FRAME_BUFFER_PAGES*4096)

/* One-letter commands for code not meant to be ready for humans.
 * The code was generated by a set of programs/scripts.
 * M print out a kernel message
 * R read a register. We do these mainly to ensure that if hardware wanted
 * the register read, it was read; also, in debug, we can see what was expected
 * and what was found. This has proven *very* useful to get this debugged.
 * The udelay, if non-zero, will make sure there is a
 * udelay() call with the value.
 * The count is from the kernel and tells us how many times this read was done.
 * Also useful for debugging and the state
 * machine uses the info to drive a poll.
 * W Write a register
 * V set verbosity. It's a bit mask.
 *   0 -> nothing
 *   1 -> print kernel messages
 *   2 -> print IO ops
 *   4 -> print the number of times we spin on a register in a poll
 *   8 -> restore whatever the previous verbosity level was
 *   		(only one deep stack)
 *
 * Again, this is not really meant for human consumption. There is not a poll
 * operator as such because, sometimes, there is a read/write/read where the
 * second read is a poll, and this chipset is so touchy I'm reluctant to move
 * things around and/or delete too many reads.
 */
#define M 1
#define R 2
#define W 4
#define V 8
#define I 16
#define P 32

struct iodef {
	unsigned char op;
	unsigned int count;
	const char *msg;
	unsigned long addr;
	unsigned long data;
	unsigned long udelay;
};

/* i915.c */
unsigned long io_i915_READ32(unsigned long addr);
void io_i915_WRITE32(unsigned long val, unsigned long addr);

/* intel_dp.c */
u32 pack_aux(u32 *src, int src_bytes);
void unpack_aux(u32 src, u32 *dst, int dst_bytes);
int intel_dp_aux_ch(u32 ch_ctl, u32 ch_data, u32 *send, int send_bytes,
	u32 *recv, int recv_size);
