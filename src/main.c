// SPDX-License-Identifier: GPLv2
//
// main.c
//
// Copyright (c) 2022 Kuninori Morimoto <kuninori.morimoto.gx@renesas.com>
//
#include "param.h"

//=======================================
//
// usage
// parse_options
//
//=======================================
static void usage(void)
{
	printf("\n\n"
		"simple_dtmf [oircv]\n\n"
		"	-o : output dir\n"
		"	-i : input file\n"
		"	-r : rate\n"
		"	-c : chan\n"
		"	-v : verbose print\n\n"
		);
}

static int parse_options(int argc, char **argv, struct dev_param *param)
{
	int opt;

	// default settings
	param->is_out	= 1;		// out
	param->chan	= 2;		// 2ch
	param->rate	= 8000;
	param->sample	= sizeof(s16);	// update me
	param->path	= NULL;

	//==========================
	// parse
	//==========================
	while ((opt = getopt(argc, argv, "o:i:r:c:v")) != -1) {
		switch (opt) {
		case 'o':
			param->is_out	= 1;
			param->path	= optarg;
			break;
		case 'i':
			param->is_out	= 0;
			param->path	= optarg;
			break;
		case 'r':
			sscanf(optarg, "%d", &param->rate);
			break;
		case 'c':
			sscanf(optarg, "%d", &param->chan);
			break;
		case 'v':
			param->verbose = 1;
			break;
		default:
			goto err;
		}
	}
	argc -= optind;
	argv += optind;

	//==========================
	// check params
	//==========================
	if (!param->path)
		goto err;
	if (param->chan % 2)
		goto err;
	switch (param->rate) {
	case   8000:
	case  11025:
	case  16000:
	case  22050:
	case  32000:
	case  44100:
	case  48000:
	case  64000:
	case  88200:
	case  96000:
	case 176400:
	case 192000:
		break;
	default:
		goto err;
	}

	return 0;
err:
	usage();
	return -EINVAL;
}

//=======================================
//
// buf_alloc
// buf_free
//
//=======================================
static int buf_alloc(struct dev_param *param)
{
	s16 *buf;

	buf = calloc(param->length, param->sample);
	if (!buf)
		return -ENOMEM;

	param->buf	= buf;

	return 0;
}

static void buf_free(struct dev_param *param)
{
	if (param->buf)
		free(param->buf);

	param->buf = NULL;
}

//=======================================
//
// dtmf_wav_write
//
//=======================================
static int dtmf_wav_write(struct dev_param *param)
{
	int ret;

	//==========================
	// alloc buf for 1sec
	//==========================
	param->length = param->rate;

	ret = buf_alloc(param);
	if (ret < 0)
		goto err;

	printv(param, "chan    : %d\n", param->chan);
	printv(param, "rate    : %d\n", param->rate);
	printv(param, "bit     : %d\n", param->sample * 8);
	printv(param, "length  : %d\n", param->length);

	//==========================
	// write 0.wav - 9.wav
	//==========================
	for (char num = '0'; num <= '9'; num++) {
		ret = dtmf_fill(param, num);
		if (ret < 0)
			goto free;

		ret = wav_write(param, num);
		if (ret < 0)
			goto free;
	}

	// sucess
	ret = 0;
free:
	buf_free(param);
err:
	return ret;
}

//=======================================
//
// dtmf_wav_analyze
//
//=======================================
static int dtmf_wav_analyze(struct dev_param *param)
{
	int ret;

	//==========================
	// read wav header, and fill params
	//==========================
	ret = wav_read_header(param);
	if (ret < 0)
		goto err;

	printv(param, "chan    : %d\n", param->chan);
	printv(param, "rate    : %d\n", param->rate);
	printv(param, "bit     : %d\n", param->sample * 8);
	printv(param, "length  : %d\n", param->length);

	// alloc buf
	ret = buf_alloc(param);
	if (ret < 0)
		goto err;

	//==========================
	// analyze DTMF
	//==========================
	for (int i = 0; i < param->chan; i++) {
		ret = wav_read_data(param, i);
		if (ret < 0)
			goto free;

		printf("%c", dtmf_analyze(param));
	}

	// success
	ret = 0;
free:
	printf("\n");
	buf_free(param);
err:
	return ret;
}

//=======================================
//
// main
//
//=======================================
int main(int argc, char **argv)
{
	struct dev_param param;
	int ret;

	//==========================
	// parse options
	//==========================
	memset(&param, 0, sizeof(param));

	ret = parse_options(argc, argv, &param);
	if (ret < 0)
		goto err;

	//==========================
	// write or analyze DTMF
	//==========================
	if (param.is_out)
		ret = dtmf_wav_write(&param);
	else
		ret = dtmf_wav_analyze(&param);
err:
	if (ret < 0)
		printf("%s\n", strerror(ret * -1));

	return ret;
}
