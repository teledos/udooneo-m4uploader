/*
 * mqx_upload_on_m4SoloX.c operations to load fw and startup M4 core
 *
 * Copyright (C) 2015-2016 Giuseppe Pagano <giuseppe.pagano@seco.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/mman.h>

#ifdef ANDROID
#define LOG_TAG "M4Uploader"
#include <cutils/log.h>
#define LogDebug ALOGD
#define LogError ALOGE
#else
#define LogDebug printf
#define LogError printf
#endif

#define MAP_SIZE			4096UL
#define MAP_MASK			(MAP_SIZE - 1)
#define SIZE_4BYTE			4UL
#define SIZE_16BYTE			16UL
#define MAP_OCRAM_SIZE		512*1024
#define MAP_OCRAM_MASK		(MAP_OCRAM_SIZE - 1)
#define MAX_FILE_SIZE		MAP_OCRAM_SIZE
#define MAX_RETRIES			32
#define HANDSHAKE_MSG		"0xHELLOM4"

#define ADDR_STACK_PC                   0x007F8000
#define ADDR_SRC_SCR                    0x020D8000
#define M4c_RST                         (1 << 4)
#define M4p_RST                         (1 << 12)
#define M4e_RST                         (1 << 22)

#define ADDR_GATE_M4_CLOCK              0x020C4074
#define GATE_M4_CLOCK                   0x0000000C

#define ADDR_SHARED_TRACE_FLAGS         0xBFF0FFF4 // address in shared RAM for M4 trace flags
#define ADDR_SHARED_BYTE_FOR_M4STOP     0xBFF0FFFF // to force M4 sketch to secure exit
#define TRACE01_TOOLCHAIN_STARTUP       0x00000001
#define TRACE02_MAIN_CALLED             0x00000002
#define TRACE03_MQX_STARTED             0x00000004
#define TRACE04_BSP_PRE_INIT            0x00000008
#define TRACE05_BSP_INIT                0x00000010
#define TRACE06_MAIN_TASK_RUN           0x00000020
#define TRACE07_EXIT_TASK_RUN           0x00000040
#define TRACE08_ARDUINO_LOOP_TASK_RUN   0x00000080
#define TRACE09_ARDUINO_YIELD_TASK_RUN  0x00000100
#define TRACE10_MCC_RX_TASK_RUN         0x00000200
#define TRACE11_UART_RX_TASK_RUN        0x00000400
#define TRACE12_MQX_EXIT                0x00000800
#define TRACE13_RPMSG_INIT_LOCKED       0x00001000
#define TRACE14_RPMSG_TX_CHANNEL        0x00002000
#define TRACE15_RPMSG_RX_TASK_RUN       0x00004000

#define MQX_RUNNING						(TRACE01_TOOLCHAIN_STARTUP | TRACE02_MAIN_CALLED | TRACE03_MQX_STARTED | \
										 TRACE04_BSP_PRE_INIT | TRACE05_BSP_INIT)
#define SKETCH_TASKS_RUNNING            (TRACE07_EXIT_TASK_RUN | TRACE08_ARDUINO_LOOP_TASK_RUN | TRACE09_ARDUINO_YIELD_TASK_RUN)
#define RPMSG_NEEDS_UNLOCK				(TRACE13_RPMSG_INIT_LOCKED | TRACE14_RPMSG_TX_CHANNEL)

#define RETURN_CODE_OK                  0
#define RETURN_CODE_ARGUMENTS_ERROR     1
#define RETURN_CODE_M4STOP_FAILED       2
#define RETURN_CODE_M4START_FAILED      3

