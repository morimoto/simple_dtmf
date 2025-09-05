// SPDX-License-Identifier: GPLv2
//
// wav.c
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
#define ID_SIZE 4
struct wav_base {
	char riff[ID_SIZE];		// 4: "RIFF"
	u32  rsize;			// 4: file size - 8
	char ID[ID_SIZE];		// 4: "WAVE"
	char ckID[ID_SIZE];		// 4: "fmt "
	u32  cksize;			// 4: 16
	u16  wFormatTag;		// 2: Format code
	u16  nChannels;			// 2: Channels
	u32  nSamplesPerSec;		// 4: Sampling rate
	u32  nAvgBytesPerSec;		// 4: Data rate
	u16  nBlockAlign;		// 2: Data block size (bytes)
	u16  wBitsPerSample;		// 2: Bits per sample
};

struct wav_exp {
	u16  cbSize;			// 2: 22
	u16  wValidBitsPerSample;	// 2: Valid bit 24/32
	u32  dwChannelMask;		// 4: speaker position
	u32  GUID_1;			// 4: GUID: PCM: 0x00000001
	u16  GUID_2;			// 2: GUID: PCM: 0x0000
	u16  GUID_3;			// 2: GUID: PCM: 0x0010
	u8   GUID_4[8];			// 8: GUID: PCM: {0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71}
};

struct wav_fact {
	char SubChunck[ID_SIZE];	// 4: "fact"
	u32 cksize;			// 4: chunk size
	u32 dwSampleLength;		// 4: sample size
};

struct wav_data {
	char SubChunck[ID_SIZE];	// 4: "data"
	u32  SubChunckSize;		// 4: file size - 44
};

const static char *str_riff	= "RIFF";
const static char *str_wave	= "WAVE";
const static char *str_fmt	= "fmt ";
const static char *str_fact	= "fact";
const static char *str_data	= "data";

//=======================================
//
// name_fill
//
//=======================================
#define name_fill(pos, ans)   memcpy(pos, ans, ID_SIZE)

//=======================================
//
// name_check
//
//=======================================
int name_check(const char *pos, const char *ans)
{
	int ret = strncmp(pos, ans, ID_SIZE);

	if (ret != 0)
		ret = -EINVAL;

	return ret;
}

//=======================================
//
// wav_write
//
//=======================================
#define FILE_NAME_SIZE 128
int wav_write_header(struct dev_param *param)
{
	struct wav_base wav;
	struct wav_data data;
	FILE *fp;
	int null = 0;
	int ret = -ENOENT;
	int blockalign = param->word * param->chan;

	//==========================
	// open the file
	//==========================
	if (!(fp = fopen(param->filename, "w")))
		goto no_open;

	//==========================
	// fill data
	//==========================
	data.SubChunckSize = blockalign * param->length;
	name_fill(data.SubChunck, str_data);

	//==========================
	// fill the wav file header
	//==========================
	name_fill(wav.riff,		str_riff);
	name_fill(wav.ID,		str_wave);
	name_fill(wav.ckID,		str_fmt);
	wav.cksize		= 16;
	wav.wFormatTag		= 0x0001;
	wav.nChannels		= param->chan;
	wav.nSamplesPerSec	= param->rate;
	wav.wBitsPerSample	= param->sample;
	wav.nBlockAlign		= blockalign;
	wav.nAvgBytesPerSec	= wav.nBlockAlign * param->rate;
	wav.rsize		= data.SubChunckSize +
				  sizeof(struct wav_base) +
				  sizeof(struct wav_data) - 8;

	//==========================
	// write wav base
	//==========================
	ret = -EINVAL;
	if (!fwrite(&wav, sizeof(struct wav_base), 1, fp))
		goto err;

	//==========================
	// write wav data
	//==========================
	ret = -EINVAL;
	if (!fwrite(&data, sizeof(struct wav_data), 1, fp))
		goto err;

	//==========================
	// fill null data
	//==========================
	for (int i = 0; i < data.SubChunckSize; i++)
		if (!fwrite(&null, 1, 1, fp))
			goto err;

	// success
	ret = 0;
err:
	fclose(fp);
no_open:
	return ret;
}

int wav_write_data(struct dev_param *param, int chan)
{
	FILE *fp;
	int ret = -ENOENT;
	int offset;

	//==========================
	// open the file
	//==========================
	if (!(fp = fopen(param->filename, "r+")))
		goto no_open;

	//==========================
	// skip "header part" and
	// 1st "non target channel" (offset)
	//==========================
	offset = chan * param->word;
	if (fseek(fp, sizeof(struct wav_base) +
		      sizeof(struct wav_data) + offset, SEEK_SET))
		goto err;

	//==========================
	// write data
	//==========================
	ret = -EIO;
	offset = (param->chan - 1) * param->word;

	for (int i = 0; i < param->length; i++) {
		if (!fwrite(&param->buf[i], param->word, 1, fp))
			goto err;

		if (offset > 0 && fseek(fp, offset, SEEK_CUR))
			goto err;
	}

	// success
	ret = 0;
err:
	fclose(fp);
no_open:
	return ret;
}

//=======================================
//
// wav_read_header
//
//=======================================
int wav_read_header(struct dev_param *param)
{
	struct wav_base wav;
	struct wav_exp  exp;
	struct wav_data data;
	FILE *fp;
	int ret = -ENOENT;
	char subchunck[ID_SIZE];

	//==========================
	// file open
	//==========================
	if (!(fp = fopen(param->filename, "r")))
		goto no_open;

	//==========================
	// read header part
	//==========================
	ret = -EIO;
	if (!fread(&wav, sizeof(struct wav_base), 1, fp))
		goto err;

	//==========================
	// name part check
	//==========================
	ret = name_check(wav.riff, str_riff);
	if (ret)
		goto err;

	ret = name_check(wav.ID, str_wave);
	if (ret)
		goto err;

	ret = name_check(wav.ckID, str_fmt);
	if (ret)
		goto err;

	// It supports 16/24/32 bit only
	switch (wav.wBitsPerSample) {
	case 16:
	case 24:
	case 32:
		break;
	default:
		goto err;
	}

	//==========================
	// expectation part check
	//==========================
	/* WAVE_FORMAT_PCM */
	if ((wav.cksize == 16) && (wav.wFormatTag == 0x0001))
		goto data_part;
	/* WAVE_FORMAT_EXTENSIBLE */
	if ((wav.cksize == 40) && (wav.wFormatTag == 0xFFFE))
		goto exp_part;

	ret = -EINVAL;
	goto err;

exp_part:
	//==========================
	// exp part check
	//==========================
	if (!fread(&exp, sizeof(struct wav_exp), 1, fp))
		goto err;

	ret = -EINVAL;
	if (exp.cbSize != 22)
		goto err;

	if (exp.GUID_1	  != 0x00000001	||
	    exp.GUID_2	  != 0x0000	||
	    exp.GUID_3	  != 0x0010	||
	    exp.GUID_4[0] != 0x80	||
	    exp.GUID_4[1] != 0x00	||
	    exp.GUID_4[2] != 0x00	||
	    exp.GUID_4[3] != 0xAA	||
	    exp.GUID_4[4] != 0x00	||
	    exp.GUID_4[5] != 0x38	||
	    exp.GUID_4[6] != 0x9B	||
	    exp.GUID_4[7] != 0x71)
		goto err;

	//==========================
	// fact part check
	//==========================
	if (!fread(subchunck, ID_SIZE, 1, fp))
		goto err;

	ret = name_check(subchunck, str_fact);
	if (ret)
		goto err;

	fseek(fp, sizeof(struct wav_fact) - ID_SIZE, SEEK_CUR);

data_part:
	//==========================
	// data part check
	//==========================
	if (!fread(&data, sizeof(struct wav_data), 1, fp))
		goto err;

	ret = name_check(data.SubChunck, str_data);
	if (ret)
		goto err;

	//==========================
	// set param->xxx
	//==========================
	param->chan	= wav.nChannels;
	param->rate	= wav.nSamplesPerSec;
	param->word	= wav.nBlockAlign / wav.nChannels;
	param->sample	= wav.wBitsPerSample;
	param->length	= data.SubChunckSize / wav.nBlockAlign;

	// success
	ret = 0;
err:
	fclose(fp);
no_open:
	return ret;
}

//=======================================
//
// wav_read_data
//
//=======================================
int wav_read_data(struct dev_param *param, int chan)
{
	FILE *fp;
	struct wav_base wav;
	struct wav_exp  exp;
	struct wav_fact fact;
	struct wav_data data;
	int ret = -ENOMEM;
	int offset;

	//==========================
	// file open
	//==========================
	if (!(fp = fopen(param->filename, "r")))
		goto no_open;

	//==========================
	// skip "header part" and
	// 1st "non target channel" (offset)
	//==========================
	if (!fread(&wav, sizeof(struct wav_base), 1, fp))
		goto err;

	if ((wav.cksize == 16) && (wav.wFormatTag == 0x0001))
		goto data_part;
	/* WAVE_FORMAT_EXTENSIBLE */
	if ((wav.cksize == 40) && (wav.wFormatTag == 0xFFFE))
		goto exp_part;

	ret = -EINVAL;
	goto err;

exp_part:
	if (!fread(&exp, sizeof(struct wav_exp), 1, fp))
		goto err;

	if (!fread(&fact, sizeof(struct wav_fact), 1, fp))
		goto err;
data_part:
	if (!fread(&data, sizeof(struct wav_data), 1, fp))
		goto err;

	offset = chan * param->word;
	if (fseek(fp, offset, SEEK_CUR))
		goto err;

	//==========================
	// read data
	//==========================
	ret = -EINVAL;
	offset = (param->chan - 1) * param->word;

	for (int i = 0; i < param->length; i++) {
		int minus = 0;

		if (!fread(&param->buf[i], param->word, 1, fp))
			goto err;

		/*
		 * FIXME
		 *
		 * Because we are using 4byte buffer for 16/24/32bit data,
		 * we need to expand minus for 16/24bit case.
		 */
		switch (param->sample) {
		case 16:
			if (param->buf[i] & 0x8000)
				minus = 1;

			param->buf[i] &= 0xffff;
			if (minus)
				param->buf[i] |= 0xffff0000;

			break;
		case 24:
			if (param->buf[i] & 0x800000)
				minus = 1;

			param->buf[i] &= 0xffffff;
			if (minus)
				param->buf[i] |= 0xff000000;
			break;
		}

		if (offset > 0 && fseek(fp, offset, SEEK_CUR))
			goto err;
	}

	// success
	ret = 0;
err:
	fclose(fp);
no_open:
	return ret;
}
