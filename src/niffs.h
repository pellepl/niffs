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
#define ERR_NIFFS_NOT_A_FILESYSTEM          -2
#define ERR_NIFFS_BAD_SECTOR                -3
#define ERR_NIFFS_DELETING_FREE_PAGE        -4
#define ERR_NIFFS_DELETING_DELETED_PAGE     -5
#define ERR_NIFFS_MOVING_FREE_PAGE          -6
#define ERR_NIFFS_MOVING_DELETED_PAGE       -7
#define ERR_NIFFS_MOVING_TO_UNFREE_PAGE     -8
#define ERR_NIFFS_MOVING_TO_SAME_PAGE       -9
#define ERR_NIFFS_NO_FREE_PAGE              -10
#define ERR_NIFFS_SECTOR_UNFORMATTABLE      -11
#define ERR_NIFFS_NULL_PTR                  -12
#define ERR_NIFFS_NO_FREE_ID                -13
#define ERR_NIFFS_WR_PHDR_UNFREE_PAGE       -14
#define ERR_NIFFS_WR_PHDR_BAD_ID            -15
#define ERR_NIFFS_NAME_CONFLICT             -16
#define ERR_NIFFS_FULL                      -17
#define ERR_NIFFS_OUT_OF_FILEDESCS          -18
#define ERR_NIFFS_FILE_NOT_FOUND            -19
#define ERR_NIFFS_FILEDESC_CLOSED           -20
#define ERR_NIFFS_FILEDESC_BAD              -21
#define ERR_NIFFS_INCOHERENT_ID             -22
#define ERR_NIFFS_PAGE_NOT_FOUND            -23
#define ERR_NIFFS_END_OF_FILE               -24
#define ERR_NIFFS_MODIFY_BEYOND_FILE        -25
#define ERR_NIFFS_TRUNCATE_BEYOND_FILE      -26
#define ERR_NIFFS_NO_GC_CANDIDATE           -27
#define ERR_NIFFS_PAGE_DELETED              -28
#define ERR_NIFFS_PAGE_FREE                 -29
#define ERR_NIFFS_MOUNTED                   -30
#define ERR_NIFFS_NOT_MOUNTED               -31

typedef int (* niffs_hal_erase_f)(u8_t *addr, u32_t len);
typedef int (* niffs_hal_write_f)(u8_t *addr, u8_t *src, u32_t len);

typedef struct {
  niffs_obj_id obj_id;
  niffs_page_ix obj_pix;
  u32_t offs;
  niffs_page_ix cur_pix;
} niffs_file_desc;

typedef struct {
  // cfg
  u8_t *phys_addr;
  u32_t sectors;
  u32_t sector_size;
  u32_t page_size;

  u8_t *buf;
  u32_t buf_len;

  niffs_hal_write_f hal_wr;
  niffs_hal_erase_f hal_er;

  // dyna
  u32_t pages_per_sector;
  niffs_page_ix last_free_pix;
  u8_t mounted;
  u32_t free_pages;
  u32_t dele_pages;
  niffs_file_desc *descs;
  u32_t descs_len;
  u32_t max_era;

} niffs;

int NIFFS_init(niffs *fs,
    u8_t *phys_addr,
    u32_t sectors,
    u32_t sector_size,
    u32_t page_size,
    u8_t *buf,
    u32_t buf_len,
    niffs_file_desc *descs,
    u32_t file_desc_len,
    niffs_hal_erase_f erase_f,
    niffs_hal_write_f write_f
    );
int NIFFS_mount(niffs *fs);
int NIFFS_creat(niffs *fs, char *name);
int NIFFS_open(niffs *fs, char *name, u8_t flags);
int NIFFS_read_ptr(niffs *fs, int fd, const u8_t **ptr, u32_t *len);
int NIFFS_read(niffs *fs, int fd, u8_t *dst, u32_t len);
int NIFFS_seek(niffs *fs, int fd, s32_t offs, int whence);
int NIFFS_remove(niffs *fs, int fd);
int NIFFS_write(niffs *fs, int fd, u8_t *data, u32_t len);
int NIFFS_flush(niffs *fs, int fd);
int NIFFS_close(niffs *fs, int fd);
int NIFFS_rename(niffs *fs, char *old_path, char *new_path);
int NIFFS_unmount(niffs *fs);
int NIFFS_format(niffs *fs);
#ifdef NIFFS_DUMP
void NIFFS_dump(niffs *fs);
#endif

#endif /* NIFFS_H_ */
