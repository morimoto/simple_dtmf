#! /bin/bash
#===============================
#
# A-auto-test
#
# 2025/04/02 Kuninori Morimoto <kuninori.morimoto.gx@renesas.com>
#===============================
TOP=`readlink -f "$0" | xargs dirname| xargs dirname| xargs dirname`

test() {
        DIR=$1
        LIST=$2

        echo "test on ${DIR}"
        for list in ${LIST}
        do
                ${TOP}/script/sample-test-loopback.sh -f ${TOP}/renesas/${DIR}/${list}.wav -e ${list}
        done
}

${TOP}/renesas/sparrowhawk/S-amixer
test 44100 "19 28 37 46 50"
test 48000 "01 29 38 47 56"
