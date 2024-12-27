/*
 * RDP: Based on linux-kernel-labs Block device driver lab but adapted for Kernel 6.x. 
 * RDP: This is the bio-version. 
 * 
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/blk_types.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/bio.h>
#include <linux/vmalloc.h>

MODULE_DESCRIPTION("Simple RAM Disk");
MODULE_AUTHOR("SO2 modified by RDP for Kernel 6");
MODULE_LICENSE("GPL");

#define KERN_LOG_LEVEL		KERN_ALERT

#define MY_BLOCK_MAJOR		240
#define MY_BLKDEV_NAME		"mybdev"
#define MY_BLOCK_MINORS		1
#define NR_SECTORS		128

#define KERNEL_SECTOR_SIZE	512


/* "Private" data structure used to contain information about my block device */

struct my_block_dev {
     struct gendisk *gd;
     u8 *data;
     size_t size;
};

static struct my_block_dev *g_dev;

/* Technically not necessary */
static int my_block_open(struct gendisk *gd, fmode_t mode) { return 0; }
static void my_block_release(struct gendisk *gd) { return; }

/*

   If you put in pr_ statements to debug reads and writes, don't be surprised to see the reads happening first. 
   There is a higher level caching layer which is "fetching" (reading) then writing. 
  
   So if your user program is doing write then a read, 
   that layer is doing a read first to acquire the original device data (with a read), 
   then a write of the changed/'dirty' areas, then not doing the final read 
   because it already has the data cached. 

*/

static void my_block_transfer(
                struct my_block_dev *dev,  // Device being operated on. 
                sector_t sector,           // Sector being operated on 
		unsigned long len,         // Length of transfer 
                char *buffer,              // start of memory section. 
                int dir)                   // Direction. 
{
	unsigned long sectorstart = sector * KERNEL_SECTOR_SIZE;

	if ((sectorstart + len) > dev->size)
        {
	   return;
        }

        if (dir == WRITE) // write  
        { 
           memcpy(dev->data + sectorstart , buffer, len);
           pr_info("Wrote %lu bytes to   sector %llu",len,sector);
        } 
        else
        {
           memcpy(buffer, dev->data + sectorstart, len);
           pr_info("Read  %lu bytes from sector %llu",len,sector);
        }
}
////////////////////////////////////////////////////////////////////////////
//
// When an upper level (say the filesystem) accesses a file, it indicates:
//
//     the sector of the drive to work on 
//     a page of main memory containing the data to write or where 
//           read data should go 
//     Length and direction
// 
// When the server does a request, it submits one or more requests to a queue. 
//    No matter how you do things the requests will boil down to those. 
// 
// The most basic driver just takes each bio request (in bvec) and executes the
// appropriate writes (or reads). Writes and reads are always separated
// per "bio" request.  
//
// For SSDs or ram-disks this is fine as the order of the requests doesn't 
// matter. 
//
// For platter drives, it makes sense to reorder the requests 
// (such as for an elevator algorithm). Elevators select their next floors based on the elevator's 
// position and direction, not the order in which the floor requests came in. So the driver will 
// configure (or provide) a queue so it can intercept the requests and 
// reorder them. 
//
// See ram-disk-queue for a queue based example. 
//
// 

static void my_submit_bio(struct bio * bio)
{

      struct bio_vec bvec;
      struct bvec_iter iter;

      bio_get(bio); // Similar to a semaphore to get a bio. 
      bio_for_each_bvec(bvec, bio, iter)
      {
	   unsigned int len = bvec.bv_len;
           void * mem = kmap_atomic(bvec.bv_page); 
                    // 'Pin' the user-space page we are reading from/writing to and get the pointer. 
	   unsigned int offset = bvec.bv_offset; // Offset is page offset. 
           sector_t sector     = iter.bi_sector; 
           unsigned char dir   = bio_data_dir(bio); 

           my_block_transfer(g_dev, sector, len, mem+offset, dir);
           kunmap_atomic(mem);
      }
      bio_put(bio);
      bio_endio(bio);
     
      return;

}

static const struct block_device_operations my_block_ops = {
      .owner = THIS_MODULE,
      .open = my_block_open, 
      .release = my_block_release, 
      .submit_bio = my_submit_bio,
      .devnode=NULL,
};

static void delete_block_device(struct my_block_dev *dev)
{
	if (dev->gd) {
		del_gendisk(dev->gd);
		put_disk(dev->gd);
	}

	if (dev->data)
		vfree(dev->data);
}

static int create_block_device(struct my_block_dev *dev)
{
	int err;

	dev->size = NR_SECTORS * KERNEL_SECTOR_SIZE;
	dev->data = kzalloc(dev->size,GFP_KERNEL);

	if (dev->data == NULL) {
		pr_err("vmalloc: out of memory\n");
		err = -ENOMEM;
                goto out_vmalloc;
	}


	/* Initialize the gendisk structure. This builds its own queue. */
	dev->gd = blk_alloc_disk(MY_BLOCK_MINORS);
	if (!dev->gd) {
		pr_err("alloc_disk: failure\n");
		err = -ENOMEM;
                goto out_alloc_dev_data;
	}

	dev->gd->major = MY_BLOCK_MAJOR;
	dev->gd->first_minor = 0;
	dev->gd->minors = MY_BLOCK_MINORS;
        dev->gd->flags = GENHD_FL_NO_PART;

	dev->gd->fops = &my_block_ops;
	dev->gd->private_data = dev;
	snprintf(dev->gd->disk_name, DISK_NAME_LEN, "myblock");

	set_capacity(dev->gd, 0); // Setting capacity to non-zero made 
                                  // add_disk hang trying to scan for 
                                  // partitions. 
	err = add_disk(dev->gd);
        if (err < 0)
        {
            goto out_blk_alloc; 
        } 
	set_capacity(dev->gd, NR_SECTORS);

	return 0;

out_blk_alloc:
        delete_block_device(dev); 
out_alloc_dev_data:
	vfree(dev->data);
out_vmalloc:
	return err;
}


static int __init my_block_init(void)
{
	int err = 0;
        g_dev = kmalloc(sizeof(struct my_block_dev),GFP_KERNEL); 

        if (g_dev == NULL) {
            pr_err("Failed to allocate struct block_dev\n");
            return -ENOMEM;
        }

	/* TODO 1: register block device */
        err = register_blkdev(MY_BLOCK_MAJOR, MY_BLKDEV_NAME);
        if (err < 0) {
             pr_err("unable to register mybdev block device\n");
             err = -EBUSY;
             goto free_gdev;
        }

	/* TODO 2: create block device using create_block_device */
	err = create_block_device(g_dev); 
        if (err < 0) {
             pr_err("unable to register mybdev block device\n");
             goto unregister;
        }

	return 0;

unregister:
	unregister_blkdev(MY_BLOCK_MAJOR, MY_BLKDEV_NAME);
free_gdev:
        vfree(g_dev);
	return err;
}

static void __exit my_block_exit(void)
{
        delete_block_device(g_dev);
        vfree(g_dev); 
	unregister_blkdev(MY_BLOCK_MAJOR, MY_BLKDEV_NAME);

}

module_init(my_block_init);
module_exit(my_block_exit);
