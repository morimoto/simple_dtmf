#! /bin/bash
#===============================
#
# 2ch+8ch-test
#
# 2022/12/07 Kuninori Morimoto <kuninori.morimoto.gx@renesas.com>
#===============================
TOP=`readlink -f "$0" | xargs dirname`

${TOP}/S-2ch-amixer
${TOP}/S-8ch-amixer

# test 8ch first, because of unknown noise
${TOP}/S-8ch-test

# sleep 5, because of unknown noise
sleep 5

${TOP}/S-2ch-test
