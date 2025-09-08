/* SPDX-License-Identifier: GPLv2
 *
 * param.h
 *
 * Copyright (c) 2022 Kuninori Morimoto <kuninori.morimoto.gx@renesas.com>
 */
#ifndef __PARAM_H
#define __PARAM_H

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include "common.h"

#define FLAG_TYPE_MASK	(0xF << 0)
#define FLAG_TYPE_OUT	(0x1 << 0)
#define FLAG_TYPE_IN	(0x2 << 0)
#define FLAG_TYPE_INFO	(0x3 << 0)

#define FLAG_VERBOSE	(1 << 31)

struct dev_param {
	/*
	 * <---- chan ---->
	 * <-word->
	 * [sample][sample] ^
	 * [sample][sample] |
	 * ...              length
	 * [sample][sample] |
	 * [sample][sample] v
	 */
	int rate;
	int chan;
	int word;	/* (byte) word size */
	int sample;	/* (bit)  16/24/32 */
	int length;

	u32 flag;

	s32 *buf;	/* always use s32 */
	char *nums;
	char *filename;
};

char dtmf_analyze(s32 *buf, int length, int rate);
int dtmf_fill(s32 *buf, int length, int rate, int sample, char num);

int wav_write_header(struct dev_param *param);
int wav_write_data(struct dev_param *param, int chan);

int wav_read_header(struct dev_param *param);
int wav_read_data(struct dev_param *param, int chan);

#endif /* __PARAM_H */
