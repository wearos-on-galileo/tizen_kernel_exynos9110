#!/bin/bash
#
JOBS=`grep -c processor /proc/cpuinfo`
let JOBS=${JOBS}*2
JOBS="-j${JOBS}"
RELEASE_DATE=`date +%Y%m%d`
COMMIT_ID=`git log --pretty=format:'%h' -n 1`
ARM=arm64
BOOT_PATH="arch/${ARM}/boot"
IMAGE="Image"
DZIMAGE="dzImage"
ARGC=$((${#}))
MODEL=${1}
TIZEN_MODEL=tizen_${MODEL}

HOST_OS=`uname -m`
if [ $HOST_OS = "x86_64" ]; then
	#toolchain for 64bit HOST OS
	export CROSS_COMPILE="/opt/toolchain-aarch64/bin/aarch64-tizen-linux-gnu-"
else
	echo "Tizen4.0 only support 64bit environment. Please try to build with 64bit environment"
	exit 0
fi

if [ "${MODEL}" = "" ]; then
	echo "Warnning: failed to get machine id."
	echo "ex)./release.sh model_name region_name"
	echo "ex)--------------------------------------------------"
	echo "ex)./release.sh galileo large"
	echo "ex)./release.sh galileo large lte na"
	echo "ex)./release.sh galileo large lte kor"
	echo "ex)./release.sh galileo large lte eur"
	echo "ex)./release.sh galileo large lte chn"
	echo "ex)./release.sh galileo small"
	echo "ex)./release.sh galileo small lte na"
	echo "ex)./release.sh galileo small lte kor"
	echo "ex)./release.sh galileo small lte eur"
	echo "ex)./release.sh pulse"
	echo "ex)./release.sh pulse lte"
	echo "ex)./release.sh renaissance"
	echo "ex)./release.sh renaissance lte"
	echo "ex)./release.sh renaissance_chub"
	echo "ex)./release.sh renaissance_chub lte"
	exit
fi

COUNT=2
declare -A v_array
while [ ${COUNT} -le ${ARGC} ]; do
	eval "VARIANT=\${${COUNT}}"
	if [ "${VARIANT}" != "" ]; then
		v_array[$VARIANT]=1
		if [ -f arch/${ARM}/configs/${TIZEN_MODEL}_${VARIANT}_defconfig ]; then
			echo "Merge ${TIZEN_MODEL}_${VARIANT}_defconfig to ${TIZEN_MODEL}_variant_defconfig"
			cat arch/${ARM}/configs/${TIZEN_MODEL}_${VARIANT}_defconfig >> arch/${ARM}/configs/${TIZEN_MODEL}_variant_defconfig
		else
			echo "File not found: arch/${ARM}/configs/${TIZEN_MODEL}_${VARIANT}_defconfig"
		fi
	fi
	COUNT=$((${COUNT}+1))
done

if [ -f arch/${ARM}/configs/${TIZEN_MODEL}_variant_defconfig ]; then
	make ARCH=${ARM} ${TIZEN_MODEL}_defconfig VARIANT_DEFCONFIG=${TIZEN_MODEL}_variant_defconfig
else
	make ARCH=${ARM} ${TIZEN_MODEL}_defconfig
fi


if [ "$?" != "0" ]; then
	echo "Failed to make defconfig :"$ARCH
	exit 1
fi

rm ${BOOT_PATH}/dts/exynos/*.dtb -f
rm ${BOOT_PATH}/merged-dtb -f
rm ${BOOT_PATH}/${DZIMAGE}.u -f
if [ -f arch/${ARM}/configs/${TIZEN_MODEL}_variant_defconfig ]; then
	rm arch/${ARM}/configs/${TIZEN_MODEL}_variant_defconfig
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

if [ "${MODEL}" = "galileo" ]; then
	mv ${BOOT_PATH}/${DZIMAGE} ${BOOT_PATH}/${DZIMAGE}.u
	dd if=/dev/zero of=${BOOT_PATH}/dummy bs=1 count=272
	cat ${BOOT_PATH}/dummy >> ${BOOT_PATH}/${DZIMAGE}.u
	rm ${BOOT_PATH}/dummy
	if [ ${v_array[large]} ]; then
		if [ ${v_array[lte]} ]; then
			if [ ${v_array[na]} ]; then
				MODEL_NAME=SM-R805U_NA_USA_USA0
			elif [ ${v_array[kor]} ]; then
				MODEL_NAME=SM-R805N_KOR_SKC_KOR0
			elif [ ${v_array[eur]} ]; then
				MODEL_NAME=SM-R805F_EUR_XX_EUR0
			elif [ ${v_array[chn]} ]; then
				MODEL_NAME=SM-R8050_CHN_CHN_CHN0
			else
				# default SM-R805U_NA_USA
				MODEL_NAME=SM-R805U_NA_USA_USA0
			fi
		else
			MODEL_NAME=SM-R800_NA_USA_USA0
		fi
	elif [ ${v_array[small]} ]; then
		if [ ${v_array[lte]} ]; then
			if [ ${v_array[na]} ]; then
				MODEL_NAME=SM-R815U_NA_USA_USA0
			elif [ ${v_array[kor]} ]; then
				MODEL_NAME=SM-R815N_KOR_SKC_KOR0
			elif [ ${v_array[eur]} ]; then
				MODEL_NAME=SM-R815F_EUR_XX_EUR0
			else
				# default SM-R815U_NA_USA
				MODEL_NAME=SM-R815U_NA_USA_USA0
			fi
		else
			MODEL_NAME=SM-R810_NA_USA_USA0
		fi
	fi
	java -jar ./scripts/signclient.jar -runtype ss_exynos40_all -model ${MODEL_NAME} -input ${BOOT_PATH}/${DZIMAGE}.u -output ${BOOT_PATH}/${DZIMAGE}
elif [ "${MODEL}" = "pulse" ]; then
	if [ ${v_array[lte]} ]; then
		MODEL_NAME=PULSE_LTE
		echo "this is lte model"
	else
		mv ${BOOT_PATH}/${DZIMAGE} ${BOOT_PATH}/${DZIMAGE}.u
		dd if=/dev/zero of=${BOOT_PATH}/dummy bs=1 count=272
		cat ${BOOT_PATH}/dummy >> ${BOOT_PATH}/${DZIMAGE}.u
		rm ${BOOT_PATH}/dummy
		MODEL_NAME=SM-R500_NA_STA_USA0
		java -jar ./scripts/signclient.jar -runtype ss_exynos40_all -model ${MODEL_NAME} -input ${BOOT_PATH}/${DZIMAGE}.u -output ${BOOT_PATH}/${DZIMAGE}
	fi
elif [ "${MODEL}" = "renaissance" ]; then
	if [ ${v_array[lte]} ]; then
		MODEL_NAME=RENAISSANCE_LTE
		echo "this is lte model"
	else
		MODEL_NAME=RENAISSANCE
		echo "this is bt model"
	fi
elif [ "${MODEL}" = "renaissance_chub" ]; then
	if [ ${v_array[lte]} ]; then
		MODEL_NAME=RENAISSANCE_CHUB_LTE
		echo "this is lte model"
	else
		MODEL_NAME=RENAISSANCE_CHUB
		echo "this is bt model"
	fi
fi

RELEASE_IMAGE=System_${MODEL_NAME}_${RELEASE_DATE}-${COMMIT_ID}.tar

tar cf ${RELEASE_IMAGE} -C ${BOOT_PATH} ${DZIMAGE}
if [ "$?" != "0" ]; then
	echo "Failed to tar ${DZIMAGE}"
	exit 1
fi

echo ${RELEASE_IMAGE}
