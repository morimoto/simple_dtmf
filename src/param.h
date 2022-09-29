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
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <math.h>

#define s16	signed short

#define u16	unsigned short
#define u32	unsigned int

#define FLAG_TYPE_MASK	(0xF << 0)
#define FLAG_TYPE_OUT	(0x1 << 0)
#define FLAG_TYPE_IN	(0x2 << 0)

#define FLAG_VERBOSE	(1 << 31)

struct dev_param {
	/*
	 * <---- chan ---->
	 * [sample][sample] ^
	 * [sample][sample] |
	 * ...              length
	 * [sample][sample] |
	 * [sample][sample] v
	 */
	int rate;
	int chan;
	int sample;	/* 16 bit for now */
	int length;

	u32 flag;

	s16 *buf;
	char *nums;
	char *filename;
};

#define printv(param, fmt...) if (param->flag & FLAG_VERBOSE) printf(fmt)
#ifdef _DEBUG
#define printd(fmt...)	printf(" * " fmt)
#else
#define printd(fmt...)
#endif

char dtmf_analyze(struct dev_param *param);
int  dtmf_fill(struct dev_param *param, char num);

int wav_write_header(struct dev_param *param);
int wav_write_data(struct dev_param *param, int chan);

int wav_read_header(struct dev_param *param);
int wav_read_data(struct dev_param *param, int chan);

#endif /* __PARAM_H */
