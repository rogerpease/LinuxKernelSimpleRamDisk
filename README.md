I was playing with the (excellent) [Linux Kernel Labs](linux-kernel-labs.github.io) but realized they were targeted for early Linux 5.x kernels. 

These versions were tested on qemu with Linux 6.8.0-rc7. Passing Qemu is not a guarantee the driver will work on a real system but it is a good indicator if it won't. 

There is a lot of confusing material on block device drivers. I don't want to add to the confusion but when reading all of it I would keep the following in mind: 

- The physical disk (block devices)'s job is to read and write 512-byte sectors and to service 
  other minor requests (temperature, Write metrics, Serial numbers, etc). 
- The block device does not know anything about filenames, directories, links or partitions. 
  Those are handled by the filesystem.
- A *disk* (gendisk) structure is the abstraction that each system accesses. /dev/sda is usually the "whole" block device. 
- The filesystem does not know anything about the geometry of the disk. The filesystem submits requests, which consist of bio blocks.
- If there is a way to reorder the bio requests such that they are more efficiently written to disk, then a queue is helpful.  
  The classical example of this is an elevator algorithm on platter drives, where blocks are written based on where the drive heads currently are rather than which request arrived. 
- If there is no advantage to the order in which blocks are read/written then you can just work off the bios directly and not set up a queue. 
 
