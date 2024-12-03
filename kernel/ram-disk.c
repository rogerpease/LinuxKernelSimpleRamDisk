/*
 *  Taken from the Linux kernel labs but the API changed in Linux 6. kernel.  
 *  References:
 *     * https://elixir.bootlin.com/linux/v6.7.10/source/drivers/block/brd.c
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
#include <linux/pagemap.h>

MODULE_DESCRIPTION("Simple RAM Disk");
MODULE_AUTHOR("SO2");
MODULE_LICENSE("GPL");

#define DISK_NAME "mydisk"


#define KERN_LOG_LEVEL		KERN_ALERT

#define MY_BLOCK_MAJOR		240
#define MY_BLKDEV_NAME		"mybdev"
#define MY_BLOCK_MINORS		1
#define NR_SECTORS		128

#define KERNEL_SECTOR_SIZE	512


static struct my_block_dev {
	struct blk_mq_tag_set tag_set;
	struct request_queue *queue;
	struct gendisk *gd;
	u8 *data;
	size_t size;
} g_dev;

static void copy_to_brd(void * mem_start, unsigned int sector,unsigned int  len)
{ pr_info("Write to %p %d %d\n",mem_start,sector,len); }

static void copy_from_brd(void * mem_start, unsigned int sector,unsigned int len)
{ pr_info("Read to %p %d %d\n",mem_start,sector,len); }

static int copy_to_brd_setup(unsigned int sector,unsigned int  len,unsigned int  gfp)
{ pr_info("Setup %d %d %x",sector,len,gfp); return 0; }

static int brd_do_bvec(struct page *page,
			unsigned int len, unsigned int off, blk_opf_t opf,
			sector_t sector)
{
	void *mem;
	int err = 0;

	if (op_is_write(opf)) {
		/*
		 * Must use NOIO because we don't want to recurse back into the
		 * block or filesystem layers from page reclaim.
		 */
		gfp_t gfp = opf & REQ_NOWAIT ? GFP_NOWAIT : GFP_NOIO;

		err = copy_to_brd_setup(sector, len, gfp);
		if (err)
			goto out;
	}

	mem = kmap_atomic(page);
	if (!op_is_write(opf)) {
		copy_from_brd(mem + off, sector, len);
		flush_dcache_page(page);
	} else {
		flush_dcache_page(page);
		copy_to_brd(mem + off, sector, len);
	}
	kunmap_atomic(mem);

out:
	return err;
}

static void my_submit_bio(struct bio *bio)
{
    pr_info("My submit bio");
    struct brd_device *brd = bio->bi_bdev->bd_disk->private_data;
    sector_t sector = bio->bi_iter.bi_sector;
    struct bio_vec bvec;
    struct bvec_iter iter;

    bio_for_each_segment(bvec, bio, iter) {
	unsigned int len = bvec.bv_len;
	int err;

	/* Don't support un-aligned buffer */
	WARN_ON_ONCE((bvec.bv_offset & (SECTOR_SIZE - 1)) ||
			(len & (SECTOR_SIZE - 1)));

	err = brd_do_bvec(bvec.bv_page, len, bvec.bv_offset,
			  bio->bi_opf, sector);
	if (err) {
		if (err == -ENOMEM && bio->bi_opf & REQ_NOWAIT) {
			bio_wouldblock_error(bio);
			return;
		}
		bio_io_error(bio);
		return;
	}
	sector += len >> SECTOR_SHIFT;
    }
    bio_endio(bio);
}

static const struct block_device_operations my_block_ops = {
	.owner = THIS_MODULE,
	.submit_bio = my_submit_bio,
};

static int create_block_device(struct my_block_dev *dev)
{

        struct gendisk *disk; 
	int result; 
	int err;
 	pr_info("create_block_device1 called"); 

	dev->size = NR_SECTORS * KERNEL_SECTOR_SIZE;
	dev->data = kzalloc(dev->size,GFP_KERNEL);

	if (dev->data == NULL) {
		pr_err("vmalloc: out of memory\n");
		err = -ENOMEM;
		goto out_vmalloc;
	}


        if (IS_ERR(disk)) { 
		pr_err("DISK ALLOCATION ERROR"); 
		return -1; 
        }
        pr_info("made disk");

	disk = blk_alloc_disk(NUMA_NO_NODE);

	disk->major = MY_BLOCK_MAJOR;
	disk->first_minor = 0;
	disk->minors = 2;
	disk->fops = &my_block_ops;
	disk->private_data = dev;
	strscpy(disk->disk_name, DISK_NAME,strlen(DISK_NAME));
	set_capacity(disk, NR_SECTORS*2);

        pr_info("disk queue setup start");
        blk_queue_physical_block_size(disk->queue, 4096u);

//	blk_queue_flag_set(QUEUE_FLAG_NONROT, disk->queue);
//	blk_queue_flag_set(QUEUE_FLAG_SYNCHRONOUS, disk->queue);
//	blk_queue_flag_set(QUEUE_FLAG_NOWAIT, disk->queue);
        pr_info("disk queue setup end");
        
        pr_info("Adding Disk"); 
	result = add_disk(disk);
        if (result) 
        {
           pr_err("add disk went bad");
           goto out_add_disk;
        } 
        pr_info("added disk ok");

        dev->gd = disk;

	return 0;

out_add_disk:
	del_gendisk(dev->gd);
	put_disk(dev->gd);

out_alloc_disk:
	blk_put_queue(dev->queue);
out_blk_init:
	blk_mq_free_tag_set(&dev->tag_set);
out_alloc_tag_set:
	vfree(dev->data);
out_vmalloc:
	return err;
}

static int __init my_block_init(void)
{
	int err = 0;
        pr_info("my_block_init\n");

	/* TODO 1: register block device */
        err = register_blkdev(MY_BLOCK_MAJOR, MY_BLKDEV_NAME);
        if (err < 0) {
             pr_err("unable to register mybdev block device\n");
             return -EBUSY;
        }

        pr_info("CreateBlockDev calling\n");
	err = create_block_device(&g_dev); 
        pr_info("CreateBlockDev called\n");
        if (err < 0) {
             pr_err("unable to register mybdev block device\n");
             goto out;
        }
        pr_info("err >= 0\n");

	return 0;

out:
	unregister_blkdev(MY_BLOCK_MAJOR, MY_BLKDEV_NAME);
	return err;
}

static void delete_block_device(struct my_block_dev *dev)
{
	if (dev->gd) {
		del_gendisk(dev->gd);
		put_disk(dev->gd);
	}

	if (dev->queue)
		blk_put_queue(dev->queue);
	if (dev->tag_set.tags)
		blk_mq_free_tag_set(&dev->tag_set);
	if (dev->data)
		vfree(dev->data);
}

static void __exit my_block_exit(void)
{
	/* TODO 2: cleanup block device using delete_block_device */

	/* TODO 1: unregister block device */
	unregister_blkdev(MY_BLOCK_MAJOR, MY_BLKDEV_NAME);

}

module_init(my_block_init);
module_exit(my_block_exit);
