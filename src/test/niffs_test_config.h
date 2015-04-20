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

typedef unsigned int u32_t;
typedef unsigned short u16_t;
typedef unsigned char u8_t;
typedef signed int s32_t;
typedef signed short s16_t;
typedef signed char s8_t;

#define MIN(x,y) ((x)>(y)?(y):(x))
#define MAX(x,y) ((x)>(y)?(x):(y))

// test config

// for test framework
#define NIFFS_TEST
#define TESTATIC
// emulate 16 sectors
#define EMUL_SECTORS            16
// each sector is 1024 bytes
#define EMUL_SECTOR_SIZE        1024
// use max 4 filedescriptors
#define EMUL_FILE_DESCS         4
// divide sectors in maximum 128 byte blocks
#define EMUL_PAGE_SIZE          128
// give niffs a 128 byte work buffer
#define EMUL_BUF_SIZE           128

// enable checks for stm32f1 flash writes
#define TEST_CHECK_UNALIGNED_ACCESS
#define TEST_CHECK_WRITE_ON_NONERASED_DATA_OTHER_THAN_ZERO

// niffs config

extern u8_t __dbg;
#define NIFFS_DBG_DEFAULT           0
#define NIFFS_DBG(...)              if (__dbg) printf(__VA_ARGS__)
#define NIFFS_NAME_LEN              (16)  // max 16 characters file name
#define NIFFS_OBJ_ID_BITS           (8)   // max 256-2 files
#define NIFFS_SPAN_IX_BITS          (8)   // max 256 pages of data per file
#define NIFFS_WORD_ALIGN            (2)   // 16-bit word alignment
#define NIFFS_TYPE_OBJ_ID_SIZE      u8_t  // see NIFFS_OBJ_ID_BITS
#define NIFFS_TYPE_SPAN_IX_SIZE     u8_t  // see NIFFS_SPAN_IX_BITS
#define NIFFS_TYPE_RAW_PAGE_ID_SIZE u16_t // see NIFFS_OBJ_ID_BITS + NIFFS_SPAN_IX_BITS
#define NIFFS_TYPE_PAGE_IX_SIZE     u16_t // max 65536 pages in filesystem
#define NIFFS_TYPE_PAGE_FLAG_SIZE   u16_t // use 16-bit page flag
#define NIFFS_TYPE_MAGIC_SIZE       u16_t // use 16-bit magic nbr
#define NIFFS_TYPE_ERASE_COUNT_SIZE u16_t // use 16-bit sector erase counter
#define NIFFS_DUMP                        // enable dumping
#define NIFFS_DUMP_OUT(...)         printf(__VA_ARGS__)
//#define NIFFS_EXPERIMENTAL_GC_DISTRIBUTED_SPARE_SECTOR  // let's be bold
//#define NIFFS_RD_ALLO_TEST

#define NIFFS_ASSERT(x) do { \
  if (!(x)) { \
    printf("ASSERT on %s:%i\n", __FILE__, __LINE__); \
    exit(0); \
  } \
} while (0)


#ifdef NIFFS_RD_ALLO_TEST
void *niffs_alloc_read(void *src, u32_t len);
void niffs_alloc_free(void *addr);
#endif

// test internals

#define ERR_NIFFS_TEST_UNLIGNED_WRITE_LEN       -1000
#define ERR_NIFFS_TEST_UNLIGNED_WRITE_ADDR      -1001
#define ERR_NIFFS_TEST_WRITE_TO_NONERASED_DATA  -1002
#define ERR_NIFFS_TEST_BAD_ADDR                 -1003
#define ERR_NIFFS_TEST_BAD_LEN                  -1004
#define ERR_NIFFS_TEST_ABORTED_WRITE            -1005
#define ERR_NIFFS_TEST_ABORTED_READ             -1006
#define ERR_NIFFS_TEST_REF_DATA_MISMATCH        -1007
#define ERR_NIFFS_TEST_EOF                      -1008
#define ERR_NIFFS_TEST_FATAL                    -1100

#endif /* TEST_NIFFS_TEST_CONFIG_H_ */
