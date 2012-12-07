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

/* simple program for check ifwi version
 */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

//#define DEBUG

#define DEVICE_NAME "/dev/mid_ipc"
#define INTE_SCU_IPC_FW_REVISION_GET  0xB0
#define FIP_pattern 0x50494624
#define IFWI_offset 36

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;

struct IA32_rev{
	uint8 intel_reserved;
	uint8 IA32_minor;
	uint8 IA32_major;
	uint8 IA32_checksum;
};

struct Punit_rev{
	uint8 intel_reserved;
	uint8 Punit_minor;
	uint8 Punit_major;
	uint8 Punit_checksum;
};

struct OEM_rev{
	uint8 intel_reserved;
	uint8 OEM_minor;
	uint8 OEM_major;
	uint8 OEM_checksum;
};

struct suppIA32_rev{
	uint8 intel_reserved;
	uint8 suppIA32_minor;
	uint8 sippIA32_major;
	uint8 suppIA32_checksum;
};

struct SCU_rev{
	uint8 intel_reserved;
	uint8 SCU_minor;
	uint8 SCU_major;
	uint8 SCU_checksum;
};

struct Chaabi_rev{
	uint32 Chaabi_icache;
	uint32 Chaabi_resident;
	uint32 Chaabi_ext;
};

struct IFWI_rev{
	uint8 IFWI_minor;
	uint8 IFWI_major;
	uint16 intel_reserved;
};

struct FIP_header{
	uint32 FIP_SIG;
	uint32 header_info;
	struct IA32_rev ia32_rev;
	struct Punit_rev punit_rev;
	struct OEM_rev oem_rev;
	struct suppIA32_rev suppia32_rev;
	struct SCU_rev scu_rev;
	struct Chaabi_rev chaabi_rev;
	struct IFWI_rev ifwi_rev;
};

struct scu_ipc_version {
	uint32	count;  /* length of version info */
	uint8	data[16]; /* version data */
};

static int find_fw_rev(void)
{
	int devfd, errNo;
	struct scu_ipc_version version;
	uint32 ifwi_version;

	version.count = 16;    /*read back 16 bytes fw info from IPC*/
	if ((devfd = open(DEVICE_NAME, O_RDWR)) == -1) {
		fprintf(stderr, "unable to open the DEVICE %s\n", DEVICE_NAME);
		return -1;
	}

	if ((errNo = ioctl(devfd, INTE_SCU_IPC_FW_REVISION_GET, &version)) < 0) {
		fprintf(stderr, "finding fw_info, ioctl for DEVICE %s, returns error-%d\n", DEVICE_NAME, errNo);
		return -1;
	}

#ifdef DEBUG
	int i;
	for (i = 0; i < 16; ++i) {
		printf("offset=%x, value=%2x\n", i, version.data[i]);
	}
#endif
	close(devfd);
	ifwi_version = version.data[15];
	ifwi_version = (ifwi_version << 8) + version.data[14];
	printf("The current ifwi version on board is %x\n", ifwi_version);
	return ifwi_version;
}

static int crack_update_fw(char *fw_file){
	struct FIP_header fip;
	FILE * fd;
	int tmp = 0;
	int location;
	uint32 ifwi_version = 0;

	memset((void *)&fip, 0, sizeof(fip));

	if ((fd = fopen(fw_file, "rb")) == NULL) {
		fprintf(stderr, "fopen error: Unable to open file\n");
		return -1;
	}

	while (tmp != FIP_pattern) {
		int cur;
		fread(&tmp, sizeof(int), 1, fd);
		if (ferror(fd) || feof(fd)) {
			printf("find FIP_pattern failed\n");
			fclose(fd);
			return -1;
		}
		cur = ftell(fd) - sizeof(int);
		fseek(fd, cur + sizeof(char), SEEK_SET);
	}
	location = ftell(fd) - sizeof(char);

	fseek(fd, location, SEEK_SET);
	fread((void *)&fip, sizeof(fip), 1, fd);
	if (ferror(fd) || feof(fd)) {
		printf("read of FIP_header failed\n");
		fclose(fd);
		return -1;
	}
	fclose(fd);

	ifwi_version = fip.ifwi_rev.IFWI_major;
	ifwi_version = (ifwi_version << 8) + fip.ifwi_rev.IFWI_minor;
	printf("Crack of fip_header works\n");
	printf("Firmware file %s's IFWI version is %x\n",fw_file, ifwi_version);

	return ifwi_version;
}

static int compare_fw_rev(char *filename){
	uint32 cur_fw = 0;
	uint32 upd_fw = 0;

	cur_fw = find_fw_rev();
#ifdef DEBUG
	printf("The ifwi version is %x\n",cur_fw);
#endif
	upd_fw = crack_update_fw(filename);

	if (cur_fw == -1 || upd_fw == -1)
		return -1;

	if(upd_fw > cur_fw) {
		printf("Waiting to update new firmware %s......\n", filename);
		return 0;
	} else if (upd_fw < cur_fw) {
		printf("The %s is out of date,do not update IFWI\n", filename);
		return 1;
	} else {
		printf("The %s is the same with current version.\n", filename);
		return 2;
	}
}

int main(int argc,char *argv[])
{
	if(argc != 2){
		printf("IFWI version check tool Usage:\nIFWI_check /tmp/ifwi_firmware.bin\n");
		return -1;
	}

	return compare_fw_rev(argv[1]);
}
