#!/bin/sh

# run this shell script from the autotier/example directory 

# ensure that the user is running autotier from the 
# correct directory 
RUN_DIR="/autotier/example"
if echo "$PWD" | grep -q "$RUN_DIR"; then
	echo "[AUTOTIER]: setting up autotier example"
else
  	echo "[AUTOTIER]: atexample.sh must be run from autotier/example";
  	exit 1
fi

if [ "$EUID" -ne 0 ]
  then echo "[AUTOTIER]: Please run as root (sudo ./atexample.sh)"
  exit 1
fi

# First we need to make directories to serve as our mount points
echo "[AUTOTIER]: creating mount point directories"

mkdir tierA_MountPoint
mkdir tierB_MountPoint
mkdir tierC_MountPoint
mkdir autoTier_MountPoint

# We'll make a copy of the default configuration called atexample.conf
# which will be used in the autotier example
echo "[AUTOTIER]: creating configuration files"
cp default.conf atexample.conf

# now let's make some disk images (10M each)
echo "[AUTOTIER]: creating example disk images"
{
dd if=/dev/zero of=blockDeviceA.img bs=1M count=10
dd if=/dev/zero of=blockDeviceB.img bs=1M count=10
dd if=/dev/zero of=blockDeviceC.img bs=1M count=10
}&> /dev/null

# now make a file system for the disks (ext4)
echo "[AUTOTIER]: creating file systems for example disk images"
{
mkfs.ext4 blockDeviceA.img
mkfs.ext4 blockDeviceB.img
mkfs.ext4 blockDeviceC.img
}&> /dev/null

# and mount them to the mount points that we created
echo "[AUTOTIER]: mounting disks"
{
mount $PWD/blockDeviceA.img $PWD/tierA_MountPoint
mount $PWD/blockDeviceB.img $PWD/tierB_MountPoint
mount $PWD/blockDeviceC.img $PWD/tierC_MountPoint
}&> /dev/null

# let's put some files on blockDeviceA in this 
# case, we're just making some 1M files
echo "[AUTOTIER]: creating files:"
cd tierA_MountPoint
for x in {A..H}
do
	{
		dd if=/dev/zero of=file$x bs=1M count=1
		echo "$PWD/file$x"
	}&> /dev/null
done
cd ..

# this makes the necessary changes to the 
# config file
echo "[AUTOTIER]: modifying the configuration file (atexample.conf)"
{
sed -i 's,MOUNT_POINT=/path/for/autotier/goes/here,'MOUNT_POINT=$PWD/autoTier_MountPoint',' atexample.conf
sed -i 's,DIR=/path/for/fastest/tier/goes/here,'DIR=$PWD/tierA_MountPoint',' atexample.conf
sed -i 's,DIR=/path/for/medium/tier/goes/here,'DIR=$PWD/tierB_MountPoint',' atexample.conf
sed -i 's,DIR=/path/for/slowest/tier/goes/here,'DIR=$PWD/tierC_MountPoint',' atexample.conf
} &> /dev/null

# adjust permissions for all created example files and directories
chmod -R 777 autoTier_MountPoint tierA_MountPoint tierB_MountPoint tierC_MountPoint

# remove the lost+found directories
rm -r "tierA_MountPoint/lost+found"
rm -r "tierB_MountPoint/lost+found"
rm -r "tierC_MountPoint/lost+found"

echo "[AUTOTIER]: example setup complete:" 
echo "run \"sudo ./autotier -c atexample.conf run\" to start autotier"
echo "run \"sudo ./cleanup.sh \" to unmount disk images and remove created files"


# now we run autotier, making sure that 
# we use the atexample.conf configuration file using the -c option
#./autotier -c atexample.conf run -o allow_other