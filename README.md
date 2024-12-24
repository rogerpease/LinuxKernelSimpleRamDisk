I was playing with the (excellent) [Linux Kernel Labs](linux-kernel-labs.github.io) but realized they were targeted for early Linux 5.x kernels. I decided to adapt to Linux 6. 

These versions were tested on qemu with Linux 6.8.0-rc7.

There is a lot of confusing material on block device drivers. I don't want to add to the confusion but when reading all of it I would keep the following in mind: 


- The physical disk (block devices)'s job is to read and write (usually) 512-byte sectors and to service other minor requests (temperature, Write metrics, Serial numbers, etc). Some newer disks have 1024 or even 4096 byte sectors. 
- The physical disk and the driver do not know anything about filenames, directories, links or partitions. Those are handled by the filesystem. All the block device does is read and write blocks plus handle other minor requests. 
- A *disk* (gendisk) structure is the abstraction that each filesystem accesses. /dev/sda is usually the "whole" block device. /dev/sda1.../dev/sdan are the partitions. 
- The filesystem generally does not know anything about the geometry of the disk. The filesystem submits requests, which consist of bio blocks, which are the commands to read and write specific 512-byte sectors.
- A bio request contains of a vector of bio_vec requests. 
- If there is a way to reorder the bio requests such that they are more efficiently written to disk, then a queue is helpful.  
--  The classical example of this is an elevator algorithm on platter drives, where blocks are written based on where the drive heads are and direction they are haded rather than the order the requests arrived.  
-- If there is no advantage to the order in which blocks are read/written (such as for a RAM disk or SSD) then you can just read/write the bio-s directly and not set up a special queue. 
- You submit requests and get 'tags' which tell you the status of requests. Those requests go into a queue (either a default one or your own). The software queues can then reorder/separate requests to optimize for geometry and get sent to hardware queues. The hardware queues then perform the actual operations. 
- If you write a sector you are (apparently) expected to write the whole sector. So a filesystem-level request for writing an uninitialized sector may have a read beforehand to get the initial data. 
- With the differing kernel versions the changes are generally in how things are allocated/the order of allocation and the introduction of multi-queues. That is why the lab needed to be updated. 
