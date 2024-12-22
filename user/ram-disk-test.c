/*
 * SO2 - Block device driver (#8)
 * Test suite for exercise #3 (RAM Disk)
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <linux/ioctl.h>
#include <sys/ioctl.h>


#define WR_VALUE 'w'
#define RD_VALUE 'r'

#define NR_SECTORS	128
#define SECTOR_SIZE	512

#define DEVICE_NAME	"/dev/myblock"
#define MODULE_NAME	"ram-disk"
#define MY_BLOCK_MAJOR	"240"
#define MY_BLOCK_MINOR	"0"


#define max_elem_value(elem)	\
	(1 << 8*sizeof(elem))

static unsigned char buffer[SECTOR_SIZE*NR_SECTORS];
static unsigned char buffer_copy[SECTOR_SIZE];

static void test_sectors()
{
	int i,sector, fd;

	for (i = 0; i < sizeof(buffer); i++)
	     buffer[i] = (i-i%SECTOR_SIZE)/SECTOR_SIZE; 
        
	fd = open(DEVICE_NAME, O_RDWR);
 
	for (sector = 0; sector < NR_SECTORS; sector++)
        {
   	    lseek(fd, sector * SECTOR_SIZE, SEEK_SET);
            write(fd, buffer+sector*SECTOR_SIZE, SECTOR_SIZE);
        }

	close(fd);
	fd = open(DEVICE_NAME, O_RDWR);

	for (sector = 0; sector < NR_SECTORS; sector++)
        {
           lseek(fd, sector * SECTOR_SIZE, SEEK_SET);
           read(fd, buffer_copy, sizeof(buffer_copy));
           printf("test sector %3d ... ", sector);
           if (memcmp(buffer+sector*SECTOR_SIZE, buffer_copy, sizeof(buffer_copy)) == 0)
                printf("passed\n");
           else
                printf("failed\n");
        } 
	close(fd);
}


int main(void)
{

	printf("insmod ../kernel/" MODULE_NAME ".ko\n");
	if (system("insmod " MODULE_NAME ".ko\n")) {
		fprintf(stderr, "insmod failed\n");
		exit(EXIT_FAILURE);
	}

	sleep(1);

	printf("mknod " DEVICE_NAME " b " MY_BLOCK_MAJOR " " MY_BLOCK_MINOR "\n");
	system("mknod " DEVICE_NAME " b " MY_BLOCK_MAJOR " " MY_BLOCK_MINOR "\n");
	sleep(1);

        printf("Open\n"); 
	test_sectors();


	sleep(1);
	printf("rmmod " MODULE_NAME "\n");
	system("rmmod " MODULE_NAME "\n");

	return 0;
}
