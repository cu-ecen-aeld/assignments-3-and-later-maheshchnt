#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi

cd linux-stable 

if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- mrproper
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- defconfig
    cd ../
fi

    make -j24 ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- all
    make -j24 ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- modules
    make -j24 ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- dtbs


echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

mkdir -p ${OUTDIR}/rootfs
cd ${OUTDIR}/rootfs
#create root filesystem directories
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}

    make distclean
    make defconfig
else
    cd busybox
fi

echo "initializing busybox....."
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

cd ${OUTDIR}/rootfs

echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

echo "Copying required library modules to rootfs"
# Add library dependencies to rootfs
cp -r /home/mgudipati/COURSE/AESD/ARM_TOOL_CHAIN/gcc-arm-10.2-2020.11-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/lib .
cp -r /home/mgudipati/COURSE/AESD/ARM_TOOL_CHAIN/gcc-arm-10.2-2020.11-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/lib64 .

echo "Creating device nodes"
# Make device nodes
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 600 dev/console c 5 1

# Clean and build the writer utility
cd /home/mgudipati/COURSE/AESD/ASSIGNMENT_3/assignment-2-maheshchnt/finder-app
make clean
make CROSS_COMPILE=${CROSS_COMPILE}

echo "copying program files and scripts to home (rootfs)"
# Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp -r writer ${OUTDIR}/rootfs/home 
cp -r finder.sh ${OUTDIR}/rootfs/home 
cp -r finder-test.sh ${OUTDIR}/rootfs/home 
cp -r autorun-qemu.sh ${OUTDIR}/rootfs/home
mkdir -p ${OUTDIR}/rootfs/home/conf
cp -r ../conf/username.txt ${OUTDIR}/rootfs/home/conf
echo "Adding the Image in outdir"
cp -r /home/mgudipati/COURSE/AESD/LINUX/linux-stable/arch/arm64/boot/Image ${OUTDIR}

# Chown the root directory
cd ${OUTDIR}/rootfs
sudo chown -R root:root *

# Create initramfs.cpio.gz
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd ${OUTDIR}
gzip -f initramfs.cpio
