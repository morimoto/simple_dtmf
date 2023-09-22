/* SPDX-License-Identifier: GPLv2
 *
 * common.h
 *
 * Copyright (c) 2023 Kuninori Morimoto <kuninori.morimoto.gx@renesas.com>
 */
#ifndef __COMMON_H
#define __COMMON_H

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <errno.h>

#define s16	signed short

#define u16	unsigned short
#define u32	unsigned int

const static char unknown = '?';

#define printv(param, fmt...) if (param->flag & FLAG_VERBOSE) printf(fmt)
#ifdef _DEBUG
#define printd(fmt...)	printf(" * " fmt)
#else
#define printd(fmt...)
#endif

#endif /* __COMMON_H */