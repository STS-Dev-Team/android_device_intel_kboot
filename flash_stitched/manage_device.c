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

/*
 * program for updating the OSIP and blasting blobs to the unprotected nand.
 *
 * this is needed for device management, and manufacturing (updates, and
 * preloading)
 *
 * this program is implemented based on the MoorestownFASv10_final.pdf and
 * looking at mrst-boot-util-0.2.
 *
 * With input from the MRST FW team.
 *
 * mark.gross@intel.com oct 25, 2010
 *
 * API's:
 * void invalidate_payload_osip_record(void);
 * void write_payload_os_image(void * data, size_t size);
 *
 * TBD: int stream_os_image_start(TBD);
 * TBD: int stream_os_image(TBD)
 * TBD: int stream_os_image_end(TBD);
 *
 * This code assumes that the packaging of the file to flash to the target was
 * created using fstk and is what is known as a "stitched" binary.
 *
 * For a stitched binary, the first 512 bytes is LBA0 extract the OSIP and OSII
 * data from it and write the rest of the file to the computed nand lba.
 *
 * TODO: PLEASE CHECK THIS!!!!! I'm worried that the OSIP may be in LBA1 not
 * LBA0 of the stitched image.
 *
 * We can only work with Stichted images that have only 1 OSII record and that
 * record is in desc[0] of the OSIP_header.
 *
 * The OSIP layout for manufactuing and update is implement such that there are
 * 2 os images.  The first OSII record is always the payload os, the second is
 * the Porvisioning OS (POS)
 *
 * However; Physically the POS is in the lowest LBA's, with the Payload OS
 * following after, and then the first partition where the rootFS goes.
 *
 * the OSII record for the payload OS is desc[0]
 * The OSII record for the provisioning OS is the desc[1]
 * there are no others supported.
 *
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include "manage_device.h"

#define PAYLOAD_OSII_REC 0
#define POS_OSII_REC 1

#define STITCHED_IMAGE_PAGE_SIZE 512
#define STITCHED_IMAGE_BLOCK_SIZE 512

#define MAX(x, y)       ((x) > (y) ? (x) : (y))
#define MIN(x, y)       ((x) < (y) ? (x) : (y))

/*
 * logical emmc layout is:
 * LBA0 OSIP_header Logical Block Adress zero LBA0
 * LBA1--LBAn OS images.  OSimages must start on LBA boundries.
 * cat /sys/devices/pci0000:00/0000:00:01.0/mmc_host/mmc0/mmc0:0001/erase_size
 * The size of logical block and size of page are both equal to erase_size
 * the program needs to read the data from sysfs.
 */

#define MMC_PAGES_PER_BLOCK 1
#define MMC_PAGE_SIZE "/sys/devices/pci0000:00/0000:00:01.0/mmc_host/mmc0/mmc0:0001/erase_size"
#define KBYTES 1024

#ifdef __ANDROID__
#define MMC_DEV_POS "/dev/block/mmcblk0"
#else
#define MMC_DEV_POS "/dev/mmcblk0"
#endif

static int get_page_size(void)
{
	int mmc_page_size;
	char buf[16];
	int fd;

	memset((void *)buf, 0, sizeof(buf));
	fd = open(MMC_PAGE_SIZE, O_RDONLY);
	if (fd < 0) {
		printf("open mmc page size failed\n");
		return -1;
	}
	if (read(fd, buf, 16) < 0) {
		printf("read of mmc page size failed\n");
		close(fd);
		return -1;
	}
	printf("page size %s\n", buf);
	if (sscanf(buf, "%d", &mmc_page_size) != 1) {
		printf("sscanf of mmc page size failed\n");
		close(fd);
		return -1;
	}
	close(fd);

	return mmc_page_size / KBYTES;
}

static int get_block_size(void)
{
	int mmc_page_size;

	mmc_page_size = get_page_size();

	return mmc_page_size * MMC_PAGES_PER_BLOCK;
}

void dump_osip_header(struct OSIP_header *osip)
{
	int i;

	printf
	    ("sig 0x%x, header_size 0x%hx, header_rev_minor 0x%hhx, header_rev_major 0x%hhx\n",
	     osip->sig, osip->header_size, osip->header_rev_minor,
	     osip->header_rev_major);
	printf
	    ("header_checksum 0x%hhx, num_pointers 0x%hhx, num_images 0x%hhx\n",
	     osip->header_checksum, osip->num_pointers, osip->num_images);

	for (i = 0; i < osip->num_pointers; i++) {
		printf
		    (" os_rev = 0x%0hx, os_rev = 0x%hx, logcial_start_block = 0x%x\n",
		     osip->desc[i].os_rev_minor, osip->desc[i].os_rev_major,
		     osip->desc[i].logical_start_block);
		printf
		    (" ddr_load_address = 0x%0x, entry_point = 0x%0x, size_of_os_image= 0x%x,attribute= 0x%x\n",
		     osip->desc[i].ddr_load_address, osip->desc[i].entery_point,
		     osip->desc[i].size_of_os_image,osip->desc[i].attribute);
	}
}

void dump_OS_page(struct OSIP_header *osip, int os_index, int numpages)
{
	int i, j, fd;
	int pagesize = get_page_size();
	int blocksize = get_block_size();
	static uint8 buffer[8192 + 1024];
	short *temp;

	i = os_index;
	printf
	    (" os_rev = 0x%0hx, os_rev = 0x%0hx, logcial_start_block = 0x%0x\n",
	     osip->desc[i].os_rev_minor, osip->desc[i].os_rev_major,
	     osip->desc[i].logical_start_block);
	printf
	    (" ddr_load_address = 0x%0x, entry_point = 0x%0x, size_of_os_image= 0x%x\n",
	     osip->desc[i].ddr_load_address, osip->desc[i].entery_point,
	     osip->desc[i].size_of_os_image);

	for (i = 0; i < numpages; i++) {
		fd = open(MMC_DEV_POS, O_RDONLY);
		if (fd < 0)
			return;
		lseek(fd,
		      osip->desc[os_index].logical_start_block
		      * blocksize, SEEK_SET);
		//lseek(fd, lba_size, SEEK_SET);
		memset((void *)buffer, 0, sizeof(buffer));
		if (read(fd, (void *)buffer, sizeof(buffer)) < sizeof(buffer)) {
			printf("read failed\n");
			close(fd);
			return;
		}
		close(fd);
		for (j = 0; j < pagesize / 0x10; j++) {
			temp = (uint16 *) (buffer + j * 0x10);
			printf("%x %hx %hx %hx %hx %hx %hx %hx %hx\n",
			       osip->desc[os_index].logical_start_block *
			       blocksize + i * pagesize + j * 0x10, temp[0],
			       temp[1], temp[2], temp[3], temp[4], temp[5],
			       temp[6], temp[7]);
		}
	}

}

int read_OSIP(struct OSIP_header *osip)
{
	int lba_size;
	size_t address;
	int fd;

	printf("into read_OSIP\n");
	memset((void *)osip, 0, sizeof(*osip));
	lba_size = get_block_size();
	fd = open(MMC_DEV_POS, O_RDONLY);
	if (fd < 0)
		return -1;
	lseek(fd, 0, SEEK_SET);
	//lseek(fd, lba_size, SEEK_SET);
	if (read(fd, (void *)osip, sizeof(*osip)) < 0) {
		printf("read of osip failed\n");
		close(fd);
		return -1;
	}
	close(fd);
	printf("read of osip works\n");
	dump_osip_header(osip);
	//dump_OS_page(osip,0,1);
	//dump_OS_page(osip,1,1);

	return 1;
}

int write_OSIP(struct OSIP_header *osip)
{
	int lba_size;
	size_t address;
	int i, fd;
	uint32 temp;
	uint8 checksum = 0;
	uint8 *buf = (uint8 *) osip;

	//compute checksum
	osip->header_checksum = 0;
	for (i = 0; i < osip->header_size; i++) {
		checksum = checksum ^ (buf[i]);
	}
	osip->header_checksum = checksum;

	lba_size = get_block_size();
	fd = open(MMC_DEV_POS, O_RDWR);
	if (fd < 0) {
		printf("fail to open %s\n", MMC_DEV_POS);
		return -1;
	}
	lseek(fd, 0, SEEK_SET);
	if (write(fd, (void *)osip, sizeof(*osip)) < 0) {
		close(fd);
		printf("fail writing osip\n");
		return -1;
	}
	fsync(fd);
	close(fd);

	dump_osip_header(osip);
	//dump_OS_page(osip,0,1);
	//dump_OS_page(osip,1,1);
	//TODO: look for a way to flush nand
	printf("write of osip worked\n");
	return 1;
}

int crack_stitched_image(void *data, struct OSII **rec, void **blob)
{
	struct OSIP_header *osip = (struct OSIP_header *)data;
	if (!data)
		return -1;
	if (((struct OSIP_header *)data)->num_images != 1) {
		printf("too many osii records in stiched data\n");
		return -1;
	}			// we only know how to deal with trivial OS packages.

	*rec = &osip->desc[0];
	*blob = (uint8 *) data + STITCHED_IMAGE_BLOCK_SIZE;

//	dump_OS_page(osip, 0, 0);
	return 1;
}

static int invalidate_osip_record(int i)
{
	struct OSIP_header osip;
	int lsb, size_of_os_image;
	int attribute;

	if (read_OSIP(&osip) < 0) {
		printf("fail reading OSIP\n");
		return -1;	//FAIL
	}

	lsb = osip.desc[i].logical_start_block;
	size_of_os_image = osip.desc[i].size_of_os_image;
	attribute  = osip.desc[i].attribute;
	memset((void *)&osip.desc[i], 0, sizeof(struct OSII));
	osip.desc[i].logical_start_block = lsb;
	osip.desc[i].size_of_os_image = size_of_os_image;
	osip.desc[i].attribute = attribute;

	if (write_OSIP(&osip) < 0) {
		printf("fail write OSIP\n");
		return -1;	//FAIL
	}

	return 0;
}

int invalidate_payload_osip_record(void)
{
	return invalidate_osip_record(PAYLOAD_OSII_REC);
}

int restore_payload_osip_record(void)
{
	struct OSIP_header osip;
	struct OSII *osii;

	if (read_OSIP(&osip) < 0) {
		printf("read_OSIP fails\n");
		return -1;	/*FAIL */
	}

	osip.desc[PAYLOAD_OSII_REC].ddr_load_address =
	    osip.desc[POS_OSII_REC].ddr_load_address;
	osip.desc[PAYLOAD_OSII_REC].entery_point =
	    osip.desc[POS_OSII_REC].entery_point;

	if (write_OSIP(&osip) < 0) {
		printf("fail write OSIP\n");
		return -1;	//FAIL
	}

	return 0;
}

int write_payload_os_image(void *data, size_t size)
{
	struct OSIP_header osip;
	struct OSII *osii;
	void *blob;
	uint32 lba, temp_size_bytes;
	int block_size = get_block_size();
	int page_size = get_page_size();
	int carry, fd, num_pages, temp_offset;

	if (block_size < 0) {
		printf("block size wrong\n");
		return -1;	//FAIL
	}
	if (crack_stitched_image(data, &osii, &blob) < 0) {
		printf("crack_stitched_image fails\n");
		return -1;	//fail
	}
	if ((osii->size_of_os_image * STITCHED_IMAGE_PAGE_SIZE) !=
	    size - STITCHED_IMAGE_BLOCK_SIZE) {
		printf("data format is not correct! %X != %X \n",
		       osii->size_of_os_image * STITCHED_IMAGE_PAGE_SIZE,
		       size - STITCHED_IMAGE_BLOCK_SIZE
			);
		return -1;	//fail
	}
	if (read_OSIP(&osip) < 0) {
		printf("read_OSIP fails\n");
		return -1;	//FAIL
	}

	osip.num_images = 1;
	osii->logical_start_block =
	    MAX(osip.desc[PAYLOAD_OSII_REC].logical_start_block,
		osip.desc[POS_OSII_REC].logical_start_block);
	osip.desc[POS_OSII_REC].logical_start_block =
	    MIN(osip.desc[PAYLOAD_OSII_REC].logical_start_block,
		osip.desc[POS_OSII_REC].logical_start_block);

	osii->size_of_os_image =
	    (osii->size_of_os_image * STITCHED_IMAGE_PAGE_SIZE) / page_size;

	memcpy(&(osip.desc[PAYLOAD_OSII_REC]), osii, sizeof(struct OSII));
	write_OSIP(&osip);

	fd = open(MMC_DEV_POS, O_RDWR);
	if (fd < 0) {
		printf("fail open %s\n", MMC_DEV_POS);
		return -1;	//FAIL
	}
	lseek(fd, osii->logical_start_block * block_size, SEEK_SET);
	if (write(fd, blob, size - STITCHED_IMAGE_BLOCK_SIZE) <
	    size - STITCHED_IMAGE_BLOCK_SIZE) {
		printf("fail write of blob\n");
		close(fd);
		return -1;	//fail
	}
	fsync(fd);
	close(fd);

	return 0;
}
int write_payload_os_image_file(char *fwBinFile)
{
        char *fwFileData = NULL;
        char *tempPtr;
        FILE *fwp = NULL;
        struct stat sb;
        fprintf(stderr, "fw file is %s\n", fwBinFile);

        if (!(fwp = fopen(fwBinFile, "rb"))) {
                perror("fopen error:Unable to open file\n");
		return -1;
        }

        if (fstat(fileno(fwp), &sb) == -1) {
                perror("fstat error\n");
                fclose(fwp);
		return -1;
        }

        if ((fwFileData = calloc(sb.st_size, 1)) == NULL) {
                fclose(fwp);
		return -1;
        }

        if (fread(fwFileData, 1, sb.st_size, fwp) < sb.st_size) {
                perror("unable to fread fw bin file into buffer\n");
                free(fwFileData);
                fclose(fwp);
		return -1;
        }

        return write_payload_os_image(fwFileData, sb.st_size);

}
#if 0

hack code that isnt ready to even compile..static int stream_fd;
static int stream_remaining;
int stream_os_image_start(size_t os_image_size, struct OSII *rec, int index);
{
	struct OSIP_header osip;
	uint32 lba, temp_size;
	int block_size = get_block_size();

	stream_remaining = os_image_size;

	return 0;
}

int stream_os_image(void *partial_buffer, int buf_size)
{
	if (stream_remaining < buf_size) {
		close(stream_fd);
		return -1;
	}
	if (write(stream_fd, partial_buffer, buf_size) < 0) {
		close(stream_fd);
		return -1;
	}
	stream_remaining -= buf_size;

	if (stream_remaining == 0)
		close(stream_sp);

	return stream_remaining;
}

#endif
