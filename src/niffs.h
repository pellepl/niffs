/*
 * niffs.h
 *
 *  Created on: Feb 2, 2015
 *      Author: petera
 */

#ifndef NIFFS_H_
#define NIFFS_H_

#include "niffs_config.h"

#define NIFFS_SEEK_SET          (0)
#define NIFFS_SEEK_CUR          (1)
#define NIFFS_SEEK_END          (2)

#define NIFFS_OK                            0
#define ERR_NIFFS_BAD_CONF                  -1
#define ERR_NIFFS_DELETING_FREE_PAGE        -2
#define ERR_NIFFS_DELETING_DELETED_PAGE     -3
#define ERR_NIFFS_MOVING_FREE_PAGE          -4
#define ERR_NIFFS_MOVING_DELETED_PAGE       -5
#define ERR_NIFFS_MOVING_TO_UNFREE_PAGE     -6
#define ERR_NIFFS_NO_FREE_PAGE              -7
#define ERR_NIFFS_SECTOR_UNFORMATTABLE      -8
#define ERR_NIFFS_NULL_PTR                  -9
#define ERR_NIFFS_NO_FREE_ID                -10
#define ERR_NIFFS_WR_PHDR_UNFREE_PAGE       -11
#define ERR_NIFFS_WR_PHDR_BAD_ID            -12

typedef int (* niffs_hal_erase_f)(u8_t *addr, u32_t len);
typedef int (* niffs_hal_write_f)(u8_t *addr, u8_t *src, u32_t len);

typedef s32_t niffs_fd;

typedef struct {
  niffs_obj_id obj_id;
  niffs_page_ix obj_pix;
  u32_t len;
  u32_t offs;
  niffs_page_ix cur_pix;
} niffs_file_desc;

typedef struct {
  u8_t *phys_addr;
  u32_t sectors;
  u32_t sector_size;
  u32_t page_size;

  u8_t *buf;
  u32_t buf_len;

  niffs_hal_write_f hal_wr;
  niffs_hal_erase_f hal_er;

  u32_t pages_per_sector;
  niffs_page_ix last_free_pix;

} niffs;

int NIFFS_init(niffs *fs,
    u8_t *phys_addr,
    u32_t sectors,
    u32_t sector_size,
    u32_t page_size,
    u8_t *buf,
    u32_t buf_len,
    niffs_hal_erase_f erase_f,
    niffs_hal_write_f write_f
    );
int NIFFS_mount(niffs *fs);
int NIFFS_creat(niffs *fs, const char *name);
niffs_fd NIFFS_open(niffs *fs, const char *name, u8_t flags);
int NIFFS_read_ptr(niffs *fs, niffs_fd fd, const u8_t **ptr, u32_t *len);
int NIFFS_seek(niffs *fs, niffs_fd fd, s32_t offs, int whence);
int NIFFS_remove(niffs *fs, niffs_fd fd);
int NIFFS_write(niffs *fs, niffs_fd fd, u8_t *data, u32_t len);
int NIFFS_flush(niffs *fs, niffs_fd fd);
int NIFFS_close(niffs *fs, niffs_fd fd);
int NIFFS_unmount(niffs *fs);
int NIFFS_format(niffs *fs);

#endif /* NIFFS_H_ */
