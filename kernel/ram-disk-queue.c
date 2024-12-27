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
     struct blk_mq_tag_set tag_set;
     u8 *data;
     size_t size;
};

static struct my_block_dev *g_dev;


/*

   If you read dmesg to debug reads and writes, don't be surprised to see 
   the reads happening first. 
   There is a higher level caching layer. 
  
   So if your user program is doing write then a read, 
   that layer is doing a read first to acquire the original device data, 
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

	/* check for read/write beyond end of block device */
	if ((sectorstart + len) > dev->size)
        {
	   return;
        }

	/* TODO 3: read/write to dev buffer depending on dir */
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

////////////////////////////////////////////////////////////////////////
//
// Instead of directly processing the bio requests, here we can reorder them/
// group them differently into different queues if we need to. 
// In this case we just process them directly. 
//
//

static blk_status_t  my_queue_request(struct blk_mq_hw_ctx *hctx,
                                     const struct blk_mq_queue_data *bd)
{
    pr_info("my_queue_request\n");

    struct request *rq = bd->rq;
    struct bio_vec bvec;
    struct req_iterator iter;
    struct my_block_dev *dev = hctx->queue->queuedata;

    blk_mq_start_request(rq);

    if (blk_rq_is_passthrough(rq)) {
        printk (KERN_NOTICE "Skip non-fs request\n");
        blk_mq_end_request(rq, BLK_STS_IOERR);
        goto out;
    }


    rq_for_each_segment(bvec, rq, iter) {
        sector_t sector = iter.iter.bi_sector;
        char *buffer = kmap_atomic(bvec.bv_page);
        unsigned long offset = bvec.bv_offset;
        size_t len = bvec.bv_len;
        int dir = bio_data_dir(iter.bio);

        my_block_transfer(dev, sector, len, buffer + offset, dir);
    
        kunmap_atomic(buffer);
    }

    blk_mq_end_request(rq, BLK_STS_OK);

out:
    return BLK_STS_OK; 
}

static const struct block_device_operations my_block_ops = {
      .owner = THIS_MODULE,
      .devnode=NULL,
};


static struct blk_mq_ops my_queue_ops = {
      .queue_rq = my_queue_request,
};

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


        // Set up the tag set. When you submit read and write requests,
        // these track whether or not they were actually done. 

        g_dev->tag_set.ops = &my_queue_ops;
        g_dev->tag_set.nr_hw_queues = 1; 
        g_dev->tag_set.queue_depth = 10; 
        g_dev->tag_set.numa_node = NUMA_NO_NODE;
        g_dev->tag_set.cmd_size = 0;
        g_dev->tag_set.flags = BLK_MQ_F_SHOULD_MERGE;
        g_dev->tag_set.cmd_size = 10; 
         
        // And allocate the actual tags. 
        blk_mq_alloc_tag_set(&g_dev->tag_set);
        if (err < 0) {
		err = -ENOMEM;
		goto out_dev_data;
        } 

	/* Initialize the gendisk structure using the queue we built. */
	/* blk_alloc_disk will build its own queue. */
	dev->gd = blk_mq_alloc_disk(&g_dev->tag_set,dev);

	if (!dev->gd) {
		pr_err("alloc_disk: failure\n");
		err = -ENOMEM;
		goto out_tag_set;
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
           goto out_disk; 
        } 
	set_capacity(dev->gd, NR_SECTORS);

	return 0;

out_disk: 
        put_disk(g_dev->gd); 
out_tag_set: 
        blk_mq_free_tag_set(&g_dev->tag_set);
out_dev_data:
	vfree(dev->data);
out_vmalloc:
	return err;
}

static void delete_block_device(struct my_block_dev *dev)
{
	if (dev->gd) {
		del_gendisk(dev->gd);
		put_disk(dev->gd);
	}

	if (dev->data)
		vfree(dev->data);
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
             return -EBUSY;
        }

	/* TODO 2: create block device using create_block_device */
	err = create_block_device(g_dev); 
        if (err < 0) {
             pr_err("unable to register mybdev block device\n");
             goto out;
        }

	return 0;

out:
        delete_block_device(g_dev);
	return err;
}


static void __exit my_block_exit(void)
{
        delete_block_device(g_dev); 
        put_disk(g_dev->gd); 
        blk_mq_free_tag_set(&g_dev->tag_set);
	vfree(g_dev->data);
	vfree(g_dev);
	unregister_blkdev(MY_BLOCK_MAJOR, MY_BLKDEV_NAME);
	return;
}


module_init(my_block_init);
module_exit(my_block_exit);
