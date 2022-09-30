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
struct wav {
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
	char SubChunck[ID_SIZE];	// 4: "data"
	u32  SubChunckSize;		// 4: file size - 44
};

const static char *riff	= "RIFF";
const static char *wave = "WAVE";
const static char *fmt	= "fmt ";
const static char *data	= "data";

//=======================================
//
// name_check
// name_fill
//
//=======================================
#define name_check(pos, ans) strncmp(pos, ans, ID_SIZE);
#define name_fill(pos, ans)   memcpy(pos, ans, ID_SIZE)

//=======================================
//
// wav_write
//
//=======================================
#define FILE_NAME_SIZE 128
int wav_write_header(struct dev_param *param)
{
	struct wav wav;
	FILE *fp;
	int null = 0;
	int ret = -ENOENT;

	//==========================
	// open the file
	//==========================
	if (!(fp = fopen(param->filename, "w")))
		goto no_open;

	//==========================
	// fill the wav file header
	//==========================
	name_fill(wav.riff,		riff);
	name_fill(wav.ID,		wave);
	name_fill(wav.ckID,		fmt);
	name_fill(wav.SubChunck,	data);
	wav.cksize		= 16;
	wav.wFormatTag		= 0x0001;
	wav.nChannels		= param->chan;
	wav.nSamplesPerSec	= param->rate;
	wav.wBitsPerSample	= param->sample * 8;
	wav.nBlockAlign		= param->sample * param->chan;
	wav.nAvgBytesPerSec	= wav.nBlockAlign * param->rate;
	wav.SubChunckSize	= wav.nBlockAlign * param->length;
	wav.rsize		= wav.SubChunckSize + sizeof(struct wav) - 8;

	//==========================
	// write header
	//==========================
	ret = -EINVAL;
	if (!fwrite(&wav, sizeof(wav), 1, fp))
		goto err;

	//==========================
	// fill null data
	//==========================
	for (int i = 0; i < wav.SubChunckSize; i++)
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
	offset = chan * param->sample;
	if (fseek(fp, sizeof(struct wav) + offset, SEEK_SET))
		goto err;

	//==========================
	// write data
	//==========================
	ret = -EIO;
	offset = (param->chan - 1) * param->sample;

	for (int i = 0; i < param->length; i++) {
		if (!fwrite(&param->buf[i], param->sample, 1, fp))
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
	struct wav wav;
	FILE *fp;
	int rate;
	int chan;
	int sample;
	int ret = -ENOENT;

	//==========================
	// file open
	//==========================
	if (!(fp = fopen(param->filename, "r")))
		goto no_open;

	//==========================
	// read header part
	//==========================
	ret = -EIO;
	if (!fread(&wav, sizeof(struct wav), 1, fp))
		goto err;

	chan	= wav.nChannels;
	rate	= wav.nSamplesPerSec;
	sample	= wav.wBitsPerSample / 8;

	//==========================
	// name part check
	//==========================
	ret = name_check(wav.riff, riff);
	if (ret)
		goto err;

	ret = name_check(wav.ID, wave);
	if (ret)
		goto err;

	ret = name_check(wav.ckID, fmt);
	if (ret)
		goto err;

	ret = name_check(wav.SubChunck, data);
	if (ret)
		goto err;

	//==========================
	// expectation part check
	//==========================
	ret = -EINVAL;
	if (wav.cksize != 16)
		goto err;
	if (wav.wFormatTag != 0x0001) /* WAVE_FORMAT_PCM */
		goto err;
	if (sample != 2) /* 16bit only for now */
		goto err;
	if ((chan * sample) != wav.nBlockAlign)
		goto err;
	if ((wav.nBlockAlign * rate) != wav.nAvgBytesPerSec)
		goto err;

	//==========================
	// set param->xxx
	//==========================
	param->chan	= wav.nChannels;
	param->rate	= wav.nSamplesPerSec;
	param->sample	= wav.wBitsPerSample / 8;
	param->length	= wav.SubChunckSize / param->chan / param->sample;

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
	offset = chan * param->sample;
	if (fseek(fp, sizeof(struct wav) + offset, SEEK_SET))
		goto err;

	//==========================
	// read data
	//==========================
	ret = -EINVAL;
	offset = (param->chan - 1) * param->sample;

	for (int i = 0; i < param->length; i++) {
		if (!fread(&param->buf[i], param->sample, 1, fp))
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
