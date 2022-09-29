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
		"simple_dtmf [orcv]\n\n"
		"	-o : create nums (0 - 9)\n"
		"	-r : rate\n"
		"	-c : chan\n"
		"	-v : verbose print\n\n"
		"simple_dtmf [iv]\n\n"
		"	-i : input file\n"
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
	param->nums	= NULL;
	param->filename	= NULL;

	//==========================
	// parse
	//==========================
	while ((opt = getopt(argc, argv, "o:i:r:c:vh")) != -1) {
		switch (opt) {
		case 'o':
			param->is_out	= 1;
			param->nums	= optarg;
			break;
		case 'i':
			param->is_out	= 0;
			param->filename	= optarg;
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
		case 'h':
			usage();
			exit(0);
		default:
			goto err;
		}
	}
	argc -= optind;
	argv += optind;

	//==========================
	// check params
	//==========================
	if (!param->is_out && !param->filename)
		goto err;
	if (param->chan % 2)
		goto err;

	if (param->is_out) {
		int len = strlen(param->nums);
		if (!param->nums)
			goto err;
		if (len < param->chan)
			goto err;
		for (int i = 0; i < len; i++)
			if (param->nums[i] < '0' || param->nums[i] > '9')
				goto err;
	}

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
static int __dtmf_wav_write(struct dev_param *param, char *filename)
{
	char num;
	int ret = -EINVAL;

	// filename is used at wav.c
	param->filename = filename;

	//==========================
	// write header
	//==========================
	ret = wav_write_header(param);
	if (ret < 0)
		goto err;

	//==========================
	// write data for each channels
	//
	// num came from filename
	//
	// filename : 023.wav
	//            ^^^
	//==========================
	for (int chan = 0; chan < param->chan; chan++) {
		num = param->filename[chan];

		ret = dtmf_fill(param, num);
		if (ret < 0)
			goto err;

		ret = wav_write_data(param, chan);
		if (ret < 0)
			goto err;
	}

	// success
	ret = 0;
err:
	return ret;
}

#define FILE_NAME_SIZE 128
static int dtmf_wav_write(struct dev_param *param)
{
	char filename[FILE_NAME_SIZE];
	int ret = -EINVAL;
	int i, len;

	if (param->chan + 10 > FILE_NAME_SIZE)
		goto err;

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
	// create nums
	//
	// ex) simple_dtmf -c 2 -o 1234567
	// "12.wav", "34.wav", "56.wav"
	//==========================
	len = (strlen(param->nums) / param->chan) * param->chan;

	for (i = 0; i < len; i += param->chan) {
		memcpy(filename, param->nums + i, param->chan);
		sprintf(filename + param->chan, ".wav");

		printf("%s\n", filename);

		ret = __dtmf_wav_write(param, filename);
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
