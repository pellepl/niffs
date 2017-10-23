/*
 * niffs_config.h
 *
 *  Created on: Feb 3, 2015
 *      Author: petera
 */

#ifndef NIFFS_CONFIG_H_
#define NIFFS_CONFIG_H_

#ifdef NIFFS_TEST_MAKE
#include "niffs_test_config.h"
#endif

// enable this define to configure for an STM32F1 target
//#define NIFFS_STM32F1

#ifdef NIFFS_STM32F1
#define NIFFS_WORD_ALIGN        (2)
#define NIFFS_TYPE_PAGE_FLAG_SIZE u16_t
#endif

// for testing
#ifndef TESTATIC
#define TESTATIC static
#endif

// define for getting niffs debug output
#ifndef NIFFS_DBG
#define NIFFS_DBG(_f, ...)
#endif

// define NIFFS_DUMP to be able to visualize filesystem with NIFFS_dump
//#define NIFFS_DUMP
#ifdef NIFFS_DUMP
#ifndef NIFFS_DUMP_OUT
// used to output in NIFFS_dump
#define NIFFS_DUMP_OUT(_f, ...)
#endif
#endif

// define for assertions within niffs
#ifndef NIFFS_ASSERT
#define NIFFS_ASSERT(x)
#endif

// define maximum name length
#ifndef NIFFS_NAME_LEN
#define NIFFS_NAME_LEN          (16)
#endif

// Enable or disable linear area features.
// Files in the linear area are not divided by page headers but are linear on
// medium. To create a linear file, pass NIFFS_O_CREAT and NIFFS_O_CREAT_LINEAR
// when creating the file with open.
// To have more control over the linear file, NIFFS_mknod_linear can also be
// called, see corresponding function for more info.
// The linear area is not wear leveled. Once a linear file is deleted,
// corresponding sectors are immediately erased.
// This implies that each linear file will at least occupy one sector, even if
// the size is 0.
// Linear files can only be appended, never modified. Defragmentation of the
// linear area is up to the user.
#ifndef NIFFS_LINEAR_AREA
#define NIFFS_LINEAR_AREA       (1)
#endif

// define number of bits used for object ids, used for uniquely identify a file
#ifndef NIFFS_OBJ_ID_BITS
#define NIFFS_OBJ_ID_BITS       (8)
#endif

// define number of bits used for span indices, used for uniquely identify part of a file
#ifndef NIFFS_SPAN_IX_BITS
#define NIFFS_SPAN_IX_BITS      (8)
#endif

// word align for target flash, e.g. stm32f1 can only write 16-bit words at a time
#ifndef NIFFS_WORD_ALIGN
#define NIFFS_WORD_ALIGN        (1)
#endif

// garbage collection uses a score system to select sector to erase:
// sector_score = sector_erase_difference * F1 + free_pages * F2 + deleted_pages * F3 + busy_pages * F4
// sector with highest score is selected for garbage collection

// F1: garbage collection score factor for sector erase difference
#ifndef NIFFS_GC_SCORE_ERASE_CNT_DIFF
#define NIFFS_GC_SCORE_ERASE_CNT_DIFF (100)
#endif
// F2: garbage collection score factor for percentage of free pages in sector
#ifndef NIFFS_GC_SCORE_FREE
#define NIFFS_GC_SCORE_FREE (-4)
#endif
// F3: garbage collection score factor for percentage of deleted pages in sector
#ifndef NIFFS_GC_SCORE_DELE
#define NIFFS_GC_SCORE_DELE (2)
#endif
// F4: garbage collection score factor for percentage of busy/written pages in sector
#ifndef NIFFS_GC_SCORE_BUSY
#define NIFFS_GC_SCORE_BUSY (-2)
#endif

// formula for selecting sector to garbage collect
// free, dele and busy is the percentage (0-99) of each type
#ifndef NIFFS_GC_SCORE
#define NIFFS_GC_SCORE(era_cnt_diff, free, dele, busy) \
  ((era_cnt_diff) * NIFFS_GC_SCORE_ERASE_CNT_DIFF) + \
  ((free) * NIFFS_GC_SCORE_FREE) + \
  ((dele) * NIFFS_GC_SCORE_DELE) + \
  ((busy) * NIFFS_GC_SCORE_BUSY)
#endif

// type sizes, depend of the size of the filesystem and the size of the pages

// must comprise NIFFS_OBJ_ID_BITS
#ifndef NIFFS_TYPE_OBJ_ID_SIZE
#define NIFFS_TYPE_OBJ_ID_SIZE u8_t
#endif

// must comprise NIFFS_SPAN_IX_BITS
#ifndef NIFFS_TYPE_SPAN_IX_SIZE
#define NIFFS_TYPE_SPAN_IX_SIZE u8_t
#endif

// must comprise (NIFFS_OBJ_ID_BITS + NIFFS_SPAN_IX_BITS)
#ifndef NIFFS_TYPE_RAW_PAGE_ID_SIZE
#define NIFFS_TYPE_RAW_PAGE_ID_SIZE u16_t
#endif

// must uniquely address all pages
#ifndef NIFFS_TYPE_PAGE_IX_SIZE
#define NIFFS_TYPE_PAGE_IX_SIZE u16_t
#endif

// magic bits, must be sized on alignment, NIFFS_WORD_ALIGN
#ifndef NIFFS_TYPE_MAGIC_SIZE
#define NIFFS_TYPE_MAGIC_SIZE u16_t
#endif

// sector erase counter, must be sized on alignment, NIFFS_WORD_ALIGN
#ifndef NIFFS_TYPE_ERASE_COUNT_SIZE
#define NIFFS_TYPE_ERASE_COUNT_SIZE u16_t
#endif

// page flag, 3 values, must be sized on alignment, NIFFS_WORD_ALIGN
#ifndef NIFFS_TYPE_PAGE_FLAG_SIZE
#define NIFFS_TYPE_PAGE_FLAG_SIZE u8_t
#endif

typedef NIFFS_TYPE_OBJ_ID_SIZE niffs_obj_id;
typedef NIFFS_TYPE_SPAN_IX_SIZE niffs_span_ix;
typedef NIFFS_TYPE_RAW_PAGE_ID_SIZE niffs_page_id_raw;
typedef NIFFS_TYPE_PAGE_IX_SIZE niffs_page_ix;
typedef NIFFS_TYPE_MAGIC_SIZE niffs_magic;
typedef NIFFS_TYPE_ERASE_COUNT_SIZE niffs_erase_cnt;
typedef NIFFS_TYPE_PAGE_FLAG_SIZE niffs_flag;

#endif /* NIFFS_CONFIG_H_ */
