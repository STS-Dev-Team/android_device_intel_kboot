/*************************************************************************
 * Copyright(c) 2011 Intel Corporation. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * This is medfield firmware update application.
 *
 * **************************************************************************/

/* Brief Overview * /
 * ------------------
 *
 * This is medfield firmware update application for medfield(B0 silicon).
 * This application expects signed DnX binary and IFWI binary as inputs.
 * It checks for mid_ipc device entry in /proc/misc, and creates a node under /dev.
 * It creates DnX header, and parses IFWI to get FUPH header and passes a bunch
 * of parameters to ipc utility driver through ioctl.
 * ipc driver subsequently picks up the pieces, and works with SCU, to serially
 * update firmware chunks. If there are any errors encountered, ipc driver
 * will quit, and control will return back to the application. If firmware update
 * gets completed, then control returns back to this application. This application
 * is expected to sync the file system and do a system reboot.
 */

/* Issues-TODOs-TBDs-To-Be-Confirmed items listed below:
 * ----------------------------------------------------
 - Current usage model, is to use this application from Provising OS(POS).
POS is expected to do a reboot later in the update sequence.
 - For failure cases, if fw-update fails half-way thro, ioctl will return error,
but still need to do a reboot, since we could be having DnX binary
in SCU execution RAM.
 - If device has a very old version of IFWI, and if we are trying to upgrade,
to a very new version of IFWI, there could be issues trying to update, if there are any
feature changes that are required for update to work correctly, between the 2 versions.
 - Also if there are any irrecoverable errors, during update (For Example, failure to write to emmc),
when device reboots, it will get into Firmware Recovery Mode, and
it would require external tool like FSTK to reflash.
 - Based on discussions with architects,
	-- Current method, is for kernel to disable
	power management during firmware update. So POS, will NOT have OSPM enabled.
	-- Also expectation is that file-system is unmounted, and SCU wil be the only
	entity that can access emmc during firmware update. If this is not taken care
	of, it can result in file-system corruption.

*/

#include <stdio.h>
#include <stdint.h>

#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>

#define INTEL_SCU_IPC_MEDFIELD_FW_UPDATE		0xA3
#define DEVICE_NAME "/dev/mid_ipc"

#define DEV_LIST  "/proc/devices"
#define MISC_LIST "/proc/misc"
#define IPC_DEV   "mid_ipc"
#define MISC_DEV  "misc"

#define GPF_BIT32  1

#define DNX_SIZE (128*1024)
#define IFWI_SIZE (1024*1024*3)

#define DNX_SIZE_OFFSET 0
#define GP_FLAG_OFFSET 4
#define XOR_CHK_OFFSET 20

#define FUPH_STR "UPH$"
#define FUPH_STR_LEN 4
#define FUPH_MAX_LEN 36
#define SKIP_BYTES 8

typedef struct {
	unsigned char *fwFileData;
	unsigned int fsize;
	unsigned char *dnxhdr;
	unsigned char *dnxFileData;
	unsigned int dnxsize;
	unsigned int fuph_hdr_len;
} fw_ud;

static int find_device_id(int *id, char *proc_file, char *dev_name);
static int checksufix(char * fw);
static int find_fuph_header_len(unsigned int *len, unsigned char *file_data, unsigned int file_size);

int main(int argc, char **argv)
{

/*TODO_SK
Add code to provide basic validation for inputs, ie check if image supplied to program is infact
DnX image, check for CDPH signature, at the end of the image.
Similary UPH$ for IFWI image.
*/
	struct stat sb;
	int devfd, errNo;

	char *dnxBinFile = NULL;
	unsigned char *dnxFileData = NULL;
	unsigned int gpFlags = 0;
	unsigned int xorcs;
	unsigned int dnxFileSize;
	unsigned char dnxSH[24] = { 0 };
	int i, size,bytes_read;
	char *fwBinFile = NULL;
	unsigned char *fwFileData = NULL;

	int major = 0;
	int minor = 0;
	dev_t ipc_util;
	mode_t dev_mode;
	int dnx_fd,ifwi_fd;

	fw_ud fwud;
	int ret = -1;
	bool reboot = false;

	if (argc != 3) {
		fprintf(stderr,
			"Incorrect args,correct usage is %s DnX.bin IFWI.bin\n",
			argv[0]);
		goto end;
	}

	dnxBinFile = argv[1];
	fwBinFile = argv[2];

	fprintf(stderr, "dnx file is =%s,fw=%s\n", dnxBinFile, fwBinFile);
        if(checksufix(dnxBinFile))
        {
            printf("dnx file is not ended with .bin\n");
            goto end;
        }
        if(checksufix(fwBinFile))
        {
            printf("firmware file is not ended with .bin\n");
            goto end;
        }

	/* Find misc in /proc/devices */

	if (find_device_id(&major, DEV_LIST, MISC_DEV) < 0) {
		fprintf(stderr,"\nUnable to find major no for dev=%s", MISC_DEV);
		goto end;
	} else
		fprintf(stderr,"\nFound major no=%d for dev=%s", major, MISC_DEV);

	/* Find mid_ipc in /proc/misc and create device entry */

	if (find_device_id(&minor, MISC_LIST, IPC_DEV) < 0) {
		fprintf(stderr,"\nUnable to find minor no for dev=%s", IPC_DEV);
		goto end;
	} else
		fprintf(stderr,"\nFound minor no=%d for dev=%s", minor, IPC_DEV);

	ipc_util = makedev(major, minor);

	unlink(DEVICE_NAME);

	dev_mode = S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;

	if (mknod(DEVICE_NAME, dev_mode, ipc_util) != 0) {
		fprintf(stderr,"Unable to create entry for =%s\n", DEVICE_NAME);
		goto end;
	}

	/* Open DnX file */
	dnx_fd = open(dnxBinFile,O_RDONLY);

	if (dnx_fd < 0) {
		perror("open error:Unable to open file\n");
		goto end;
	}

	if (fstat(dnx_fd, &sb) == -1) {
		goto dnxp;
	}

	fprintf(stderr, "file size is=%ld\n", sb.st_size);

	//TODO_SK::Need to check with SCU folks, if we can have this check here..or not..
	if ((sb.st_size > DNX_SIZE) || (sb.st_size == 0)) {
		fprintf(stderr,
			"Invalid FW bin file,unexpected file size-->%ld\n",
			sb.st_size);
		goto dnxp;
	}

	if ((dnxFileData = calloc(sb.st_size, 1)) == NULL) {
		goto dnxp;
	}

	bytes_read = read(dnx_fd,dnxFileData,sb.st_size);

	if(bytes_read < 0) {
		perror("unable to read dnx bin file into buffer\n");
		goto dnxdata;
	}

	size = sb.st_size;
	dnxFileSize = size;

	/* Set GPFlags parameter */
	gpFlags = gpFlags | (GPF_BIT32 << 31);

	xorcs = (size ^ gpFlags);

	memcpy( (dnxSH + DNX_SIZE_OFFSET),(unsigned char *)(&size),4);
	memcpy( (dnxSH + GP_FLAG_OFFSET),(unsigned char *)(&gpFlags),4);
	memcpy( (dnxSH + XOR_CHK_OFFSET),(unsigned char *)(&xorcs),4);

	/* Open IFWI File */
	ifwi_fd = open(fwBinFile,O_RDONLY);

	if (ifwi_fd < 0) {
		perror("open error:Unable to open file\n");
		goto dnxdata;
	}

	if (fstat(ifwi_fd, &sb) == -1) {
		perror("fstat error\n");
		goto fwp;
	}

	fprintf(stderr, "file size is=%ld\n", sb.st_size);

        /*
         * In C0, Integrated Firmware can be more than 2MB due to support
         * for SCU ROM patch.
         */
	if (sb.st_size > IFWI_SIZE || (sb.st_size == 0)) {
		fprintf(stderr,
			"Invalid FW bin file,unexpected file size-->%ld\n",
			sb.st_size);
		goto fwp;
	}

	if ((fwFileData = calloc(sb.st_size, 1)) == NULL) {
		goto fwp;
	}

	bytes_read = read(ifwi_fd,fwFileData,sb.st_size);

	if(bytes_read < 0) {
		perror("unable to read ifwi bin file into buffer\n");
		goto fwdata;
	}

	size = sb.st_size;

	fwud.fwFileData = fwFileData;
	fwud.fsize = size;
	fwud.dnxhdr = dnxSH;
	fwud.dnxFileData = dnxFileData;
	fwud.dnxsize = dnxFileSize;

	fprintf(stderr,"\nfsize=%d,dnxs=%d\n", fwud.fsize, fwud.dnxsize);
	fprintf(stderr,"\nPassing Firmware to IPC driver ioctl\n");

	if (find_fuph_header_len(&(fwud.fuph_hdr_len), fwud.fwFileData, fwud.fsize) < 0) {
		fprintf(stderr,"Error, with FUPH header\n");
		goto fwdata;
	} 

	//open the char device file.
	if ((devfd = open(DEVICE_NAME, O_RDWR)) == -1) {
		fprintf(stderr, "\n,unable to open the DEVICE=%s", DEVICE_NAME);
		goto fwdata;
	}

	system("sync");

	//Use char driver's ioctl interface to upgrade firmware.
	if ((errNo = ioctl(devfd, INTEL_SCU_IPC_MEDFIELD_FW_UPDATE, &fwud)) < 0) {
		fprintf(stderr, "\n, ioctl for DEVICE=%s, returns error-%d",
			DEVICE_NAME, errNo);
	} else {
		fprintf(stderr, "\n, ioctl for DEVICE=%s, SUCCESS,ret-code=%d",
			DEVICE_NAME, errNo);
		ret = 0;
	}

	//Post-ioctl at this point, we could have DnX binary in SCU eRAM, so better to reboot
	//for failure cases too.
	//Current usage model is to use this application(ifwi-update) only from Prov. OS(POS)
	//Based on latest update(Aug 5th,2011), from Xiaokang and Emmanuel Berthier, re-enabling
	//reboot in this application. Expectation is that file-system is alredy unmounted in POS,
	//prior to invoking this ifwi-update application.

	reboot = true;

	close(devfd);
fwdata:
	free(fwFileData);
fwp:
	close(ifwi_fd);
dnxdata:
	free(dnxFileData);
dnxp:
	close(dnx_fd);
end:

	if (reboot) {
		system("sync");
		system("reboot -f");
	}

	return ret;

}

/* Parses from the end of IFWI, and looks for UPH$, 
 * to determine length of FUPH header */
static int find_fuph_header_len(unsigned int *len, unsigned char *file_data, unsigned int file_size)
{

	int ret = -1;
	unsigned char *temp;
	unsigned int cnt = 0;

	if (!len || !file_data || !file_size ) {
		printf("Invalid inputs \n");
		return -1;
	}

	//Skipping the checksum at the end, and moving to the 
	//start of the last add-on firmware size in fuph.
	temp = file_data + file_size - SKIP_BYTES;

	while (cnt <= FUPH_MAX_LEN) {
		if (!strncmp(temp, FUPH_STR, FUPH_STR_LEN)) {
			printf("Fuph_hdr_len=%d\n", cnt + SKIP_BYTES);
			*len = cnt + SKIP_BYTES;
			ret = 0;
			break;
		}
		temp -= 4;
		cnt +=4;
	}

	return ret;

}


/*
 *Generic helper function to retreive device-ids in proc files, given device-name.
 */
static int find_device_id(int *id, char *proc_file, char *dev_name)
{

	FILE *fp;
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	int ret = -1;
	int found = false;

	if( !id || !proc_file || !dev_name) {
		printf("input param NULL, find_device_id\n");
		return -1;
	}

	fp = fopen(proc_file, "r");

	if (fp == NULL) {
		return -1;
	}

	while (((read = getline(&line, &len, fp)) != -1) && (!found)) {

		if (strstr(line, dev_name) != NULL) {

			if (sscanf(line, "%d", id) == 1) {
				found = true;
				ret = 0;
			} else {
				break;
			}
		}
	}

	if (line)
		free(line);

	fclose(fp);
	return ret;

}

static int checksufix(char * fw)
{
   char * sufix=NULL;
   if(!fw)
   {
     printf("input file name is NULL.\n");
     return -1;
   }
   sufix=strrchr(fw,'.');
   if(!sufix)
   {
      return -1;

   }
   if(strcmp(sufix,".bin"))
   {
      return -1;
   }
   return 0;
}
