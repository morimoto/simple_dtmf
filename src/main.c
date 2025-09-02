// SPDX-License-Identifier: GPLv2
//
// main.c
//
// Copyright (c) 2022 Kuninori Morimoto <kuninori.morimoto.gx@renesas.com>
//
#include "param.h"

#define VERSION		"1.1.0"
#define MAX_CHAN	16

#define is_versbose(param)	(param->flag & FLAG_VERBOSE)

//=======================================
//
// usage
// parse_options
//
//=======================================
static void usage(void)
{
	printf( "simple_dtmf v%s\n\n"
		"(output) simple_dtmf -o [rcv]\n\n"
		"	-o : create nums (0123456789 or _)\n"
		"	-r : rate (default: 8000)\n"
		"	-c : chan (default: 2)\n"
		"	-v : verbose print\n\n"
		"(input) simple_dtmf [v] -i file.wav\n\n"
		"	-i : input file\n"
		"	-v : verbose print\n\n"
		"(info)  simple_dtmf -l file.wav\n\n"
		"note:\n"
		"	max %d channels\n",
		VERSION, MAX_CHAN
		);
}

static int parse_options(int argc, char **argv, struct dev_param *param)
{
	int opt;

	// default settings
	param->chan	= 2;		// 2ch
	param->rate	= 8000;
	param->sample	= sizeof(s16);	// update me
	param->nums	= NULL;
	param->filename	= NULL;

	//==========================
	// parse
	//==========================
	while ((opt = getopt(argc, argv, "o:i:l:r:c:vh")) != -1) {
		switch (opt) {
		case 'o':
			param->flag	|= FLAG_TYPE_OUT;
			param->nums	= optarg;
			break;
		case 'i':
			param->flag	|= FLAG_TYPE_IN;
			param->filename	= optarg;
			break;
		case 'l':
			param->flag	|= FLAG_TYPE_INFO;
			param->filename	= optarg;
			break;
		case 'r':
			sscanf(optarg, "%d", &param->rate);
			break;
		case 'c':
			sscanf(optarg, "%d", &param->chan);
			break;
		case 'v':
			param->flag |= FLAG_VERBOSE;
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
	switch (param->flag & FLAG_TYPE_MASK) {
		int len;
	case FLAG_TYPE_OUT:
		len = strlen(param->nums);

		if (len < param->chan)
			goto err;
		for (int i = 0; i < len; i++)
			if ((param->nums[i] != '_') &&
			    (param->nums[i] < '0' || param->nums[i] > '9'))
				goto err;
		break;
	case FLAG_TYPE_IN:
	case FLAG_TYPE_INFO:
		break;
	default:
		goto err;
	}
	if (param->chan % 2)
		goto err;
	if (param->chan > MAX_CHAN)
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

		ret = dtmf_fill(param->buf,
				param->length,
				param->rate,
				param->sample, num);
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

	if (is_versbose(param)) {
		printf("chan    : %d\n", param->chan);
		printf("rate    : %d\n", param->rate);
		printf("bit     : %d\n", param->sample * 8);
		printf("length  : %d\n", param->length);
	}

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
#define DEGREE	10	// 10%
static int dtmf_wav_analyze(struct dev_param *param)
{
	int ret;
	int challenge;
	char *result;
	char prev[MAX_CHAN];
	int i, j;
	int comma = 0;
	int late_j = 0;
	int width;

	//==========================
	// read wav header, and fill params
	//==========================
	ret = wav_read_header(param);
	if (ret < 0)
		goto err;

	if (is_versbose(param)) {
		printf("chan    : %d\n", param->chan);
		printf("rate    : %d\n", param->rate);
		printf("bit     : %d\n", param->sample * 8);
		printf("length  : %d\n", param->length);
	}
	// alloc buf
	ret = buf_alloc(param);
	if (ret < 0)
		goto err_buff;

	//==========================
	// analyze DTMF
	//
	// Basic way is below, but it can't handle if data has too big noise,
	// and/or if data has multi dtmfs.
	// To reduse noise effect, and/or handle multi dtmfs, it will try to analyze by small pieces
	//
	// for (int i = 0; i < param->chan; i++) {
	//	ret = wav_read_data(param, i);
	//	...
	//
	//	printf("%c", dtmf_analyze(param->buf,
	//				  param->length,
	//				  param->rate));
	// }
	//==========================

	// alloc for result
	//
	// wav_read_data() will read 1ch data each, and dtmf_analyze() will analyze specified data.
	// We want to analyze 1ch data in 10% rate (= DEGREE) increments (= challenge),
	// because it might include noise. Thus we need to keep the results.
	//
	// ex)
	//	       <------ length ----- ... -->
	//	       <- rate -><- rate -> ...
	//	       <><><><><><><><><><>  (challenge)
	//	buf = [xxxxxxxxxxxxxxxxxxxx ... xx]
	//
	// 1 rate   has DEGREE% challenges (= param->rate / 100 * DEGREE)
	// Total challenges = param->length / (param->rate / 100 * DEGREE)
	//		    = param->length * 100 / param->rate / DEGREE
	challenge = param->length * 100 / param->rate / DEGREE;

	// 1ch needs "challenge" results, and we has param->chan.
	//
	// "result" will keep each channels result
	//
	//	     <-- 1ch --><-- 2ch -->...
	// result = [xxxxxxxxxxxyyyyyyyyyyy...]
	//
	result = calloc(param->chan, challenge);
	if (!result)
		goto err_buff;

	// width = 1 challenge size
	width = param->rate / 100 * DEGREE;

	// analyze for each channels.
	for (i = 0; i < param->chan; i++) {

		// read each 1ch
		ret = wav_read_data(param, i);
		if (ret < 0)
			goto free;

		// analyze par 1 width
		for (j = 0; j < challenge; j++)
			result[challenge * i + j] = dtmf_analyze(param->buf + (width * j),
								 width,
								 param->rate);
	}

	if (is_versbose(param)) {
		for (i = 0; i < param->chan; i++) {
			for (j = 0; j < challenge; j++)
				printf("%c", result[challenge * i + j]);
			printf("\n");
		}
	}

	//
	// In reality, data might have some dfmfs, and some parts might be noise
	//
	// <> : challenge
	// vv : noise
	//
	//        vvvv               vvv          vvvv
	// buf = [___xxxxxx____xxxxxxxx_____xxxxxxxx__]
	//        <><><><><><><><><><><><><><><><><><>
	//
	// We keep prev "challenge" to judge either it was noise or not.
	//
	memset(&prev, 0, sizeof(prev));

	//
	// handle each challenge and print it
	//
	// ex)
	//
	// result = [?? 11 11 22 22 ?? 33 33 3? ?9 ?? ?4 44 44 ?? 55 55 55 ??]
	//
	// print = 11 22 33 ?9 44 55
	//
	for (j = 0; j < challenge; j++) {
		int same = 0;
		int uk = 0;

		// check prev challenge, unknown
		for (i = 0; i < param->chan; i++) {
			if (result[challenge * i + j] == prev[i])
				same++;
			if (result[challenge * i + j] == unknown)
				uk++;
		}

		//    skip if all data were same as prev data. (.. 33 33 ..)
		// or skip if all data were unknown (noise)    (.. ?? ..)
		if (same == param->chan ||
		    uk   == param->chan)
			continue;

		// ex)
		//
		// result ~= [11 22 33 3? ?9 ?4 44 55]
		//

		// [.. 3? .. ?9 .. ?4 ..]
		if (uk) {
			//        *
			// [.. 33 3? ..]
			// it is noise
			if (same > 0)
				continue;

			//      *
			// [.. ?4 44 ..]
			// it is also noise, but we can't judge it now.
			// late judge
			late_j = 1;
			continue;
		}

		// We want like below
		//	11,22,33,?9,44,55
		if (comma)
			printf(",");

		if (late_j) {
			int judge = 0;

			//        *
			// [.. ?4 44 ..]
			// [.. ?9 ?4 ..]
			for (i = 0; i < param->chan; i++)
				if (result[challenge * i + j] != unknown &&
				    result[challenge * i + j] == prev[i])
					judge++;

			//        *
			// [.. ?4 44 ..]
			// "?4" is noise

			//        *
			// [.. ?9 ?4 ..]
			// "?9" is unknown data
			if (judge)
				for (i = 0; i < param->chan; i++)
					printf("%c", prev[i]);
		}

		// print current data, and keep prev[]
		for (i = 0; i < param->chan; i++) {
			comma = 1;
			late_j = 0;
			printf("%c", result[challenge * i + j]);
			prev[i] = result[challenge * i + j];
		}
	}

	// all "?" case
	if (!comma)
		for (i = 0; i < param->chan; i++)
			printf("%c", unknown);

	// success
	ret = 0;
free:
	printf("\n");
	free(result);
err_buff:
	buf_free(param);
err:
	return ret;
}

//=======================================
//
// dtmf_wav_info
//
//=======================================
static int dtmf_wav_info(struct dev_param *param)
{
	int ret;

	//==========================
	// read wav header, and fill params
	//==========================
	ret = wav_read_header(param);
	if (ret < 0)
		goto err;

	printf("chan:%d\n", param->chan);
	printf("rate:%d\n", param->rate);
	printf("bit :%d\n", param->sample * 8);

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
	switch (param.flag & FLAG_TYPE_MASK) {
	case FLAG_TYPE_OUT:
		ret = dtmf_wav_write(&param);
		break;
	case FLAG_TYPE_IN:
		ret = dtmf_wav_analyze(&param);
		break;
	case FLAG_TYPE_INFO:
		ret = dtmf_wav_info(&param);
		break;
	default:
		ret = -EINVAL;
		break;
	}

err:
	if (ret < 0)
		printf("%s\n", strerror(ret * -1));

	return ret;
}
