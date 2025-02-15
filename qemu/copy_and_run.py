#!/usr/bin/env python3 
#
# Run as root 
#
#

import os 
import shutil
import sys 
from subprocess import Popen,PIPE
import tempfile
import time

def check_root(): 
   if os.geteuid() != 0:
       print("Must be root") 
       exit(1) 

def exec_local(command):
    os.system(command)

YOCTO_IMAGE = "core-image-minimal-qemux86.ext4"
LINUXIMAGE="bzImage16_Linux6.8.0-rc7"

def get_images():
     for image in [YOCTO_IMAGE,"disk1.img","disk2.img",LINUXIMAGE]:
         if not os.path.exists(image):
            exec_local("wget https://rogerpease.com/LinuxKernelSimpleRamDisk/"+image)
         else:
            print("already had "+image)

def update_image(): 
    with tempfile.TemporaryDirectory() as tmpdirname:
        exec_local("mount -t ext4 -o loop "+YOCTO_IMAGE + " " + tmpdirname)
        for filename in ["../kernel/ram-disk-queue.ko", "../kernel/ram-disk-bio.ko",
                         "../user/ram-disk-test"]: 
            shutil.copy(filename, tmpdirname+"/home/root") 
        exec_local("umount "+tmpdirname)


def run_qemu():

   print("Login as root (no password), insmod one of the .ko modules and run ./ram-disk-test. ")
   print("rmmod the module once done and you can run the other if you want.")
   Popen(['qemu-system-x86_64','-kernel',LINUXIMAGE, \
        '-device', 'virtio-serial',                 \
        '-chardev','pty,id=virtiocon0','-device','virtconsole,chardev=virtiocon0', \
        '-netdev', 'tap,id=lkt-tap0,ifname=lkt-tap0,script=no,downscript=no', \
        '-net',    'nic,netdev=lkt-tap0,model=virtio',  \
        '-netdev', 'tap,id=lkt-tap1,ifname=lkt-tap1,script=no,downscript=no', \
        '-net',    'nic,netdev=lkt-tap1,model=i82559er', \
        '-drive',  'file=core-image-minimal-qemux86.ext4,if=virtio,format=raw',\
        '-drive',  'file=disk1.img,if=virtio,format=raw',\
        '-drive',  'file=disk2.img,if=virtio,format=raw',\
        '-s','-m','256', \
        '--append','root=/dev/vda loglevel=15 console=hvc0 pci=noacpi nokaslr'],
        )

check_root()
get_images()
update_image() 
run_qemu()
