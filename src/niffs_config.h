/*
 * niffs_config.h
 *
 *  Created on: Feb 3, 2015
 *      Author: petera
 */

#ifndef NIFFS_CONFIG_H_
#define NIFFS_CONFIG_H_

#include "niffs_test_config.h" // remove this on target

#ifndef TESTATIC
#define TESTATIC static
#endif

#ifndef NIFFS_DBG
#define NIFFS_DBG(...)
#endif

#ifdef NIFFS_DUMP
#ifndef NIFFS_DUMP_OUT
#define NIFFS_DUMP_OUT(...)
#endif
#endif

#ifndef NIFFS_ASSERT
#define NIFFS_ASSERT(x)
#endif

#ifndef NIFFS_NAME_LEN
#define NIFFS_NAME_LEN          (16)
#endif

#ifndef NIFFS_OBJ_ID_BITS
#define NIFFS_OBJ_ID_BITS       (8)
#endif

#ifndef NIFFS_SPAN_IX_BITS
#define NIFFS_SPAN_IX_BITS      (6)
#endif

#ifndef NIFFS_WORD_ALIGN
#define NIFFS_WORD_ALIGN        (2)
#endif


typedef u16_t niffs_magic; // must be sized on alignment
typedef u16_t niffs_erase_cnt; // must be sized on alignment
typedef u8_t niffs_obj_id; // must comprise NIFFS_OBJ_ID_BITS
typedef u8_t niffs_span_ix; // must comprise NIFFS_SPAN_IX_BITS
typedef u16_t niffs_flag; // must be sized on alignment
typedef u16_t niffs_page_id_raw; // must comprise (2 + NIFFS_OBJ_ID_BITS + NIFFS_SPAN_IX_BITS)
typedef u16_t niffs_page_ix;

#endif /* NIFFS_CONFIG_H_ */
