/*
 * niffs_test_config.h
 *
 *  Created on: Feb 3, 2015
 *      Author: petera
 */

#ifndef NIFFS_TEST_CONFIG_H_
#define NIFFS_TEST_CONFIG_H_

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#define NIFFS_DUMP
#define NIFFS_DUMP_OUT(...) printf(__VA_ARGS__)
#define NIFFS_TEST

typedef unsigned int u32_t;
typedef unsigned short u16_t;
typedef unsigned char u8_t;
typedef signed int s32_t;
typedef signed short s16_t;
typedef signed char s8_t;

#define MIN(x,y) ((x)>(y)?(y):(x))
#define MAX(x,y) ((x)>(y)?(x):(y))

// test config

#define TEST_CHECK_UNALIGNED_ACCESS
#define TEST_CHECK_WRITE_ON_NONERASED_DATA_OTHER_THAN_ZERO

#define EMUL_SECTORS            8
#define EMUL_SECTOR_SIZE        1024

#define EMUL_FILE_DESCS         4

#define TEST_PARAM_PAGE_SIZE    128

#define TESTATIC

// niffs config

//#define NIFFS_DBG(...)          printf(__VA_ARGS__)
#define NIFFS_NAME_LEN          (16)
#define NIFFS_OBJ_ID_BITS       (8)
#define NIFFS_SPAN_IX_BITS      (6)
#define NIFFS_WORD_ALIGN        (2)

#define NIFFS_ASSERT(x) do { \
  if (!(x)) { \
    printf("ASSERT on %s:%i\n", __FILE__, __LINE__); \
    exit(0); \
  } \
} while (0)

// test internals

#define ERR_NIFFS_TEST_UNLIGNED_WRITE_LEN       -1000
#define ERR_NIFFS_TEST_UNLIGNED_WRITE_ADDR      -1001
#define ERR_NIFFS_TEST_WRITE_TO_NONERASED_DATA  -1002
#define ERR_NIFFS_TEST_BAD_ADDR                 -1003
#define ERR_NIFFS_TEST_BAD_LEN                  -1004
#define ERR_NIFFS_TEST_ABORTED_WRITE            -1005
#define ERR_NIFFS_TEST_ABORTED_READ             -1006

#endif /* TEST_NIFFS_TEST_CONFIG_H_ */
