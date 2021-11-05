#!/bin/bash
#
JOBS=`grep -c processor /proc/cpuinfo`
let JOBS=${JOBS}*2
JOBS="-j${JOBS}"
ARM=arm64
BOOT_PATH="arch/${ARM}/boot"
IMAGE="Image"
DZIMAGE="dzImage"

ARGC=$((${#}))
RELEASE=${1}
MODEL=${2}

COUNT=3
while [ ${COUNT} -le ${ARGC} ]; do
	eval "VARIANT=\${${COUNT}}"
	if [ "${VARIANT}" != "" ]; then
		if [ -f arch/${ARM}/configs/${MODEL}_${VARIANT}_defconfig ]; then
			echo "Merge ${MODEL}_${VARIANT}_defconfig to ${MODEL}_variant_defconfig"
			cat arch/${ARM}/configs/${MODEL}_${VARIANT}_defconfig >> arch/${ARM}/configs/${MODEL}_variant_defconfig
		else
			echo "There is no arch/${ARM}/configs/${MODEL}_${VARIANT}_defconfig"
		fi
	fi
	COUNT=$((${COUNT}+1))
done

if [ "${RELEASE}" = "factory" ]; then
	echo "Now enable CONFIG_SEC_FACTORY for ${MODEL}_defconfig"

	sed -i 's/# CONFIG_SEC_FACTORY is not set/\CONFIG_SEC_FACTORY=y/g' arch/${ARM}/configs/${MODEL}_defconfig
	if [ "$?" != "0" ]; then
		echo "Failed to enble CONFIG_SEC_FACTORY feature"
		exit 1
	fi
elif [ "${RELEASE}" = "usr" ]; then
	echo "Now disable CONFIG_TIZEN_SEC_KERNEL_ENG for ${MODEL}_defconfig"

	sed -i 's/CONFIG_TIZEN_SEC_KERNEL_ENG=y/\# CONFIG_TIZEN_SEC_KERNEL_ENG is not set/g' arch/${ARM}/configs/${MODEL}_defconfig
	if [ "$?" != "0" ]; then
		echo "Failed to disable CONFIG_TIZEN_SEC_KERNEL_ENG feature"
		exit 1
	fi

	echo "Now disable CONFIG_DYNAMIC_DEBUG for ${MODEL}_defconfig"

	sed -i 's/CONFIG_DYNAMIC_DEBUG=y/\# CONFIG_DYNAMIC_DEBUG is not set/g' arch/${ARM}/configs/${MODEL}_defconfig
	if [ "$?" != "0" ]; then
		echo "Failed to disable CONFIG_DYNAMIC_DEBUG feature"
		exit 1
	fi
fi

if [ -f arch/${ARM}/configs/${MODEL}_variant_defconfig ]; then
	make ARCH=${ARM} ${MODEL}_defconfig VARIANT_DEFCONFIG=${MODEL}_variant_defconfig
else
	make ARCH=${ARM} ${MODEL}_defconfig
fi

if [ "$?" != "0" ]; then
	echo "Failed to make defconfig :"${ARCH}
	exit 1
fi

rm ${BOOT_PATH}/dts/exynos/*.dtb -f
if [ -f arch/${ARM}/configs/${MODEL}_variant_defconfig ]; then
	rm arch/${ARM}/configs/${MODEL}_variant_defconfig
fi
make ${JOBS} ARCH=${ARM} ${IMAGE}
if [ "$?" != "0" ]; then
	echo "Failed to make "${IMAGE}
	exit 1
fi

DTC_PATH="scripts/dtc/"

dtbtool -o ${BOOT_PATH}/merged-dtb -p ${DTC_PATH} -v ${BOOT_PATH}/dts/exynos/
if [ "$?" != "0" ]; then
	echo "Failed to make merged-dtb"
	exit 1
fi

mkdzimage -o ${BOOT_PATH}/${DZIMAGE} -k ${BOOT_PATH}/${IMAGE} -d ${BOOT_PATH}/merged-dtb
if [ "$?" != "0" ]; then
	echo "Failed to make mkdzImage"
	exit 1
fi
