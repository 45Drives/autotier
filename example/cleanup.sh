#!/bin/sh

# This script will unmount and remove all files created when using the 
# atexample.sh script, it should be run from the autotier/example folder
# using sudo ./cleanup.sh

# ensure that the user is running autotier from the 
# correct directory 
RUN_DIR="/autotier/example"
if echo "$PWD" | grep -q "$RUN_DIR"; then
	echo "[AUTOTIER]: cleaning up files"
else
  	echo "[AUTOTIER]: cleanup.sh must be run from autotier/example";
  	exit 1
fi

if [ "$EUID" -ne 0 ]
  then echo "[AUTOTIER]: Please run as root (sudo ./atexample.sh)"
  exit 1
fi

# unmount all of the disk images created using the example
echo "[AUTOTIER]: unmounting example disk images"
if echo "$lsblk" | grep -q "$PWD/autoTier_MountPoint"; then
	umount $PWD/autoTier_MountPoint -l
fi
umount $PWD/tierA_MountPoint -l
umount $PWD/tierB_MountPoint -l
umount $PWD/tierC_MountPoint -l

# remove all of the files that were created using atexample.sh
echo "[AUTOTIER]: removing all example files"
rm -rf tier*
rmdir autoTier_MountPoint
rm blockDevice*
rm atexample.conf
