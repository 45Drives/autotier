#!/bin/sh

# run this shell script to perform the necessary 
# commands for the example

# First we need to make directories to serve as our mount points
mkdir tierA_MountPoint
mkdir tierB_MountPoint
mkdir tierC_MountPoint
mkdir autoTier_MountPoint

# We'll make a copy of the default configuration called atexample.conf
# which will be used in the autotier example
cp default.conf atexample.conf

# now let's make some disk images (10M each)
dd if=/dev/zero of=blockDeviceA.img bs=1M count=10
dd if=/dev/zero of=blockDeviceB.img bs=1M count=10
dd if=/dev/zero of=blockDeviceC.img bs=1M count=10

# now make a file system for the disks (ext4)
mkfs.ext4 blockDeviceA.img
mkfs.ext4 blockDeviceB.img
mkfs.ext4 blockDeviceC.img

# and mount them to the mount points that we created
mount $PWD/blockDeviceA.img $PWD/tierA_MountPoint
mount $PWD/blockDeviceB.img $PWD/tierB_MountPoint
mount $PWD/blockDeviceC.img $PWD/tierC_MountPoint


# let's put some files on blockDeviceA in this 
# case, we're just making some 1M files
cd tierA_MountPoint
dd if=/dev/zero of=fileA bs=1M count=1
dd if=/dev/zero of=fileB bs=1M count=1
dd if=/dev/zero of=fileC bs=1M count=1
dd if=/dev/zero of=fileD bs=1M count=1
dd if=/dev/zero of=fileE bs=1M count=1
dd if=/dev/zero of=fileF bs=1M count=1
cd ..

# this makes the necessary changes to the 
# config file
sed -i 's,MOUNT_POINT=/path/for/autotier/goes/here,'MOUNT_POINT=$PWD/autoTier_MountPoint',' atexample.conf
sed -i 's,DIR=/path/for/fastest/tier/goes/here,'DIR=$PWD/tierA_MountPoint',' atexample.conf
sed -i 's,DIR=/path/for/medium/tier/goes/here,'DIR=$PWD/tierB_MountPoint',' atexample.conf
sed -i 's,DIR=/path/for/slowest/tier/goes/here,'DIR=$PWD/tierC_MountPoint',' atexample.conf



# now we run autotier, making sure that 
# we use the atexample.conf configuration file using the -c option
./autotier -c atexample.conf run