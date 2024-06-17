#! /bin/bash
#===============================
#
# A-auto-test
#
# 2023/03/14 Kuninori Morimoto <kuninori.morimoto.gx@renesas.com>
#===============================
TOP=`readlink -f "$0" | xargs dirname`

dmesg | grep -w "snd-kf-split$" > /dev/null
if [ $? = 0 ]; then
	${TOP}/S-split-amixer
	${TOP}/S-split-test
fi

dmesg | grep -w "snd-ulcb-mix$" > /dev/null
if [ $? = 0 ]; then
	${TOP}/S-mix-amixer
	${TOP}/S-mix-test
fi

dmesg | grep -w "snd-kf$" > /dev/null
if [ $? = 0 ]; then
	${TOP}/S-8ch-amixer
	${TOP}/S-8ch-test
	echo "sleep 5 sec"
	sleep 5
fi

dmesg | grep -w "snd-ulcb$" > /dev/null
if [ $? = 0 ]; then
	${TOP}/S-2ch-amixer
	${TOP}/S-2ch-test
fi
