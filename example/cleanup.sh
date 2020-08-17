#!/bin/sh

# This script will unmount and remove all files created when using the 
# atexample.sh script, it should be run from the autotier/example folder
# using sudo ./cleanup.sh

# unmount all of the disk images created using the example
umount $PWD/tierA_MountPoint
umount $PWD/tierB_MountPoint
umount $PWD/tierC_MountPoint

# remove all of the files that were created using atexample.sh
rm -r tier*
rm -r autoTier_MountPoint
rm blockDevice*
rm atexample.conf
