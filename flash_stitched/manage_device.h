/*
 * Copyright (C) 2010 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;

/* mfld-structures section 2.7.1 mfld-fas v0.8*/

struct OSII {			//os image identifier
	uint16 os_rev_minor;
	uint16 os_rev_major;
	uint32 logical_start_block;	//units defined by get_block_size() if
	//reading/writing to/from nand, units of
	//512 bytes if cracking a stitched image
	uint32 ddr_load_address;
	uint32 entery_point;
	uint32 size_of_os_image;	//units defined by get_page_size() if
	//reading/writing to/from nand, units of
	//512 bytes if cracking a stitched image
	uint8 attribute;
	uint8 reserved[3];
};

struct OSIP_header {		// os image profile
	uint32 sig;
	uint8 intel_reserved;	// was header_size;       // in bytes
	uint8 header_rev_minor;
	uint8 header_rev_major;
	uint8 header_checksum;
	uint8 num_pointers;
	uint8 num_images;
	uint16 header_size;	//was security_features;
	uint32 reserved[5];

	struct OSII desc[7];
};

int crack_stitched_image(void *data, struct OSII **rec, void **blob);	// for debug testing.
void dump_osip_header(struct OSIP_header *osip);
int write_OSIP(struct OSIP_header *osip);
