// SPDX-License-Identifier: GPLv2
//
// dtmf.c
//
// Copyright (c) 2022 Kuninori Morimoto <kuninori.morimoto.gx@renesas.com>
//
#include "param.h"

//=================================================
//
//
//		defines
//
//
//=================================================
struct tone_info {
	char num;
	int low;
	int hi;
};

#define TONE_123A	 697
#define TONE_456B	 770
#define TONE_789C	 852
#define TONE_x0xD	 941
#define TONE_147x	1209
#define TONE_2580	1336
#define TONE_369x	1477
#define TONE_ABCD	1633

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
const static struct tone_info tone_info[] = {
	{ '0', TONE_x0xD, TONE_2580 },
	{ '1', TONE_123A, TONE_147x },
	{ '2', TONE_123A, TONE_2580 },
	{ '3', TONE_123A, TONE_369x },
	{ '4', TONE_456B, TONE_147x },
	{ '5', TONE_456B, TONE_2580 },
	{ '6', TONE_456B, TONE_369x },
	{ '7', TONE_789C, TONE_147x },
	{ '8', TONE_789C, TONE_2580 },
	{ '9', TONE_789C, TONE_369x },
};
const static char unknown = '?';

#define PI2		(M_PI * 2)

//=======================================
//
// goertzel
//
//=======================================
static double goertzel(struct dev_param *param, int dtmf_fq)
{
	double omega	= PI2 * dtmf_fq / param->rate;
	double sine	= sin(omega);
	double cosine	= cos(omega);
	double coeff	= cosine * 2;
	double q0	= 0;
	double q1	= 0;
	double q2	= 0;

	for (int i = 0; i < param->length; i++) {
		q0 = coeff * q1 - q2 + param->buf[i];
		q2 = q1;
		q1 = q0;
	}

	double real = (q1 - q2 * cosine) / (param->length / 2.0);
	double imag = (q2 * sine)        / (param->length / 2.0);

	return sqrt(real * real + imag * imag);
}

//=======================================
//
// dtmf_analyze
//
//=======================================
#define DTMF_LEVELS_MAX	4
static void __dtmf_analyze(struct dev_param *param, const int *fq, int *ret)
{
	double level[DTMF_LEVELS_MAX];
	int i, idx = 0;

	//==========================
	// analyze dtmf
	//==========================
	for (i = 0; i < DTMF_LEVELS_MAX; i++) {
		level[i] = goertzel(param, fq[i]);

		printd("%8d : %10.5f\n", fq[i], level[i]);

		// FIXME
		if (level[idx] < level[i])
			idx = i;
	}

	if (level[idx] < 0.5) // FIXME
		return;

	//==========================
	// FIXME
	//
	// check the DTMF levels
	//==========================
	for (i = 0; i < DTMF_LEVELS_MAX; i++) {
		if (i == idx)
			continue;

		if ((level[i] * 20) > level[idx])
			return;
	}

	// success
	*ret = fq[idx];
}

char dtmf_analyze(struct dev_param *param)
{
	static const int dtmf_fq_low[DTMF_LEVELS_MAX] = { TONE_123A, TONE_456B, TONE_789C, TONE_x0xD };
	static const int dtmf_fq_hi[ DTMF_LEVELS_MAX] = { TONE_147x, TONE_2580, TONE_369x, TONE_ABCD };
	int low = -1;
	int hi  = -1;

	__dtmf_analyze(param, dtmf_fq_low, &low);
	__dtmf_analyze(param, dtmf_fq_hi,  &hi);

	if (low < 0 || hi < 0)
		goto err;

	for (int i = 0; i < ARRAY_SIZE(tone_info); i++) {
		if (tone_info[i].low == low &&
		    tone_info[i].hi  == hi) {
			return tone_info[i].num;
		}
	}
err:
	return unknown;
}

//=======================================
//
// dtmf_fill
//
//=======================================
int dtmf_fill(struct dev_param *param, char num)
{
	long volume		= 40000000 / param->rate;
	double phase_low	= 0;
	double phase_hi		= 0;
	double add_low;
	double add_hi;
	int tone_low = 0;
	int tone_hi  = 0;
	int i, v = 0;

	if (num == '_') {
		/* do nothing */
		memset(param->buf, 0, param->length * param->sample);
		return 0;
	}


	for (i = 0; i < ARRAY_SIZE(tone_info); i++) {
		if (tone_info[i].num == num) {
			tone_hi  = tone_info[i].hi;
			tone_low = tone_info[i].low;
			goto found;
		}
	}
	return -EINVAL;

found:
	add_low		= PI2 * tone_low / param->rate;
	add_hi		= PI2 * tone_hi  / param->rate;
	for (i = 0; i < param->length; i++) {

		if (add_low != 0) v += sin(phase_low) * volume;
		if (add_hi  != 0) v += sin(phase_hi)  * volume;

		param->buf[i] = v;

		phase_low += add_low;
		phase_hi  += add_hi;

		while (phase_low >= PI2) phase_low -= PI2;
		while (phase_hi  >= PI2) phase_hi  -= PI2;
	}

	return 0;
}
