DIR	= 44100 48000
CH2	= 19 28 37 46 _5 _6 _7 _8 0_ 1_ 2_ 3_
CH6	= 012345 456789 543210 987654
CH8	= 01234567 23456789 76543210 98765432

all:
	@make -s ${DIR}
	@for dir in ${DIR}; do\
		echo "make dtmf for $${dir}"; \
		make -s D=$${dir} LISTS="${CH2}" CH=2 loop; \
		make -s D=$${dir} LISTS="${CH6}" CH=6 loop; \
		make -s D=$${dir} LISTS="${CH8}" CH=8 loop; \
	done;

${D}/${wav}.wav:
	@cd ${D};\
	../../simple_dtmf -r ${D} -c ${CH} -o ${wav}

loop:
	@for list in ${LISTS}; do\
		make wav=$${list} ${D}/$${list}.wav; \
	done;

${DIR}:
	@mkdir -p $@

clean:
	@rm -fr ${DIR}
