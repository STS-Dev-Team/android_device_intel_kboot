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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <getopt.h>
#include <unistd.h>
#include "osip.h"

/* Unfied interface to get page size
 * NAND: Need driver to provide the size
 * eMMC: PAGE size = SECTOR size
 */

static int get_page_size(void)
{
	return MMC_PAGE_SIZE;
}

/* Unfied interface to get block size
 * NAND: Need driver to provide the size
 * eMMC: BLOCK size = PAGE size = SECTOR size
 */

static int get_block_size(void)
{
	int mmc_page_size;

	mmc_page_size = get_page_size();

	return mmc_page_size * MMC_PAGES_PER_BLOCK;
}

int check_image_valid(size_t size, void *blob, int logical_start_block)
{
	int fd, i;
	char buff[STITCHED_IMAGE_BLOCK_SIZE];
	int block_num = (size / STITCHED_IMAGE_BLOCK_SIZE) - 1;
	int residue_bytes = (size % STITCHED_IMAGE_BLOCK_SIZE);
	int j;

	printf("Image validity checking starting->\n");
	fd = open(MMC_DEV_POS, O_RDWR);
	if (fd < 0) {
		printf("fail open %s\n", MMC_DEV_POS);
		return -1;
	}
	lseek(fd, logical_start_block * STITCHED_IMAGE_PAGE_SIZE, SEEK_SET);
	for (i = 1; i <= block_num; ++i) {
		if (read(fd, buff, STITCHED_IMAGE_BLOCK_SIZE) <
		    STITCHED_IMAGE_BLOCK_SIZE) {
			printf("fail read of buffer\n");
			close(fd);
			return -1;
		}

		if (memcmp
		    ((void *)blob, (void *)buff, STITCHED_IMAGE_BLOCK_SIZE)) {

			printf
			    ("Image disrupted!! Please re-burn your image file!\n");
			return -1;
		}
		blob += STITCHED_IMAGE_BLOCK_SIZE;
	}
	if (residue_bytes != 0) {
		if (read(fd, buff, residue_bytes) < residue_bytes) {
			printf("fail read of blob\n");
			close(fd);
			return -1;
		}
		if (memcmp((void *)blob, (void *)buff, residue_bytes))
			return -1;
	}
	close(fd);
	printf("Image validity check passed!\n");
	return 0;
}

int write_stitch_image(void *data, size_t size, int update_number)
{
	struct OSIP_header osip;
	struct OSIP_header bck_osip;
	struct OSII *osii;
	void *blob;
	uint32 lba, temp_size_bytes;
	int block_size = get_block_size();
	int page_size = get_page_size();
	int carry, fd, num_pages, temp_offset;

	printf("now into write_stitch_image\n");
	if (block_size < 0) {
		printf("block size wrong\n");
		return -1;
	}
	if (crack_stitched_image(data, &osii, &blob) < 0) {
		printf("crack_stitched_image fails\n");
		return -1;
	}
	if ((osii->size_of_os_image * STITCHED_IMAGE_PAGE_SIZE) !=
	    size - STITCHED_IMAGE_BLOCK_SIZE) {
		printf("data format is not correct! %x != %x \n",
			osii->size_of_os_image * STITCHED_IMAGE_PAGE_SIZE,
			size - STITCHED_IMAGE_BLOCK_SIZE);
		return -1;
	}
	if (read_OSIP_loc(&osip, R_START, NOT_DUMP) < 0) {
		printf("read_OSIP fails\n");
		return -1;
	}

	osip.num_images = 1;
	osii->logical_start_block =
	    osip.desc[update_number].logical_start_block;

	osii->size_of_os_image =
	    (osii->size_of_os_image * STITCHED_IMAGE_PAGE_SIZE) / page_size;

	memcpy(&(osip.desc[update_number]), osii, sizeof(struct OSII));
	if (update_number == POS)
		write_OSIP(&osip);

	read_OSIP_loc(&bck_osip, R_BCK, NOT_DUMP);
	if (bck_osip.sig != OSIP_SIG) {
		printf
		    ("No backup OSIP when flash image. Start flash new image...\n");
		write_OSIP(&osip);
	} else
		write_OSII_entry(osii, update_number, R_BCK);

	fd = open(MMC_DEV_POS, O_RDWR);
	if (fd < 0) {
		printf("fail open %s\n", MMC_DEV_POS);
		return -1;
	}
	lseek(fd, osii->logical_start_block * block_size, SEEK_SET);
	if (write(fd, blob, size - STITCHED_IMAGE_BLOCK_SIZE) <
	    size - STITCHED_IMAGE_BLOCK_SIZE) {
		printf("fail write of blob\n");
		close(fd);
		return -1;
	}
	fsync(fd);
	close(fd);

	return check_image_valid(size, blob, osii->logical_start_block);	/*check image written into EMMC is valid */
}

int flash_stitch_image(char *argv, int update_number)
{
	char *fwBinFile = NULL;
	char *fwFileData = NULL;
	char *tempPtr;
	FILE *fwp = NULL;
	struct stat sb;

	printf("run into flash_stitch_image\n");
	/*Checks to see if the file is terminated by *.bin */
	if ((tempPtr = strrchr(argv, '.')) == NULL) {
		fprintf(stderr,
			"Invalid inputs, correct usage is --image FW.bin\n");
		exit(1);
	}

	if (strncmp(tempPtr, FILE_EXT, strlen(FILE_EXT))) {
		fprintf(stderr,
			"File doesnt have *.bin extn,correct usage is --image FW.bin\n");
		exit(1);
	}

	fwBinFile = argv;

	fprintf(stderr, "fw file is %s\n", fwBinFile);

	if (!(fwp = fopen(fwBinFile, "rb"))) {
		perror("fopen error:Unable to open file\n");
		exit(1);
	}

	if (fstat(fileno(fwp), &sb) == -1) {
		perror("fstat error\n");
		fclose(fwp);
		exit(1);
	}

	if ((fwFileData = calloc(sb.st_size, 1)) == NULL) {
		fclose(fwp);
		exit(1);
	}

	if (fread(fwFileData, 1, sb.st_size, fwp) < sb.st_size) {
		perror("unable to fread fw bin file into buffer\n");
		free(fwFileData);
		fclose(fwp);
		exit(1);
	}

	return	write_stitch_image(fwFileData, sb.st_size, update_number);

}

void display_usage(void)
{
	printf("Update_osip Tool USAGE:\n");
	printf("--check     	| Print current OSIP header\n");
	printf("--backup    	| Backup all valid OSII in current OSIP\n");
	printf
	    ("--invalidate <attribute>   | Invalidate specified OSII with <attribute> ,used with --backup!\n");
	printf
	    ("--restore   	| Restore all valid OSII in backup region to current OSIP\n");
	printf
	    ("--update <OSII_Number> --image <xxx.bin>  | Update the specified OSII entry and flash xxx.bin\n");
	printf
	    ("--update <OSII_Number> -m xx -n xx -l xx -a xx -s xx -e xx | Update specified OSII with parameters following\n");
	exit(EXIT_FAILURE);
}

int read_OSIP_loc(struct OSIP_header *osip, int location, int dump)
{
	int fd;

	if (dump) {
		if (!location)
			printf("OSIP header:\n");
		else
			printf("Backup OSIP header:\n");
	}
	memset((void *)osip, 0, sizeof(*osip));
	fd = open(MMC_DEV_POS, O_RDONLY);
	if (fd < 0)
		return -1;

	if (location)
		lseek(fd, BACKUP_LOC, SEEK_SET);
	else
		lseek(fd, 0, SEEK_SET);

	if (read(fd, (void *)osip, sizeof(*osip)) < 0) {
		printf("read of osip failed\n");
		close(fd);
		return -1;
	}
	close(fd);

	if (osip->sig != OSIP_SIG) {
		printf
		    ("No backup OSIP header.\n");
	}

	if ((dump) && (osip->sig == OSIP_SIG)) {
		dump_osip_header(osip);
		if (location)
			printf("read of osip from BACKUP_LOC works\n");
		else
			printf("read of osip works\n");
	}

	return 0;
}

int backup_handle(struct OSIP_header *osip)
{
	int fd;
	struct OSIP_header bck_osip;

	fd = open(MMC_DEV_POS, O_RDWR);
	if (fd < 0) {
		printf("fail to open %s\n", MMC_DEV_POS);
		return -1;
	}
	lseek(fd, BACKUP_LOC, SEEK_SET);
	if (write(fd, (void *)osip, sizeof(*osip)) < 0) {
		close(fd);
		printf("fail writing osip\n");
		return -1;
	}
	fsync(fd);
	close(fd);

	read_OSIP_loc(&bck_osip, R_BCK, DUMP_OSIP);
	printf("write of osip to BACKUP_LOC addr worked\n");
	return 0;
}

int restore_handle(void)
{				/*don't restore all OSII,check entry if valid */
	struct OSIP_header bck_osip;
	int i, fd, devfd;
	unsigned char rbt_reason;

	printf("run into restore_handle\n");

	if (read_OSIP_loc(&bck_osip, R_BCK, DUMP_OSIP) < 0) {
		printf("read_backup_OSIP fails\n");
		return -1;	/*FAIL */
	}
	if (bck_osip.sig != OSIP_SIG)
		return -1;

	if (write_OSIP(&bck_osip) < 0) {
		printf("fail write OSIP when restore OSIP\n");
		return -1;
	}

	fd = open(MMC_DEV_POS, O_RDWR);
	if (fd < 0) {
		printf("fail to open %s\n", MMC_DEV_POS);
		return -1;
	}
	memset((void *)&bck_osip, 0, sizeof(bck_osip));	/*remove all backup entries */
	lseek(fd, BACKUP_LOC, SEEK_SET);
	if (write(fd, (void *)&bck_osip, sizeof(bck_osip)) < 0) {
		close(fd);
		printf("fail when deleting all backup entrys of OSII\n");
		return -1;
	}
	fsync(fd);
	close(fd);

	rbt_reason = RR_SIGNED_MOS;
	if ((devfd = open(IPC_DEVICE_NAME, O_RDWR)) < 0) {
		printf("unable to open the DEVICE %s\n", IPC_DEVICE_NAME);
	} else {
		ioctl(devfd, IPC_WRITE_RR_TO_OSNIB, &rbt_reason);
		close(devfd);
	}

	return 0;
}

int write_OSII_entry(struct OSII *upd_osii, int update_number, int location)
{
	int fd;

	fd = open(MMC_DEV_POS, O_RDWR);
	if (fd < 0) {
		printf("fail to open %s\n", MMC_DEV_POS);
		return -1;
	}
	if (location == R_START)
		lseek(fd, OSIP_PREAMBLE + sizeof(struct OSII) * update_number,
		      SEEK_SET);
	else
		lseek(fd,
		      BACKUP_LOC + OSIP_PREAMBLE +
		      sizeof(struct OSII) * update_number, SEEK_SET);

	if (write(fd, (void *)upd_osii, sizeof(*upd_osii)) < 0) {
		close(fd);
		printf("fail when write OSII entry\n");
		return -1;
	}
	fsync(fd);
	close(fd);
	return 0;
}

int remove_backup_OSII(int update_number)
{
	int fd, ret;
	struct OSII osii;

	memset((void *)&osii, 0xDD, sizeof(struct OSII));	/*removed pattern 0xDD */
	fd = open(MMC_DEV_POS, O_RDWR);
	if (fd < 0) {
		printf("fail to open %s\n", MMC_DEV_POS);
		return -1;
	}
	lseek(fd,
	      BACKUP_LOC + OSIP_PREAMBLE + sizeof(struct OSII) * update_number,
	      SEEK_SET);
	if (write(fd, (void *)&osii, sizeof(osii)) < 0) {
		close(fd);
		printf("fail when write OSII entry\n");
		return -1;
	}
	fsync(fd);
	close(fd);
	printf("remove_OSII_entry worked!\n");
	return 0;
}

int update_handle(struct OSII *osii, int update_number)
{
	struct OSII upd_osii;
	struct OSIP_header *osip;

	osip = (struct OSIP_header *)malloc(sizeof(struct OSIP_header));

	read_OSIP_loc(osip, R_START, NOT_DUMP);
	printf("run into update handle\n");

	upd_osii = osip->desc[update_number];
	if (osii->os_rev_major != 0xffff)
		upd_osii.os_rev_major = osii->os_rev_major;
	if (osii->os_rev_minor != 0xffff)
		upd_osii.os_rev_minor = osii->os_rev_minor;
	if (osii->logical_start_block != 0xffffffff)
		upd_osii.logical_start_block = osii->logical_start_block;
	if (osii->ddr_load_address != 0xffffffff)
		upd_osii.ddr_load_address = osii->ddr_load_address;
	if (osii->entery_point != 0xffffffff)
		upd_osii.entery_point = osii->entery_point;
	if (osii->size_of_os_image != 0xffffffff)
		upd_osii.size_of_os_image = osii->size_of_os_image;
	if (osii->attribute != 0xff)
		upd_osii.attribute = osii->attribute;

	osip->desc[update_number] = upd_osii;

	printf("into write_OSII_entry!\n");

	if (write_OSIP(osip) < 0) {
		printf("fail write OSIP\n");
		return -1;
	}

	read_OSIP_loc(osip, R_START, DUMP_OSIP);
	return 0;
}

int invalidate_handle(int inval_cnt, int *inval_values)
{
	int i, j;
	struct OSIP_header osip;
	int lsb, size_of_os_image;

	if (read_OSIP_loc(&osip, R_START, NOT_DUMP) < 0) {
		printf("fail reading OSIP\n");
		return -1;
	}

	for (i = 0; i < inval_cnt; i++) {
		for (j = 0; j < OSII_TOTAL; j++) {
			if (inval_values[i] == osip.desc[j].attribute) {
				printf("into invalidate entry\n");
				printf("invalidate attribute = %d\n",
				       osip.desc[j].attribute);
				lsb = osip.desc[j].logical_start_block;
				size_of_os_image =
				    osip.desc[j].size_of_os_image;
				memset((void *)&osip.desc[j], 0,
				       sizeof(struct OSII));
				osip.desc[j].logical_start_block = lsb;
				osip.desc[j].size_of_os_image =
				    size_of_os_image;
				osip.desc[j].attribute = inval_values[i];
				break;
			}
		}

		if (j >= OSII_TOTAL){
			printf("Can't find attribute %d\n", inval_values[i]);
			return -1;}
	}

	if (write_OSIP(&osip) < 0) {
		printf("fail write OSIP\n");
		return -1;
	}

	return 0;
}

int flash_payload_os_image(char *image)
{
	return flash_stitch_image(image, 0);
}

int restore_payload_osii_entry(void)
{
	return restore_handle();
}
