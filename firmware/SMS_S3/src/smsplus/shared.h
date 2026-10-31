#ifndef _SHARED_H_
#define _SHARED_H_
#include <stdint.h>
#include <stdbool.h>
#include "sms_platform.h"

typedef unsigned char uint8;
typedef unsigned short int uint16;
typedef uint32_t uint32;

typedef signed char int8;
typedef signed short int int16;
typedef int32_t int32;

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <malloc.h>
#include <limits.h>
#include <math.h>
#define malloc sms_port_malloc
#define calloc sms_port_calloc
#define free sms_port_free

#ifdef RETRO_GO
#include <rg_system.h>
#define LOG_PRINTF(level, x...) rg_system_log(RG_LOG_PRINTF, NULL, x)
#define crc32_le(a, b, c) rg_crc32(a, b, c)
#else
#define LOG_PRINTF(level, x...) printf(x)
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif
#define crc32_le(a, b, c) sms_port_crc32(a, b, c)
#endif

#define MESSAGE_ERROR(x, ...) LOG_PRINTF(1, "!! %s: " x, __func__, ## __VA_ARGS__)
#define MESSAGE_WARN(x, ...)  LOG_PRINTF(2, "** %s: " x, __func__, ## __VA_ARGS__)
#define MESSAGE_INFO(x, ...)  LOG_PRINTF(3, " * %s: " x, __func__, ## __VA_ARGS__)
#define MESSAGE_DEBUG(x, ...) LOG_PRINTF(4, ">> %s: " x, __func__, ## __VA_ARGS__)

// #include "config.h"
#include "cpu/z80.h"
#include "memz80.h"
#include "loadrom.h"
#include "pio.h"
#include "render.h"
#include "sms.h"
#include "state.h"
#include "system.h"
#include "tms.h"
#include "vdp.h"
#include "sound/sn76489.h"
#include "sound/emu2413.h"
#include "sound/ym2413.h"
#include "sound/fmintf.h"
#include "sound/sound.h"

#endif /* _SHARED_H_ */

