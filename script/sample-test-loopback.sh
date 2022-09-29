#! /bin/bash
#===============================
#
# sample-test-loopback
#
# [usage]
#		sample-test-loopback.sh [ofie]
#
#		-o:	output devive (plughw:0,x)
#		-f:	output DTMF wav file
#		-i:	input devive  (plughw:0,x)
#		-e: 	expect result
#
# [if NG]
#
#	It will keep bad record wav file as NG-${DATE}-xxx.wav
#
#	> sample-test-loopback.sh 12.wav 12
#	play:12 - expect:12 - rec:?? : NG
#	> ls
#	NG-xxx-0-12.wav
#
# [sample]
#	       +---------+
#	2ch => |---> 2ch |=> -----+
#	       |         |        |
#	2ch <= |<--- 2ch |<= -----+
#	       +---------+
#
#		> simple_dtmf -r 44100 -c 2 12
#		12.wav
#
#		> sample-test-loopback.sh -f 12.wav -e 12
#
#	(123456) +---------+
#	6ch   => |-+-> 2ch | (12)
#		 | |       |
#		 | +-> 2ch | (34)
#		 | |       |
#		 | +-> 2ch |=> -----+ (56)
#		 |         |        |
#	2ch   <= |<--  2ch |<= -----+
#		 +---------+
#
#		> simple_dtmf -r 44100 -c 6 123456
#		123456.wav
#
#		> sample-test-loopback.sh -f 123456.wav -e 56
#
#	(123456) +---------+
#	6ch   => |-+-> 2ch |=> -----+   (12)
#		 | |       |        |
#		 | +-> 2ch |=> ------+  (34)
#		 | |       |        ||
#		 | +-> 2ch |=> -------+ (56)
#		 |         |        |||
#	2ch   <= |     2ch |<= -----+||
#		 |         |         ||
#	2ch   <= |     2ch |<= ------+|
#		 |         |          |
#	2ch   <= |     2ch |<= -------+
#		 +---------+
#
#		> simple_dtmf -r 44100 -c 6 123456
#		123456.wav
#
#		> sample-test-loopback.sh -f 123456.wav\
#				-i plughw:0,0 \
#				-i plughw:0,1 \
#				-i plughw:0,2 \
#				-e 12         \
#				-e 34         \
#				-e 56
#
#		    +------------+
#	(12) 2ch => | --+        |
#		    |   |        |
#	(34) 2ch => | --+--> 6ch |=> ----+ (123456)
#		    |   |        |       |
#	(56) 2ch -> | --+        |       |
#		    |            |       |
#	     6ch <= |<----   6ch |<= ----+
#		    +------------+
#
#		> simple_dtmf -r 44100 -c 2 123456
#		12.wav
#		34.wav
#		56.wav
#
#		> sample-test-loopback.sh
#				-o plughw:0,0 \
#				-o plughw:0,1 \
#				-o plughw:0,2 \
#				-f 12.wav     \
#				-f 34.wav     \
#				-f 56.wav     \
#				-e 123456
#
#		    +------------+
#	(1_) 2ch => | --+--> 2ch |=> ----+
#		    |   |(MIX)   |       |
#	(_9) 2ch => | --+        |       | (19)
#		    |            |       |
#		    |        2ch |<= ----+
#		    +------------+
#
#		> simple_dtmf -r 44100 -c 2 1__9
#		1_.wav
#		_9.wav
#
#		> sample-test-loopback.sh
#				-o plughw:0,0 \
#				-o plughw:0,1 \
#				-f 1_.wav     \
#				-f _9.wav     \
#				-e 19
#
# 2022/09/29 Kuninori Morimoto <kuninori.morimoto.gx@renesas.com>
#===============================
TOP=`readlink -f "$0" | xargs dirname | xargs dirname`

idx_o=0
idx_i=0
idx_f=0
idx_e=0
while getopts o:i:f:e: opt; do
	case "$opt" in
		o)
			dev_o[${idx_o}]="${OPTARG}"
			idx_o=`expr ${idx_o} + 1`
			;;
		i)
			dev_i[${idx_i}]="${OPTARG}"
			idx_i=`expr ${idx_i} + 1`
			;;
		f)
			files[${idx_f}]="${OPTARG}"
			idx_f=`expr ${idx_f} + 1`
			;;
		e)
			exp[${idx_e}]="${OPTARG}"
			idx_e=`expr ${idx_e} + 1`
			;;
	esac
done
shift $(expr $OPTIND - 1)

DATE=`date +%Y%m%d`
REC=NG-$$-

if [ ${idx_o} -eq 0 ]; then
	dev_o[${idx_o}]="default"
	idx_o=`expr ${idx_o} + 1`
fi
if [ ${idx_i} -eq 0 ]; then
	dev_i[${idx_i}]="default"
	idx_i=`expr ${idx_i} + 1`
fi

[ ${idx_o} -ne ${idx_f} ] && echo "-o vs -f mismatch" && exit
[ ${idx_i} -ne ${idx_e} ] && echo "-o vs -f mismatch" && exit
[ ! -f ${file[0]} ] && echo "no such wav file" && exit

INFO=`${TOP}/simple_dtmf -l ${files[0]}`

RATE=`echo "${INFO}" | grep rate | cut -d ':' -f 2`
BIT=` echo "${INFO}" | grep bit  | cut -d ':' -f 2`

sl=""
echo -n "play:"
for ((idx=0; idx<${idx_o}; idx++))
do
	f=`basename ${files[$idx]} | cut -d "." -f 1`
	echo -n "${sl}${f}"
	sl="/"
done

for ((idx=0; idx<${idx_o}; idx++))
do
	aplay   -q -D ${dev_o[$idx]} ${files[$idx]} &
done

sl=""
echo -n " - expect:"
for ((idx=0; idx<${idx_i}; idx++))
do
	echo -n "${sl}${exp[${idx}]}"
	sl="/"
done

for ((idx=0; idx<${idx_i}; idx++))
do
	CHAN=`echo "${#exp[${idx}]}"`
	arecord -q -D ${dev_i[${idx}]} -d 1 -c ${CHAN} -t wav -r ${RATE} -f S${BIT} ${REC}${idx}-${exp[${idx}]} &
done

wait

RET="OK"
sl=""
echo -n " - rec:"
for ((idx=0; idx<${idx_i}; idx++))
do
	NUM=`${TOP}/simple_dtmf -i ${REC}${idx}-${exp[${idx}]}`
	echo -n "${sl}${NUM}"
	sl="/"
	[ x"${exp[${idx}]}" != x"${NUM}" ] && RET="NG"
done
echo " : ${RET}"

if [ "${RET}" = "OK" ]; then
	rm ${REC}*
fi
