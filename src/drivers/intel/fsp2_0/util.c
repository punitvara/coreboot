/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2015-2016 Intel Corp.
 * (Written by Alexandru Gagniuc <alexandrux.gagniuc@intel.com> for Intel Corp.)
 * (Written by Andrey Petrov <andrey.petrov@intel.com> for Intel Corp.)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <arch/io.h>
#include <cbfs.h>
#include <console/console.h>
#include <fsp/util.h>
#include <lib.h>
#include <reset.h>
#include <string.h>

static bool looks_like_fsp_header(const uint8_t *raw_hdr)
{
	if (memcmp(raw_hdr, FSP_HDR_SIGNATURE, 4)) {
		printk(BIOS_ALERT, "Did not find a valid FSP signature\n");
		return false;
	}

	if (read32(raw_hdr + 4) != FSP_HDR_LEN) {
		printk(BIOS_ALERT, "FSP header has invalid length\n");
		return false;
	}

	return true;
}

enum cb_err fsp_identify(struct fsp_header *hdr, const void *fsp_blob)
{
	const uint8_t *raw_hdr = fsp_blob;

	if (!looks_like_fsp_header(raw_hdr))
		return CB_ERR;

	hdr->spec_version = read8(raw_hdr + 10);
	hdr->revision = read8(raw_hdr + 11);
	hdr->fsp_revision = read32(raw_hdr + 12);
	memcpy(hdr->image_id, raw_hdr + 16, ARRAY_SIZE(hdr->image_id));
	hdr->image_id[ARRAY_SIZE(hdr->image_id) - 1] = '\0';
	hdr->image_size = read32(raw_hdr + 24);
	hdr->image_base = read32(raw_hdr + 28);
	hdr->image_attribute = read16(raw_hdr + 32);
	hdr->component_attribute = read16(raw_hdr + 34);
	hdr->cfg_region_offset = read32(raw_hdr + 36);
	hdr->cfg_region_size = read32(raw_hdr + 40);
	hdr->notify_phase_entry_offset = read32(raw_hdr + 56);
	hdr->memory_init_entry_offset = read32(raw_hdr + 60);
	hdr->silicon_init_entry_offset = read32(raw_hdr + 68);

	return CB_SUCCESS;
}

void fsp_print_header_info(const struct fsp_header *hdr)
{
	union {
		uint32_t val;
		struct {
			uint8_t bld_num;
			uint8_t revision;
			uint8_t minor;
			uint8_t major;
		} rev;
	} revision;

	revision.val = hdr->fsp_revision;

	printk(BIOS_DEBUG, "Spec version: v%u.%u\n", (hdr->spec_version >> 4 ),
							hdr->spec_version & 0xf);
	printk(BIOS_DEBUG, "Revision: %u.%u.%u, Build Number %u\n",
							revision.rev.major,
							revision.rev.minor,
							revision.rev.revision,
							revision.rev.bld_num);
	printk(BIOS_DEBUG, "Type: %s/%s\n",
			(hdr->component_attribute & 1 ) ? "release" : "debug",
			(hdr->component_attribute & 2 ) ? "test" : "official");
	printk(BIOS_DEBUG, "image ID: %s, base 0x%lx + 0x%zx\n",
		hdr->image_id, hdr->image_base, hdr->image_size);
	printk(BIOS_DEBUG, "\tConfig region        0x%zx + 0x%zx\n",
		hdr->cfg_region_offset, hdr->cfg_region_size);

	if ((hdr->component_attribute >> 12) == FSP_HDR_ATTRIB_FSPM) {
		printk(BIOS_DEBUG, "\tMemory init offset   0x%zx\n",
						hdr->memory_init_entry_offset);
	}

	if ((hdr->component_attribute >> 12) == FSP_HDR_ATTRIB_FSPS) {
		printk(BIOS_DEBUG, "\tSilicon init offset  0x%zx\n",
						hdr->silicon_init_entry_offset);
		printk(BIOS_DEBUG, "\tNotify phase offset  0x%zx\n",
						hdr->notify_phase_entry_offset);
	}

}

enum cb_err fsp_validate_component(struct fsp_header *hdr,
					const struct region_device *rdev)
{
	void *membase;

	/* Map just enough of the file to be able to parse the header. */
	membase = rdev_mmap(rdev, FSP_HDR_OFFSET, FSP_HDR_LEN);

	if (membase == NULL) {
		printk(BIOS_ERR, "Could not mmap() FSP header.\n");
		return CB_ERR;
	}

	if (fsp_identify(hdr, membase) != CB_SUCCESS) {
		rdev_munmap(rdev, membase);
		printk(BIOS_ERR, "No valid FSP header\n");
		return CB_ERR;
	}

	rdev_munmap(rdev, membase);

	fsp_print_header_info(hdr);

	/* Check if size specified in the header matches the cbfs file size */
	if (region_device_sz(rdev) < hdr->image_size) {
		printk(BIOS_ERR, "Component size bigger than cbfs file.\n");
		return CB_ERR;
	}

	return CB_SUCCESS;
}

static bool fsp_reset_requested(enum fsp_status status)
{
	return (status >= FSP_STATUS_RESET_REQUIRED_COLD &&
		status <= FSP_STATUS_RESET_REQUIRED_8);
}

void fsp_handle_reset(enum fsp_status status)
{
	if (!fsp_reset_requested(status))
		return;

	printk(BIOS_DEBUG, "FSP: handling reset type %x\n", status);

	switch(status) {
	case FSP_STATUS_RESET_REQUIRED_COLD:
		hard_reset();
		break;
	case FSP_STATUS_RESET_REQUIRED_WARM:
		soft_reset();
		break;
	case FSP_STATUS_RESET_REQUIRED_3:
	case FSP_STATUS_RESET_REQUIRED_4:
	case FSP_STATUS_RESET_REQUIRED_5:
	case FSP_STATUS_RESET_REQUIRED_6:
	case FSP_STATUS_RESET_REQUIRED_7:
	case FSP_STATUS_RESET_REQUIRED_8:
		chipset_handle_reset(status);
		break;
	default:
		break;
	}
}
