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

int main(int argc, char **argv)
{
	int c, ret, update_num = 0;
	int get_flag = 0;
	int inval_cnt = 0;
	int inval_values[10];
	int backup_flag = 0, restore_flag = 0, update_flag = 0, inval_flag =
	    0, check_flag = 0;
	struct OSII osii;
	struct OSIP_header ori_osip;
	struct OSIP_header dis_osip;
	char *fwBinFile = NULL;

	memset((void *)&osii, 0xFF, sizeof(osii));	/*set osii to all 1 by default */
	memset((void *)inval_values, 0, sizeof(inval_values));

	while (1) {
		static struct option osip_options[] = {
			{"backup", no_argument, NULL, 'b'},
			{"check", no_argument, NULL, 'c'},
			{"invalidate", required_argument, NULL, 'i'},
			{"image", required_argument, NULL, 'g'},
			{"restore", no_argument, NULL, 'r'},
			{"update", required_argument, NULL, 'u'},
			/*below options are parameters of OSII
			   TODO:lba should not be changed           */
			{"revmaj", required_argument, NULL, 'm'},
			{"revmin", required_argument, NULL, 'n'},
			{"addr", required_argument, NULL, 'a'},
			{"entry", required_argument, NULL, 'e'},
			{"lba", required_argument, NULL, 'l'},
			{"size", required_argument, NULL, 's'},
			{"attrib", required_argument, NULL, 't'},
			{0, 0, 0, 0}	/*end of long_options */
		};

		/* getopt_long stores the option index here. */
		int option_index = 0;

		c = getopt_long(argc, argv, "hbcrg:i:u:m:n:a:e:l:s:t:",
				osip_options, &option_index);

		/* Detect the end of the options. */
		if (c == -1) {
			if (!get_flag)
				display_usage();
			break;
		}
		get_flag = 1;

		switch (c) {
		case 0:
			/* If this option set a flag, do nothing else now. */
			if (osip_options[option_index].flag != 0)
				break;
			printf("option %s", osip_options[option_index].name);
			if (optarg)
				printf(" with arg %s", optarg);
			printf("\n");
			break;

		case 'b':
			printf("option -back up\n");
			backup_flag = 1;
			break;

		case 'c':
			printf("option -check\n");
			check_flag = 1;
			break;

		case 'r':
			printf("option restore osip!\n");
			restore_flag = 1;
			break;

		case 'i':
			printf("option -invalidate with value `%s'\n", optarg);
			inval_values[inval_cnt++] = atoi(optarg);
			inval_flag = 1;
			break;

		case 'g':
			printf("option --image with value `%s'\n", optarg);
			fwBinFile = optarg;
			break;

		case 'u':
			printf("option -update with value `%s'\n", optarg);
			update_num = atoi(optarg);
			update_flag = 1;
			break;

		case 'm':
			printf("option -m with value `%s'\n", optarg);
			osii.os_rev_major = atoi(optarg);
			break;

		case 'n':
			printf("option -n with value `%s'\n", optarg);
			osii.os_rev_minor = atoi(optarg);
			break;

		case 'a':
			printf("option -a with value `%s'\n", optarg);
			osii.ddr_load_address = atoi(optarg);
			break;

		case 'e':
			printf("option -e with value `%s'\n", optarg);
			osii.entery_point = atoi(optarg);
			break;

		case 'l':
			printf("option -l with value `%s'\n", optarg);
			osii.logical_start_block = atoi(optarg);
			break;

		case 's':
			printf("option -s with value `%s'\n", optarg);
			osii.size_of_os_image = atoi(optarg);
			break;

		case 't':
			printf("option -t with value `%s'\n", optarg);
			osii.attribute = atoi(optarg);
			break;

		case 'h':
		case '?':
			display_usage();
			/* getopt_long already printed an error message. */
			break;

		default:
			abort();
		}
	}

	/* Print any remaining command line arguments (not options). */
	if (optind < argc) {
		printf("non-option ARGV-elements: ");
		while (optind < argc)
			printf("%s ", argv[optind++]);
		putchar('\n');
	}

	/*Handle all possible situations. */
	if (backup_flag == 1) {
		if(read_OSIP_loc(&ori_osip, R_START, DUMP_OSIP))
			goto error;
		if(backup_handle(&ori_osip))
			goto error;
	}

	if (restore_flag == 1) {
		if(restore_handle())
			goto error;
	}

	if (update_flag == 1) {
		if (fwBinFile) {
			if(flash_stitch_image(fwBinFile, update_num))
				goto error;
		}
		if(update_handle(&osii, update_num))
			goto error;
	}

	if (inval_flag == 1) {
		if (backup_flag != 1) {
			printf
			    ("You have to backup valid OSIP before invalidate!\n");
			goto error;
		}
		if(invalidate_handle(inval_cnt, inval_values))
			goto error;
	}

	if (check_flag == 1) {
		if(read_OSIP_loc(&ori_osip, R_START, DUMP_OSIP))
			goto error;
		if(read_OSIP_loc(&ori_osip, R_BCK, DUMP_OSIP))
			goto error;
	}
	exit(0);

error:
	printf("Program Early Terminated!\n");
	exit(-1);
}
