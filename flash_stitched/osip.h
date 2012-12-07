#include "manage_device.h"

#define BACKUP_LOC	0xE0
#define OSIP_PREAMBLE	0x20
#define OSIP_SIG	0x24534f24     /* $OS$ */

#define FILE_EXT	".bin"
#define ANDROID_OS	0
#define POS		1
#define COS 		3

#define OSII_TOTAL 	7
#define DUMP_OSIP 	1
#define NOT_DUMP 	0
#define R_BCK 		1
#define R_START 	0

#ifdef __ANDROID__
#define MMC_DEV_POS	"/dev/block/mmcblk0"
#else
#define MMC_DEV_POS	"/dev/mmcblk0"
#endif

#define MMC_PAGES_PER_BLOCK	1
#define MMC_PAGE_SIZE		512 /* sector size actually */
#define STITCHED_IMAGE_PAGE_SIZE  512
#define STITCHED_IMAGE_BLOCK_SIZE 512

#define IPC_WRITE_RR_TO_OSNIB	0xC2
#define IPC_DEVICE_NAME		"/dev/mid_ipc"
#define RR_SIGNED_MOS		0x0

int read_OSIP_loc(struct OSIP_header *, int, int);
int write_OSII_entry(struct OSII *, int, int);

int restore_handle(void);
int flash_payload_os_image(char *);
