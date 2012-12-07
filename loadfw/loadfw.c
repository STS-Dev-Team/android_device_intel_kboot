/*
 *  loadfw.c - Application to upgrade firmware(for IA/SCU/PUnit/Spectra devices)
 *  on NAND,using IPC driver.
 *  Copyright (C) 2009 Intel Corp
 *  Author :Sudha Krishnakumar, Intel Inc.
 *  Copyright (C) 2009 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 * This program is distributed in the hope it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License 
 * for more details. You should have received a copy of the GNU Lesser General Public 
 * License along with this program; if not, write to the Free Software Foundation, Inc., 
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Jan 5th,2010: Fixed minor issues with debug statements.
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define DEVICE_NAME "/dev/mid_ipc"
#define DEVICE_FW_UPGRADE	0xA2
#define FW_BIN_FILE_SIZE 258*1024
#define FILE_EXT ".bin"


int main(int argc , char **argv)
{

  char *fwBinFile = NULL;
  char *fwFileData = NULL;
  FILE *fwp = NULL;
  struct stat sb;
  int devfd,errNo;
  char *tempPtr;

  if( getuid() != 0)
  {
    fprintf(stderr,"Need to be root to run this application\n");
    exit(1);
  }

  if(argc != 2)
  {
    fprintf(stderr,"Incorrect args,correct usage is %s FW.bin\n",argv[0]);
    exit(1);
  }

  //Checks to see if the file is terminated by *.bin
  if ( (tempPtr = strrchr(argv[1],'.')) == NULL)
  {
    fprintf(stderr,"Invalid inputs, correct usage is %s FW.bin\n",argv[0]);
    exit(1);
  }

  if( strncmp(tempPtr,FILE_EXT,strlen(FILE_EXT)) )
  {
    fprintf(stderr,"File doesnt have *.bin extn,correct usage is %s FW.bin\n",argv[0]);
    exit(1);
  }
 
  fwBinFile = argv[1];

  fprintf(stderr,"fw file is %s\n",fwBinFile);
 
  if( !(fwp = fopen(fwBinFile, "rb")) )
  {
    perror("fopen error:Unable to open file\n");
    exit(1);
  }

  if (fstat(fileno(fwp),&sb) == -1) 
  {
     perror("fstat error\n");
     fclose(fwp);
     exit(1);
  }
 
  fprintf(stderr,"file size is %u\n",(unsigned)sb.st_size);
   
  if(sb.st_size != FW_BIN_FILE_SIZE )
  {
     fprintf(stderr,"Invalid FW bin file,unexpected file size is %u\n",(unsigned)sb.st_size);
     fclose(fwp);
     exit(1);
  }
     
  if ( (fwFileData = calloc(sb.st_size,1)) == NULL)
  {
     fclose(fwp);
     exit(1);
  }

  if( fread(fwFileData,1,sb.st_size,fwp) < sb.st_size )
  {
     perror("unable to fread fw bin file into buffer\n");
     free(fwFileData);
     fclose(fwp);
     exit(1);
  }
     
  //Lets open the char device file.
  if( (devfd = open(DEVICE_NAME,O_RDWR)) == -1 )
  {
     fprintf(stderr,"unable to open the DEVICE %s\n",DEVICE_NAME);
     free(fwFileData);
     fclose(fwp);
     exit(1);
  }
    
  //Use char driver's ioctl interface to upgrade firmware.
  if( ( errNo = ioctl(devfd,DEVICE_FW_UPGRADE,fwFileData)) < 0 )
  {
     fprintf(stderr,"ioctl for DEVICE %s, returns error-%d\n",DEVICE_NAME,errNo);
  }
     
  free(fwFileData);
  fclose(fwp);
  close(devfd);

  exit(0);

}

