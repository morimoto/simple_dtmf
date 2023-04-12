#! /bin/bash
#===============================
#
# mix+split all-test
#
# 2022/12/07 Kuninori Morimoto <kuninori.morimoto.gx@renesas.com>
#===============================
TOP=`readlink -f "$0" | xargs dirname`

${TOP}/S-mix-amixer
${TOP}/S-split-amixer

# test 8ch first, because of unknown noise
${TOP}/S-split-test

# no sleep is needed...

${TOP}/S-mix-test
