/*
 * niffs_internal.h
 *
 *  Created on: Feb 3, 2015
 *      Author: petera
 */

#include "niffs.h"
#include "niffs_config.h"

#ifndef NIFFS_INTERNAL_H_
#define NIFFS_INTERNAL_H_

#define _NIFFS_PAGE_FREE_ID     ((niffs_page_id_raw)-1)
#define _NIFFS_PAGE_DELE_ID     ((niffs_page_id_raw)0)

#define _NIFFS_FLAG_CLEAN       ((niffs_flag)-1)
#define _NIFFS_FLAG_WRITTEN     ((niffs_flag)1)
#define _NIFFS_FLAG_MOVING      ((niffs_flag)0)

#define _NIFFS_SECT_MAGIC(_fs)  (niffs_magic)(0xfee1c01d ^ (_fs)->page_size)

#define _NIFFS_ALIGN __attribute__ (( aligned(NIFFS_WORD_ALIGN) ))
#define _NIFFS_PACKED __attribute__ (( packed ))

#define _NIFFS_NXT_CYCLE(_c) ((_c) >= 3 ? 0 : (_c)+1)

#define _NIFFS_IS_FREE(_pg_hdr) (((_pg_hdr)->id.raw) == _NIFFS_PAGE_FREE_ID)
#define _NIFFS_IS_DELE(_pg_hdr) (((_pg_hdr)->id.raw) == _NIFFS_PAGE_DELE_ID)
#define _NIFFS_IS_CLEA(_pg_hdr) (((_pg_hdr)->flag) == _NIFFS_FLAG_CLEAN)
#define _NIFFS_IS_WRIT(_pg_hdr) (((_pg_hdr)->flag) == _NIFFS_FLAG_WRITTEN)
#define _NIFFS_IS_MOVI(_pg_hdr) (((_pg_hdr)->flag) == _NIFFS_FLAG_MOVING)

#define _NIFFS_SECTOR_2_ADDR(_fs, _s) \
  ((_fs)->phys_addr + (_fs)->sector_size * (_s))
#define _NIFFS_PIX_2_SECTOR(_fs, _pix) \
  ((_pix) / (_fs)->pages_per_sector)
#define _NIFFS_PIX_IN_SECTOR(_fs, _pix) \
  ((_pix) % (_fs)->pages_per_sector)
#define _NIFFS_PIX_AT_SECTOR(_fs, _s) \
  ((_s) * (_fs)->pages_per_sector)
#define _NIFFS_PIX_2_ADDR(_fs, _pix) (\
  _NIFFS_SECTOR_2_ADDR(_fs, _NIFFS_PIX_2_SECTOR(_fs, _pix)) +  \
  sizeof(niffs_sector_hdr) + \
  _NIFFS_PIX_IN_SECTOR(_fs, _pix) * fs->page_size \
  )

#define _NIFFS_SPIX_2_PDATA_LEN(_fs, _spix) \
  ((_fs)->page_size - sizeof(niffs_page_hdr) - ((_spix) == 0 ? sizeof(niffs_object_hdr) : 0))

#define _NIFFS_OFFS_2_SPIX(_fs, _offs) ( \
  (_offs) < _NIFFS_PDATA_LEN(_fs, 0) ? 0 : \
      (1 + (((_offs) - _NIFFS_PDATA_LEN(_fs, 0)) / _NIFFS_PDATA_LEN(_fs, 1))) \
)


#define NIFFS_EXCL_SECT_NONE  -1


typedef struct {
  _NIFFS_ALIGN niffs_magic abra; // page size xored with magic
  _NIFFS_ALIGN niffs_erase_cnt era_cnt;
} _NIFFS_PACKED niffs_sector_hdr;

typedef struct {
  union {
    niffs_page_id_raw raw;
    struct {
      u8_t cycle_ix : 2;
      niffs_span_ix spix : NIFFS_SPAN_IX_BITS;
      niffs_obj_id obj_id : NIFFS_OBJ_ID_BITS;
    };
  };
} _NIFFS_PACKED niffs_page_hdr_id;

typedef struct {
  _NIFFS_ALIGN niffs_page_hdr_id id;
  _NIFFS_ALIGN niffs_flag flag;
} _NIFFS_PACKED niffs_page_hdr;

typedef struct {
  niffs_page_hdr phdr;
  u32_t len;
  u8_t name[NIFFS_NAME_LEN];
} niffs_object_hdr;



#endif /* NIFFS_INTERNAL_H_ */
