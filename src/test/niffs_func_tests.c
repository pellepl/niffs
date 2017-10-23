/*
 * niffs_func_tests.c
 *
 *  Created on: Feb 3, 2015
 *      Author: petera
 */

#include "testrunner.h"
#include "niffs_test_emul.h"

SUITE(niffs_func_tests)

static void setup(test *t) {
  (void)niffs_emul_init();
}

static void teardown(test *t) {
  if (t->test_result != TEST_RES_OK) {
    NIFFS_dump(&fs);
  }

  niffs_emul_destroy_all_data();
}


#ifdef NIFFS_DUMP
TEST(func_dump) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(res, NIFFS_OK);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);
  NIFFS_dump(&fs);
  return TEST_RES_OK;
} TEST_END
#endif

TEST(func_info) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(res, NIFFS_OK);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);
  s32_t tot, used;
  u8_t of;
  TEST_CHECK_EQ(NIFFS_info(&fs, &tot, &used, &of), NIFFS_OK);
  TEST_CHECK_GE(fs.sector_size * fs.sectors, tot);
  TEST_CHECK_LE(fs.sector_size * fs.sectors/2, tot);
  TEST_CHECK_GT(tot, used);
  TEST_CHECK_GT(tot, fs.sector_size);
  TEST_CHECK_EQ(used, 0);
  return TEST_RES_OK;
} TEST_END

TEST(func_init_virgin) {
  return TEST_RES_OK;
} TEST_END

TEST(func_format_virgin) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);

  niffs_page_ix pix;
  res = niffs_find_free_page(&fs, &pix, NIFFS_EXCL_SECT_NONE);
  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(pix, 0);

  niffs_obj_id id;
  res = niffs_find_free_id(&fs, &id, 0);
  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(id, 1);

  return TEST_RES_OK;
} TEST_END

TEST(func_mount_clean) {
  TEST_CHECK_EQ(NIFFS_mount(&fs), ERR_NIFFS_NOT_A_FILESYSTEM);

  return TEST_RES_OK;
} TEST_END

TEST(func_mount_scrap) {
  niffs_emul_rand_filesystem();
  TEST_CHECK_EQ(NIFFS_mount(&fs), ERR_NIFFS_NOT_A_FILESYSTEM);

  return TEST_RES_OK;
} TEST_END

TEST(func_find_free_page) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);

  u32_t s;
  for (s = 0; s < fs.sectors; s++) {
    niffs_page_ix pix;
    res = niffs_find_free_page(&fs, &pix, s);
    TEST_CHECK_EQ(res,  NIFFS_OK);
    TEST_CHECK(s != _NIFFS_PIX_2_SECTOR(&fs, pix));
  }

  return TEST_RES_OK;
} TEST_END

TEST(func_write_phdr) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);

  niffs_page_hdr phdr;
  phdr.flag = _NIFFS_FLAG_WRITTEN;
  phdr.id.spix = 0;

  niffs_page_ix pix = 0;
  phdr.id.obj_id = 0;
  res = niffs_write_phdr(&fs, pix, &phdr);
  TEST_CHECK_EQ(res,  ERR_NIFFS_WR_PHDR_BAD_ID);
  phdr.id.obj_id = (niffs_obj_id)-1;
  res = niffs_write_phdr(&fs, pix, &phdr);
  TEST_CHECK_EQ(res,  ERR_NIFFS_WR_PHDR_BAD_ID);

  phdr.id.obj_id = 1;
  res = niffs_write_phdr(&fs, pix, &phdr);
  TEST_CHECK_EQ(res,  NIFFS_OK);

  res = niffs_write_phdr(&fs, pix, &phdr);
  TEST_CHECK_EQ(res,  ERR_NIFFS_WR_PHDR_UNFREE_PAGE);

  return TEST_RES_OK;
} TEST_END

TEST(func_write_phdr_fill) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);

  niffs_page_hdr phdr;
  phdr.flag = _NIFFS_FLAG_WRITTEN;
  phdr.id.spix = 0;

  niffs_page_ix pix;
  do {
    niffs_obj_id id;
    res = niffs_find_free_id(&fs, &id, 0);
    if (res != NIFFS_OK) break;
    phdr.id.obj_id = id;
    res = niffs_find_free_page(&fs, &pix, NIFFS_EXCL_SECT_NONE);
    if (res != NIFFS_OK) break;

    res = niffs_write_phdr(&fs, pix, &phdr);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  } while (res == NIFFS_OK);

  TEST_CHECK(res == ERR_NIFFS_NO_FREE_PAGE || res == ERR_NIFFS_NO_FREE_ID);

  return TEST_RES_OK;
} TEST_END

TEST(func_creat) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);

  TEST_CHECK_EQ(niffs_create(&fs, 0, _NIFFS_FTYPE_FILE), ERR_NIFFS_NULL_PTR);
  TEST_CHECK_EQ(niffs_create(&fs, "test", _NIFFS_FTYPE_FILE), NIFFS_OK);
  TEST_CHECK_EQ(niffs_create(&fs, "test", _NIFFS_FTYPE_FILE), ERR_NIFFS_NAME_CONFLICT);

  return TEST_RES_OK;
} TEST_END

TEST(func_creat_full) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);

  int i;
  for (i = 0; i < (fs.sectors-1) * fs.pages_per_sector; i++) {
    char fname[16];
    sprintf(fname, "t%i", i);
    res = niffs_create(&fs, fname, _NIFFS_FTYPE_FILE);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  res = niffs_create(&fs, "overflow", _NIFFS_FTYPE_FILE);
  TEST_CHECK_EQ(res,  ERR_NIFFS_FULL);

  return TEST_RES_OK;
} TEST_END

TEST(func_fd) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);

  TEST_CHECK_EQ(niffs_create(&fs, "test", _NIFFS_FTYPE_FILE), NIFFS_OK);
  int fd;
  int fd_pre = -1;
  u32_t i;
  for (i = 0; i < fs.descs_len; i++) {
    fd = niffs_open(&fs, "test", NIFFS_O_RDWR);
    TEST_CHECK(fd != fd_pre);
    fd_pre = fd;
  }
  fd = niffs_open(&fs, "test", NIFFS_O_RDWR);
  TEST_CHECK_EQ(fd, ERR_NIFFS_OUT_OF_FILEDESCS);

  return TEST_RES_OK;
} TEST_END

TEST(func_delete) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);
  TEST_CHECK_EQ(niffs_delete_page(&fs, 0), ERR_NIFFS_DELETING_FREE_PAGE);
  TEST_CHECK_EQ(niffs_create(&fs, "moo", _NIFFS_FTYPE_FILE), NIFFS_OK);
  TEST_CHECK_EQ(niffs_delete_page(&fs, 0), NIFFS_OK);
  TEST_CHECK_EQ(niffs_delete_page(&fs, 0), ERR_NIFFS_DELETING_DELETED_PAGE);

  return TEST_RES_OK;
} TEST_END

TEST(func_move) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);
  TEST_CHECK_EQ(niffs_move_page(&fs, 0, 0, 0, 0, NIFFS_FLAG_MOVE_KEEP), ERR_NIFFS_MOVING_TO_SAME_PAGE);
  TEST_CHECK_EQ(niffs_move_page(&fs, 0, 1, 0, 0, NIFFS_FLAG_MOVE_KEEP), ERR_NIFFS_MOVING_FREE_PAGE);
  TEST_CHECK_EQ(niffs_create(&fs, "moo", _NIFFS_FTYPE_FILE), NIFFS_OK);
  TEST_CHECK_EQ(niffs_move_page(&fs, 0, 1, 0, 0, NIFFS_FLAG_MOVE_KEEP), NIFFS_OK);
  TEST_CHECK_EQ(niffs_move_page(&fs, 0, 1, 0, 0, NIFFS_FLAG_MOVE_KEEP), ERR_NIFFS_MOVING_DELETED_PAGE);
  TEST_CHECK_EQ(niffs_move_page(&fs, 1, 0, 0, 0, NIFFS_FLAG_MOVE_KEEP), ERR_NIFFS_MOVING_TO_UNFREE_PAGE);

  return TEST_RES_OK;
} TEST_END

TEST(func_open) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);

  int fd = niffs_open(&fs, "test", NIFFS_O_RDWR);
  TEST_CHECK_EQ(fd, ERR_NIFFS_FILE_NOT_FOUND);

  res = niffs_create(&fs, "test", _NIFFS_FTYPE_FILE);
  TEST_CHECK_EQ(res,  NIFFS_OK);

  fd = niffs_open(&fs, "test", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);

  return TEST_RES_OK;
} TEST_END

TEST(func_append_read) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);

  u32_t free_pages_clean = fs.free_pages;

  res = niffs_create(&fs, "test", _NIFFS_FTYPE_FILE);
  TEST_CHECK_EQ(res,  NIFFS_OK);

  // append to empty file, almost a page

  int fd = niffs_open(&fs, "test", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);

  u32_t len = _NIFFS_SPIX_2_PDATA_LEN(&fs, 0)-4;
  u8_t *d = niffs_emul_create_data("test", len);
  res = niffs_append(&fs, fd, d, len);
  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);

  fd = niffs_open(&fs, "test", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);

  u8_t *rptr;
  u32_t rlen;
  u32_t ix = 0;

  while (ix < len) {
    res = niffs_emul_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    res = memcmp(rptr, &d[ix], rlen);
    TEST_CHECK_EQ(res,  0);
    ix += rlen;
    res = niffs_seek(&fs, fd, ix, NIFFS_SEEK_SET);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
  TEST_CHECK_EQ(ix, len);
  TEST_CHECK_EQ(free_pages_clean - fs.free_pages, 1);
  TEST_CHECK_EQ(fs.dele_pages, 0);

  // append one page file, rest of page

  fd = niffs_open(&fs, "test", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);

  u32_t len2 = 4;
  u8_t *d2 = niffs_emul_create_data("test_rest", len2);
  res = niffs_append(&fs, fd, d2, len2);
  TEST_CHECK_EQ(res,  NIFFS_OK);

  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);

  fd = niffs_open(&fs, "test", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);

  ix = 0;

  while (ix < len) {
    res = niffs_emul_read_ptr(&fs, fd, &rptr, &rlen);
    u32_t alen = MIN(rlen, len-ix);
    TEST_CHECK(res > 0);
    res = memcmp(rptr, &d[ix], alen);
    TEST_CHECK_EQ(res,  0);
    ix += alen;
    res = niffs_seek(&fs, fd, ix, NIFFS_SEEK_SET);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  while (ix < len + len2) {
    res = niffs_emul_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    u32_t alen = MIN(rlen, len-(ix-len));
    res = memcmp(rptr, &d2[ix-len], alen);
    TEST_CHECK_EQ(res,  0);
    ix += alen;
    res = niffs_seek(&fs, fd, ix, NIFFS_SEEK_SET);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  TEST_CHECK_EQ(res,  NIFFS_OK);

  res = niffs_emul_read_ptr(&fs, fd, &rptr, &rlen);
  TEST_CHECK(res == ERR_NIFFS_END_OF_FILE);

  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
  TEST_CHECK_EQ(ix, len+len2);
  TEST_CHECK_EQ(free_pages_clean - fs.free_pages, 1+1);
  TEST_CHECK_EQ(fs.dele_pages, 1);

  // append one and a half pages
  fd = niffs_open(&fs, "test", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);

  u32_t len3 = _NIFFS_SPIX_2_PDATA_LEN(&fs, 1) + _NIFFS_SPIX_2_PDATA_LEN(&fs, 1)/2;
  u8_t *d3 = niffs_emul_create_data("test_1_5_more", len3);
  res = niffs_append(&fs, fd, d3, len3);
  TEST_CHECK_EQ(res,  NIFFS_OK);

  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);

  fd = niffs_open(&fs, "test", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);

  ix = 0;

  while (ix < len) {
    res = niffs_emul_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    u32_t alen = MIN(rlen, len-ix);

    res = memcmp(rptr, &d[ix], alen);
    TEST_CHECK_EQ(res,  0);
    ix += alen;
    res = niffs_seek(&fs, fd, ix, NIFFS_SEEK_SET);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  while (ix < len + len2) {
    res = niffs_emul_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    u32_t alen = MIN(rlen, len2-(ix-len));
    res = memcmp(rptr, &d2[ix-len], alen);
    TEST_CHECK_EQ(res,  0);
    ix += alen;
    res = niffs_seek(&fs, fd, ix, NIFFS_SEEK_SET);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  while (ix < len + len2 + len3) {
    res = niffs_emul_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    u32_t alen = MIN(rlen, len3-(ix-len-len2));
    res = memcmp(rptr, &d3[ix-len-len2], alen);
    TEST_CHECK_EQ(res,  0);
    ix += alen;
    res = niffs_seek(&fs, fd, ix, NIFFS_SEEK_SET);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
  TEST_CHECK_EQ(ix, len+len2+len3);
  TEST_CHECK_EQ(free_pages_clean - fs.free_pages, 1+1+3);
  TEST_CHECK_EQ(fs.dele_pages, 2);

  // append just a little more data
  fd = niffs_open(&fs, "test", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);

  u32_t len4 = 8;
  u8_t *d4 = niffs_emul_create_data("littlemore", len4);
  res = niffs_append(&fs, fd, d4, len4);
  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);

  fd = niffs_open(&fs, "test", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);

  ix = 0;

  while (ix < len) {
     res = niffs_emul_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    u32_t alen = MIN(rlen, len-ix);
    res = memcmp(rptr, &d[ix], alen);
    TEST_CHECK_EQ(res,  0);
    ix += alen;
    res = niffs_seek(&fs, fd, ix, NIFFS_SEEK_SET);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  while (ix < len + len2) {
    res = niffs_emul_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    u32_t alen = MIN(rlen, len2-(ix-len));
     res = memcmp(rptr, &d2[ix-len], alen);
    TEST_CHECK_EQ(res,  0);
    ix += alen;
    res = niffs_seek(&fs, fd, ix, NIFFS_SEEK_SET);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  while (ix < len + len2 + len3) {
    res = niffs_emul_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    u32_t alen = MIN(rlen, len3-(ix-len-len2));
    res = memcmp(rptr, &d3[ix-len-len2], alen);
    TEST_CHECK_EQ(res,  0);
    ix += alen;
    res = niffs_seek(&fs, fd, ix, NIFFS_SEEK_SET);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  while (ix < len + len2 + len3 + len4) {
    res = niffs_emul_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    u32_t alen = MIN(rlen, len4-(ix-len-len2-len3));
    res = memcmp(rptr, &d4[ix-len-len2-len3], alen);
    TEST_CHECK_EQ(res,  0);
    ix += alen;
    res = niffs_seek(&fs, fd, ix, NIFFS_SEEK_SET);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
  TEST_CHECK_EQ(ix, len+len2+len3+len4);
  TEST_CHECK_EQ(fs.dele_pages, 4);
  TEST_CHECK_EQ(free_pages_clean - fs.free_pages, 1+1+3+2);

  return TEST_RES_OK;
} TEST_END

TEST(func_modify_ohdr) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);

  res = niffs_create(&fs, "modify", _NIFFS_FTYPE_FILE);
  TEST_CHECK_EQ(res,  NIFFS_OK);

  // append to empty file, one page
  int fd = niffs_open(&fs, "modify", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);

  u32_t len = _NIFFS_SPIX_2_PDATA_LEN(&fs, 0);
  u8_t *d = niffs_emul_create_data("modify", len);
  res = niffs_append(&fs, fd, d, len);
  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);

  u32_t b_ix = 4;
  u32_t e_ix = len - 4;
  u8_t *dm = niffs_emul_create_data("modified", e_ix - b_ix);
  u32_t i;
  for (i = b_ix; i < e_ix; i++) {
    d[i] = dm[i-b_ix];
  }

  // modify midst of ohdr page

  fd = niffs_open(&fs, "modify", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);

  res = niffs_modify(&fs, fd, b_ix, dm, e_ix - b_ix);
  TEST_CHECK_EQ(res,  0);

  u8_t *rptr;
  u32_t rlen;
  u32_t ix = 0;

  TEST_CHECK_EQ(niffs_seek(&fs, fd, 0, NIFFS_SEEK_SET), NIFFS_OK);
  while (ix < len) {
    res = niffs_emul_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    res = memcmp(rptr, &d[ix], rlen);
    TEST_CHECK_EQ(res,  0);
    ix += rlen;
    res = niffs_seek(&fs, fd, ix, NIFFS_SEEK_SET);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
  TEST_CHECK_EQ(ix, len);


  return TEST_RES_OK;
} TEST_END

TEST(func_modify_page) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);

  res = niffs_create(&fs, "modify", _NIFFS_FTYPE_FILE);
  TEST_CHECK_EQ(res,  NIFFS_OK);

  // append to empty file, two pages
  int fd = niffs_open(&fs, "modify", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);

  u32_t len = _NIFFS_SPIX_2_PDATA_LEN(&fs, 0) + _NIFFS_SPIX_2_PDATA_LEN(&fs, 1);
  u8_t *d = niffs_emul_create_data("modify", len);
  res = niffs_append(&fs, fd, d, len);
  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);

  u32_t b_ix = _NIFFS_SPIX_2_PDATA_LEN(&fs, 0) + 4;
  u32_t e_ix = len - 4;
  u8_t *dm = niffs_emul_create_data("modified", e_ix - b_ix);
  u32_t i;
  for (i = b_ix; i < e_ix; i++) {
    d[i] = dm[i-b_ix];
  }

  // modify midst of data page

  fd = niffs_open(&fs, "modify", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);

  res = niffs_modify(&fs, fd, b_ix, dm, e_ix - b_ix);
  TEST_CHECK_EQ(res,  0);

  u8_t *rptr;
  u32_t rlen;
  u32_t ix = 0;

  TEST_CHECK_EQ(niffs_seek(&fs, fd, 0, NIFFS_SEEK_SET), NIFFS_OK);
  while (ix < len) {
    res = niffs_emul_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    res = memcmp(rptr, &d[ix], rlen);
    TEST_CHECK_EQ(res,  0);
    ix += rlen;
    res = niffs_seek(&fs, fd, ix, NIFFS_SEEK_SET);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
  TEST_CHECK_EQ(ix, len);


  return TEST_RES_OK;
} TEST_END

TEST(func_modify_pagespan) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);

  res = niffs_create(&fs, "modify", _NIFFS_FTYPE_FILE);
  TEST_CHECK_EQ(res,  NIFFS_OK);

  // append to empty file, two pages
  int fd = niffs_open(&fs, "modify", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);

  u32_t len = _NIFFS_SPIX_2_PDATA_LEN(&fs, 0) + _NIFFS_SPIX_2_PDATA_LEN(&fs, 1);
  u8_t *d = niffs_emul_create_data("modify", len);
  res = niffs_append(&fs, fd, d, len);
  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);

  u32_t b_ix = 4;
  u32_t e_ix = len - 4;
  u8_t *dm = niffs_emul_create_data("modified", e_ix - b_ix);
  u32_t i;
  for (i = b_ix; i < e_ix; i++) {
    d[i] = dm[i-b_ix];
  }

  // modify midst of ohdr page till midst of data page

  fd = niffs_open(&fs, "modify", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);

  res = niffs_modify(&fs, fd, b_ix, dm, e_ix - b_ix);
  TEST_CHECK_EQ(res,  0);

  u8_t *rptr;
  u32_t rlen;
  u32_t ix = 0;

  TEST_CHECK_EQ(niffs_seek(&fs, fd, 0, NIFFS_SEEK_SET), NIFFS_OK);
  while (ix < len) {
    res = niffs_emul_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    res = memcmp(rptr, &d[ix], rlen);
    TEST_CHECK_EQ(res,  0);
    ix += rlen;
    res = niffs_seek(&fs, fd, ix, NIFFS_SEEK_SET);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
  TEST_CHECK_EQ(ix, len);


  return TEST_RES_OK;
} TEST_END

TEST(func_modify_pagespan_nobreak) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);

  res = niffs_create(&fs, "modify", _NIFFS_FTYPE_FILE);
  TEST_CHECK_EQ(res,  NIFFS_OK);

  // append to empty file, four pages
  int fd = niffs_open(&fs, "modify", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);

  u32_t len = _NIFFS_SPIX_2_PDATA_LEN(&fs, 0) + _NIFFS_SPIX_2_PDATA_LEN(&fs, 1)*3;
  u8_t *d = niffs_emul_create_data("modify", len);
  res = niffs_append(&fs, fd, d, len);
  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);

  u32_t b_ix = _NIFFS_SPIX_2_PDATA_LEN(&fs, 0);
  u32_t e_ix = _NIFFS_SPIX_2_PDATA_LEN(&fs, 0) + _NIFFS_SPIX_2_PDATA_LEN(&fs, 1)*2;
  u8_t *dm = niffs_emul_create_data("modified", e_ix - b_ix);
  u32_t i;
  for (i = b_ix; i < e_ix; i++) {
    d[i] = dm[i-b_ix];
  }

  // modify midst of ohdr page till midst of data page

  fd = niffs_open(&fs, "modify", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);

  res = niffs_modify(&fs, fd, b_ix, dm, e_ix - b_ix);
  TEST_CHECK_EQ(res,  0);

  u8_t *rptr;
  u32_t rlen;
  u32_t ix = 0;

  TEST_CHECK_EQ(niffs_seek(&fs, fd, 0, NIFFS_SEEK_SET), NIFFS_OK);
  while (ix < len) {
    res = niffs_emul_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    res = memcmp(rptr, &d[ix], rlen);
    TEST_CHECK_EQ(res,  0);
    ix += rlen;
    res = niffs_seek(&fs, fd, ix, NIFFS_SEEK_SET);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
  TEST_CHECK_EQ(ix, len);


  return TEST_RES_OK;
} TEST_END

TEST(func_modify_beyond) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);

  res = niffs_create(&fs, "modify", _NIFFS_FTYPE_FILE);
  TEST_CHECK_EQ(res,  NIFFS_OK);

  // append to empty file, one page
  int fd = niffs_open(&fs, "modify", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);

  u32_t len = _NIFFS_SPIX_2_PDATA_LEN(&fs, 0);
  u8_t *d = niffs_emul_create_data("modify", len);

  res = niffs_modify(&fs, fd, 0, d, len);
  TEST_CHECK_EQ(res, ERR_NIFFS_MODIFY_BEYOND_FILE);

  res = niffs_append(&fs, fd, d, len);
  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);

  fd = niffs_open(&fs, "modify", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);

  u8_t *dm = niffs_emul_create_data("modified", len);

  res = niffs_modify(&fs, fd, 4, dm, len);
  TEST_CHECK_EQ(res, ERR_NIFFS_MODIFY_BEYOND_FILE);

  // modify midst of ohdr page

  fd = niffs_open(&fs, "modify", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);

  u8_t *rptr;
  u32_t rlen;
  u32_t ix = 0;

  while (ix < len) {
    res = niffs_emul_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    res = memcmp(rptr, &d[ix], rlen);
    TEST_CHECK_EQ(res,  0);
    ix += rlen;
    res = niffs_seek(&fs, fd, ix, NIFFS_SEEK_SET);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
  TEST_CHECK_EQ(ix, len);


  return TEST_RES_OK;
} TEST_END

TEST(func_truncate) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);

  res = niffs_create(&fs, "trunc", _NIFFS_FTYPE_FILE);
  TEST_CHECK_EQ(res,  NIFFS_OK);

  // append to empty file, three pages
  int fd = niffs_open(&fs, "trunc", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);

  u32_t len = _NIFFS_SPIX_2_PDATA_LEN(&fs, 0) + _NIFFS_SPIX_2_PDATA_LEN(&fs, 1) * 2;
  u8_t *d = niffs_emul_create_data("trunc", len);

  res = niffs_append(&fs, fd, d, len);
  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);


  u8_t *rptr;
  u32_t rlen;
  u32_t ix = 0;

  fd = niffs_open(&fs, "trunc", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);
  while (ix < len) {
    res = niffs_emul_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    res = memcmp(rptr, &d[ix], rlen);
    TEST_CHECK_EQ(res,  0);
    ix += rlen;
    res = niffs_seek(&fs, fd, ix, NIFFS_SEEK_SET);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  TEST_CHECK_EQ(res,  NIFFS_OK);

  res = niffs_emul_read_ptr(&fs, fd, &rptr, &rlen);
  TEST_CHECK(res == ERR_NIFFS_END_OF_FILE);

  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
  TEST_CHECK_EQ(ix, len);
  TEST_CHECK_EQ(fs.dele_pages, 0);

  // truncate half a page

  len -= _NIFFS_SPIX_2_PDATA_LEN(&fs, 1) / 2;
  fd = niffs_open(&fs, "trunc", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);
  TEST_CHECK_EQ(niffs_truncate(&fs, fd, len), NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);

  fd = niffs_open(&fs, "trunc", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);
  ix = 0;
  while (ix < len) {
    res = niffs_emul_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    res = memcmp(rptr, &d[ix], rlen);
    TEST_CHECK_EQ(res,  0);
    ix += rlen;
    res = niffs_seek(&fs, fd, ix, NIFFS_SEEK_SET);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  TEST_CHECK_EQ(res,  NIFFS_OK);

  res = niffs_emul_read_ptr(&fs, fd, &rptr, &rlen);
  TEST_CHECK(res == ERR_NIFFS_END_OF_FILE);

  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
  TEST_CHECK_EQ(ix, len);
  TEST_CHECK_EQ(fs.dele_pages, 1); // rewritten obj hdr

  // truncate to two pages

  len = _NIFFS_SPIX_2_PDATA_LEN(&fs, 0) * 2;
  fd = niffs_open(&fs, "trunc", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);
  TEST_CHECK_EQ(niffs_truncate(&fs, fd, len), NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);

  fd = niffs_open(&fs, "trunc", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);
  ix = 0;
  while (ix < len) {
    res = niffs_emul_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    res = memcmp(rptr, &d[ix], rlen);
    TEST_CHECK_EQ(res,  0);
    ix += rlen;
    res = niffs_seek(&fs, fd, ix, NIFFS_SEEK_SET);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  TEST_CHECK_EQ(res,  NIFFS_OK);

  res = niffs_emul_read_ptr(&fs, fd, &rptr, &rlen);
  TEST_CHECK(res == ERR_NIFFS_END_OF_FILE);

  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
  TEST_CHECK_EQ(ix, len);
  TEST_CHECK_EQ(fs.dele_pages, 1 + 2); // rewritten obj hdr + deleted page

  // truncate, rm

  len = 0;
  fd = niffs_open(&fs, "trunc", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);
  TEST_CHECK_EQ(niffs_truncate(&fs, fd, len), NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);

  fd = niffs_open(&fs, "trunc", NIFFS_O_RDWR);
  TEST_CHECK_EQ(fd, ERR_NIFFS_FILE_NOT_FOUND);
  TEST_CHECK_EQ(fs.dele_pages, 1 + 2 + 2); // rewritten obj hdr + deleted page

  return TEST_RES_OK;
} TEST_END

TEST(func_rename) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);

  res = niffs_create(&fs, "orig", _NIFFS_FTYPE_FILE);
  TEST_CHECK_EQ(res,  NIFFS_OK);

  // append to empty file, three pages
  int fd = niffs_open(&fs, "orig", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);

  u32_t len = _NIFFS_SPIX_2_PDATA_LEN(&fs, 0) + _NIFFS_SPIX_2_PDATA_LEN(&fs, 1) * 2;
  u8_t *d = niffs_emul_create_data("orig", len);

  res = niffs_append(&fs, fd, d, len);
  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);

  TEST_CHECK_EQ(niffs_rename(&fs, "orig", "new"), NIFFS_OK);

  u8_t *rptr;
  u32_t rlen;
  u32_t ix = 0;

  fd = niffs_open(&fs, "new", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);
  while (ix < len) {
    res = niffs_emul_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    res = memcmp(rptr, &d[ix], rlen);
    TEST_CHECK_EQ(res,  0);
    ix += rlen;
    res = niffs_seek(&fs, fd, ix, NIFFS_SEEK_SET);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  TEST_CHECK_EQ(res,  NIFFS_OK);

  res = niffs_emul_read_ptr(&fs, fd, &rptr, &rlen);
  TEST_CHECK(res == ERR_NIFFS_END_OF_FILE);

  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
  TEST_CHECK_EQ(ix, len);

  TEST_CHECK_EQ(niffs_rename(&fs, "orig", "new2"), ERR_NIFFS_FILE_NOT_FOUND);

  res = niffs_create(&fs, "new2", _NIFFS_FTYPE_FILE);
  TEST_CHECK_EQ(res,  NIFFS_OK);

  TEST_CHECK_EQ(niffs_rename(&fs, "new", "new2"), ERR_NIFFS_NAME_CONFLICT);

  return TEST_RES_OK;
} TEST_END

TEST(func_gc) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);
  int i;
  for (i = 0; i < (fs.sectors-1) * fs.pages_per_sector; i++) {
    char fname[16];
    sprintf(fname, "t%i", i);
    res = niffs_create(&fs, fname, _NIFFS_FTYPE_FILE);
    TEST_CHECK_EQ(res,  NIFFS_OK);
    u32_t len = _NIFFS_SPIX_2_PDATA_LEN(&fs, 0);
    u8_t *data = niffs_emul_create_data(fname, len);
    int fd = niffs_open(&fs, fname, NIFFS_O_RDWR);
    TEST_CHECK(fd >= 0);
    res = niffs_append(&fs, fd, data, len);
    TEST_CHECK_EQ(res,  NIFFS_OK);
    TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
  }

  res = niffs_create(&fs, "overflow", _NIFFS_FTYPE_FILE);
  TEST_CHECK_EQ(res, ERR_NIFFS_FULL);

  TEST_CHECK_EQ(fs.free_pages, fs.pages_per_sector);
  TEST_CHECK_EQ(fs.dele_pages, 0);

  int fd = niffs_open(&fs, "t0", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);
  TEST_CHECK_EQ(niffs_truncate(&fs, fd, 0), NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);

  res = niffs_create(&fs, "overflow", _NIFFS_FTYPE_FILE);
  TEST_CHECK_EQ(res, NIFFS_OK);

  TEST_CHECK_EQ(fs.free_pages, fs.pages_per_sector);
  TEST_CHECK_EQ(fs.dele_pages, 0);

  fd = niffs_open(&fs, "t2", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);
  TEST_CHECK_EQ(niffs_truncate(&fs, fd, 0), NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
  fd = niffs_open(&fs, "t3", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);
  TEST_CHECK_EQ(niffs_truncate(&fs, fd, 0), NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
  fd = niffs_open(&fs, "t4", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);
  TEST_CHECK_EQ(niffs_truncate(&fs, fd, 0), NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);

  u32_t len = _NIFFS_SPIX_2_PDATA_LEN(&fs, 0) + _NIFFS_SPIX_2_PDATA_LEN(&fs, 1)*3 - 5;
  u8_t *data = niffs_emul_create_data("overflow", len);
  fd = niffs_open(&fs, "overflow", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);
  res = niffs_append(&fs, fd, data, len);
  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);

  TEST_CHECK_EQ(fs.free_pages, fs.pages_per_sector);
  TEST_CHECK_EQ(fs.dele_pages, 0);

  fd = niffs_open(&fs, "overflow", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);
  u32_t ix = 0;
  u8_t *rptr;
  u32_t rlen;
  while (ix < len) {
    res = niffs_emul_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    res = memcmp(rptr, &data[ix], rlen);
    TEST_CHECK_EQ(res,  0);
    ix += rlen;
    res = niffs_seek(&fs, fd, ix, NIFFS_SEEK_SET);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
  TEST_CHECK_EQ(ix, len);

  return TEST_RES_OK;
} TEST_END

TEST(func_gc_big_hog) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);

  // create one hog file two sectors big
  res = niffs_create(&fs, "bighog", _NIFFS_FTYPE_FILE);
  TEST_CHECK_EQ(res,  NIFFS_OK);
  u32_t len = _NIFFS_SPIX_2_PDATA_LEN(&fs, 0) + _NIFFS_SPIX_2_PDATA_LEN(&fs, 1) * (2*fs.pages_per_sector-1);
  u8_t *data = niffs_emul_create_data("bighog", len);
  int fd = niffs_open(&fs, "bighog", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);
  res = niffs_append(&fs, fd, data, len);
  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);

  // now create loads of short lived small files, trigger gc some times
  int needed_gc_runs_to_move_stalled_sector =
      fs.sectors *
      ((MAX(NIFFS_GC_SCORE_BUSY, -NIFFS_GC_SCORE_BUSY) * 100) / NIFFS_GC_SCORE_ERASE_CNT_DIFF);
  TEST_ASSERT(needed_gc_runs_to_move_stalled_sector > 0);

  while (needed_gc_runs_to_move_stalled_sector--) {
    int i;
    for (i = 0; i < (fs.sectors-2) * fs.pages_per_sector; i++) {
      char fname[16];
      sprintf(fname, "t%i_%i", i, needed_gc_runs_to_move_stalled_sector);
      res = niffs_create(&fs, fname, _NIFFS_FTYPE_FILE);
      TEST_CHECK_EQ(res,  NIFFS_OK);
      int fd = niffs_open(&fs, fname, NIFFS_O_RDWR);
      TEST_CHECK(fd >= 0);
      res = niffs_truncate(&fs, fd, 0);
      TEST_CHECK_EQ(res, NIFFS_OK);
      TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
    }
  }

  // make sure the big hog has been moved
  u32_t era_min, era_max;
  niffs_emul_get_sector_erase_count_info(&fs, &era_min, &era_max);
  TEST_CHECK(era_max > 1);
  TEST_CHECK(era_min > 1);
  TEST_CHECK(((era_max - era_min)*100)/era_max < 50);
  NIFFS_DBG("ERA INF min:%i max:%i span:%i\n", era_min, era_max, ((era_max - era_min)*100)/era_max);

  return TEST_RES_OK;
} TEST_END

TEST(func_gc_full) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);

  niffs_page_hdr phdr;
  phdr.flag = _NIFFS_FLAG_WRITTEN;
  phdr.id.spix = 0;

  niffs_page_ix pix;
  do {
    niffs_obj_id id;
    res = niffs_find_free_id(&fs, &id, 0);
    if (res != NIFFS_OK) break;
    phdr.id.obj_id = id;
    res = niffs_find_free_page(&fs, &pix, NIFFS_EXCL_SECT_NONE);
    if (res != NIFFS_OK) break;

    res = niffs_write_phdr(&fs, pix, &phdr);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  } while (res == NIFFS_OK);

  TEST_CHECK(res == ERR_NIFFS_NO_FREE_PAGE || res == ERR_NIFFS_NO_FREE_ID);

  res = NIFFS_unmount(&fs);
  TEST_CHECK_EQ(res,  NIFFS_OK);
  res = NIFFS_mount(&fs);
  TEST_CHECK_EQ(res,  NIFFS_OK);
  u32_t freed;
  res = niffs_gc(&fs, &freed, 1);
  TEST_CHECK_EQ(res, ERR_NIFFS_NO_GC_CANDIDATE);

  u32_t i;
  for (i = 0; i < fs.pages_per_sector; i++) {
    res = niffs_delete_page(&fs, i);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  res = NIFFS_unmount(&fs);
  TEST_CHECK_EQ(res,  NIFFS_OK);
  res = NIFFS_mount(&fs);
  TEST_CHECK_EQ(res,  NIFFS_OK);
  res = niffs_gc(&fs, &freed, 1);
  TEST_CHECK_EQ(res, NIFFS_OK);
  TEST_CHECK_GE(fs.free_pages, fs.pages_per_sector);

  return TEST_RES_OK;
} TEST_END

TEST(func_gc_long_run) {
#define TEST_CHECK_GC_LONG_RUN_FILES  10
#define TEST_CHECK_GC_LONG_RUN_RUNS   1000
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);

  u8_t *data[TEST_CHECK_GC_LONG_RUN_FILES];
  int i;
  char name[16];

  // create a bunch of test data
  u32_t max_file_system_data_size = fs.pages_per_sector * (fs.sectors - 1) * fs.page_size -
      (fs.pages_per_sector * (fs.sectors - 1) * sizeof(niffs_object_hdr));
  const u32_t perc_sizes[TEST_CHECK_GC_LONG_RUN_FILES] = {3,4,5,6,7,8,9,10,15,20};
  u32_t data_len[TEST_CHECK_GC_LONG_RUN_FILES];
  for (i = 0; i < TEST_CHECK_GC_LONG_RUN_FILES; i++) {
    sprintf(name, "name%i", i);
    data_len[i] = (perc_sizes[i] * max_file_system_data_size) / 100;
    data[i] = niffs_emul_create_data(name, data_len[i]);
  }

  srand(0x20140318);

  int runs = 0;

  u8_t created_file_map[TEST_CHECK_GC_LONG_RUN_FILES];
  memset(created_file_map, 0, sizeof(created_file_map));
  u32_t created_files = 0;
  u8_t deleting = 0;

  while (runs < TEST_CHECK_GC_LONG_RUN_RUNS) {
    if (deleting) {
      // randomly choose a file to delete
      u32_t file_to_remove_ix = rand() % created_files;
      u32_t ix = 0;
      for (i = 0; i < TEST_CHECK_GC_LONG_RUN_FILES; i++) {
        if (created_file_map[i]) {
          ix++;
          if (ix >= file_to_remove_ix) {
            sprintf(name, "name%i", i);
            int fd = niffs_open(&fs, name, NIFFS_O_RDWR);
            TEST_CHECK(fd >= 0);
            res = niffs_truncate(&fs, fd, 0);
            TEST_CHECK_EQ(res, NIFFS_OK);
            TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
            created_files--;
            created_file_map[i] = 0;
            break;
          }
        }
      }
      if (created_files <= 5) {
        deleting = 0;
      }
    } else {
      // randomly choose a file to create
      u32_t file_to_create_ix = rand() % (TEST_CHECK_GC_LONG_RUN_FILES-created_files);
      u32_t ix = 0;
      for (i = 0; i < TEST_CHECK_GC_LONG_RUN_FILES; i++) {
        if (created_file_map[i] == 0) {
          ix++;
          if (ix >= file_to_create_ix) {
            sprintf(name, "name%i", i);
            res = niffs_create(&fs, name, _NIFFS_FTYPE_FILE);
            TEST_CHECK_EQ(res, NIFFS_OK);

            int fd = niffs_open(&fs, name, NIFFS_O_RDWR);
            TEST_CHECK(fd >= 0);
            res = niffs_append(&fs, fd, data[i], data_len[i]);
            TEST_CHECK_EQ(res, NIFFS_OK);
            TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
            created_files++;
            created_file_map[i] = 1;
            break;
          }
        }
      }
      if (created_files >= 9) {
        deleting = 1;
      }
    }
    runs++;
  }

  // check current file validity
  for (i = 0; i < TEST_CHECK_GC_LONG_RUN_FILES; i++) {
    if (created_file_map[i]) {
      sprintf(name, "name%i", i);
      int fd = niffs_open(&fs, name, NIFFS_O_RDWR);
      TEST_CHECK(fd >= 0);

      u8_t *rptr;
      u32_t rlen;
      u32_t ix = 0;
      u32_t len = data_len[i];

      while (ix < len) {
        res = niffs_emul_read_ptr(&fs, fd, &rptr, &rlen);
        TEST_CHECK(res > 0);
        res = memcmp(rptr, &data[i][ix], rlen);
        TEST_CHECK_EQ(res,  0);
        ix += rlen;
        res = niffs_seek(&fs, fd, ix, NIFFS_SEEK_SET);
        TEST_CHECK_EQ(res,  NIFFS_OK);
      }

      TEST_CHECK_EQ(res,  NIFFS_OK);
      TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
      TEST_CHECK_EQ(ix, len);
    } else {
      sprintf(name, "name%i", i);
      int fd = niffs_open(&fs, name, NIFFS_O_RDWR);
      TEST_CHECK(fd == ERR_NIFFS_FILE_NOT_FOUND);
    }
  }

  // check erase counts
  u32_t era_min, era_max;
  niffs_emul_get_sector_erase_count_info(&fs, &era_min, &era_max);
  TEST_CHECK(era_max > 1);
  TEST_CHECK(era_min > 1);
  TEST_CHECK(((era_max - era_min)*100)/era_max < 50);
  NIFFS_DBG("ERA INF min:%i max:%i span:%i\n", era_min, era_max, ((era_max - era_min)*100)/era_max);

  return TEST_RES_OK;
} TEST_END

TEST(func_check_aborted_delete) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);

  // create one big file two sectors big
  res = niffs_create(&fs, "undel", _NIFFS_FTYPE_FILE);
  TEST_CHECK_EQ(res,  NIFFS_OK);
  u32_t len = _NIFFS_SPIX_2_PDATA_LEN(&fs, 0) + _NIFFS_SPIX_2_PDATA_LEN(&fs, 1) * (2*fs.pages_per_sector-1);
  u8_t *data = niffs_emul_create_data("undel", len);
  int fd = niffs_open(&fs, "undel", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);
  res = niffs_append(&fs, fd, data, len);
  TEST_CHECK_EQ(res,  NIFFS_OK);

  u32_t written_pre = fs.sectors * fs.pages_per_sector - fs.free_pages - fs.dele_pages;

  // create one small file two pages big
  res = niffs_create(&fs, "noorphan", _NIFFS_FTYPE_FILE);
  TEST_CHECK_EQ(res,  NIFFS_OK);
  len = _NIFFS_SPIX_2_PDATA_LEN(&fs, 0) + _NIFFS_SPIX_2_PDATA_LEN(&fs, 1);
  data = niffs_emul_create_data("noorphan", len);
  fd = niffs_open(&fs, "noorphan", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);
  res = niffs_append(&fs, fd, data, len);
  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);

  // truncate header size for file to 0
  fd = niffs_open(&fs, "undel", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);
  niffs_emul_set_write_byte_limit(sizeof(u32_t)); // length in obj header
  res = niffs_truncate(&fs, fd, 0);
  TEST_CHECK_EQ(res, ERR_NIFFS_TEST_ABORTED_WRITE);

  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);

  TEST_CHECK_EQ(NIFFS_unmount(&fs), NIFFS_OK);

  TEST_CHECK_EQ(niffs_chk(&fs), NIFFS_OK);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);
  TEST_CHECK_EQ(written_pre, fs.dele_pages);
  TEST_CHECK_EQ(NIFFS_unmount(&fs), NIFFS_OK);

  return TEST_RES_OK;
} TEST_END

TEST(func_check_orphans) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);

  // create one big file two sectors big
  res = niffs_create(&fs, "orphan", _NIFFS_FTYPE_FILE);
  TEST_CHECK_EQ(res,  NIFFS_OK);
  u32_t len = _NIFFS_SPIX_2_PDATA_LEN(&fs, 0) + _NIFFS_SPIX_2_PDATA_LEN(&fs, 1) * (2*fs.pages_per_sector-1);
  u8_t *data = niffs_emul_create_data("orphan", len);
  int fd = niffs_open(&fs, "orphan", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);
  res = niffs_append(&fs, fd, data, len);
  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);

  u32_t written_pre = fs.sectors * fs.pages_per_sector - fs.free_pages - fs.dele_pages;

  // create one small file two pages big
  res = niffs_create(&fs, "noorphan", _NIFFS_FTYPE_FILE);
  TEST_CHECK_EQ(res,  NIFFS_OK);
  len = _NIFFS_SPIX_2_PDATA_LEN(&fs, 0) + _NIFFS_SPIX_2_PDATA_LEN(&fs, 1);
  data = niffs_emul_create_data("noorphan", len);
  fd = niffs_open(&fs, "noorphan", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);
  res = niffs_append(&fs, fd, data, len);
  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);

  // delete obj header for orphan file
  TEST_CHECK_EQ(niffs_delete_page(&fs, 0), NIFFS_OK);

  TEST_CHECK_EQ(NIFFS_unmount(&fs), NIFFS_OK);

  TEST_CHECK_EQ(niffs_chk(&fs), NIFFS_OK);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);
  TEST_CHECK_EQ(written_pre, fs.dele_pages);
  TEST_CHECK_EQ(NIFFS_unmount(&fs), NIFFS_OK);

  return TEST_RES_OK;
} TEST_END

TEST(func_check_aborted_append) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);

  // create file
  res = niffs_create(&fs, "abortapp", _NIFFS_FTYPE_FILE);
  TEST_CHECK_EQ(res,  NIFFS_OK);
  u32_t orig_len = _NIFFS_SPIX_2_PDATA_LEN(&fs, 0);
  u8_t *orig_data = niffs_emul_create_data("abortapp", orig_len);
  int fd = niffs_open(&fs, "abortapp", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);
  res = niffs_append(&fs, fd, orig_data, orig_len);
  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);

  // append to file, abort
  u32_t len = _NIFFS_SPIX_2_PDATA_LEN(&fs, 1) * fs.pages_per_sector;
  u8_t *data = niffs_emul_create_data("abortapp_more", len);
  fd = niffs_open(&fs, "abortapp", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);
  niffs_emul_set_write_byte_limit(len-4);
  res = niffs_append(&fs, fd, data, len);
  TEST_CHECK_EQ(res, ERR_NIFFS_TEST_ABORTED_WRITE);

  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);

  TEST_CHECK_EQ(res, ERR_NIFFS_TEST_ABORTED_WRITE);
  TEST_CHECK_EQ(NIFFS_unmount(&fs), NIFFS_OK);
  TEST_CHECK_EQ(niffs_chk(&fs), NIFFS_OK);

  u8_t *rptr;
  u32_t rlen;
  u32_t ix = 0;

  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);
  fd = niffs_open(&fs, "abortapp", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);
  TEST_CHECK_EQ(niffs_seek(&fs, fd, 0, NIFFS_SEEK_SET), NIFFS_OK);
  while (ix < orig_len) {
    res = niffs_emul_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    res = memcmp(rptr, &orig_data[ix], rlen);
    TEST_CHECK_EQ(res,  0);
    ix += rlen;
    res = niffs_seek(&fs, fd, ix, NIFFS_SEEK_SET);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
  TEST_CHECK_EQ(ix, orig_len);


  return TEST_RES_OK;
} TEST_END

TEST(func_check_aborted_modify) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);

  // create file
  res = niffs_create(&fs, "abortapp", _NIFFS_FTYPE_FILE);
  TEST_CHECK_EQ(res,  NIFFS_OK);
  u32_t orig_len = _NIFFS_SPIX_2_PDATA_LEN(&fs, 0) + _NIFFS_SPIX_2_PDATA_LEN(&fs, 1) * fs.pages_per_sector;
  u8_t *orig_data = niffs_emul_create_data("abortapp", orig_len);
  int fd = niffs_open(&fs, "abortapp", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);
  res = niffs_append(&fs, fd, orig_data, orig_len);
  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);

  // modify file, abort
  u32_t mod_len = orig_len / 2;
  u8_t *mod_data = niffs_emul_create_data("abortapp_more", mod_len);
  fd = niffs_open(&fs, "abortapp", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);
  niffs_emul_set_write_byte_limit(mod_len/2);
  res = niffs_modify(&fs, fd,  orig_len / 4, mod_data, mod_len);
  TEST_CHECK_EQ(res, ERR_NIFFS_TEST_ABORTED_WRITE);

  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);

  TEST_CHECK_EQ(res, ERR_NIFFS_TEST_ABORTED_WRITE);
  TEST_CHECK_EQ(NIFFS_unmount(&fs), NIFFS_OK);
  TEST_CHECK_EQ(niffs_chk(&fs), NIFFS_OK);

  u8_t *rptr;
  u32_t rlen;
  u32_t ix = 0;

  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);
  fd = niffs_open(&fs, "abortapp", NIFFS_O_RDWR);
  TEST_CHECK(fd >= 0);
  TEST_CHECK_EQ(niffs_seek(&fs, fd, 0, NIFFS_SEEK_SET), NIFFS_OK);
  while (ix < orig_len) {
    res = niffs_emul_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    ix += rlen;
    res = niffs_seek(&fs, fd, ix, NIFFS_SEEK_SET);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
  TEST_CHECK_EQ(ix, orig_len);


  return TEST_RES_OK;
} TEST_END

TEST(func_check_aborted_erase) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(res, NIFFS_OK);

  niffs_emul_set_write_byte_limit(1);
  res = NIFFS_format(&fs);
  TEST_CHECK_EQ(res, ERR_NIFFS_TEST_ABORTED_WRITE);

  TEST_CHECK_EQ(niffs_chk(&fs), NIFFS_OK);

  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);

  return TEST_RES_OK;
} TEST_END


SUITE_TESTS(niffs_func_tests)
  ADD_TEST(func_dump)
  ADD_TEST(func_info)
  ADD_TEST(func_init_virgin)
  ADD_TEST(func_format_virgin)
  ADD_TEST(func_mount_clean)
  ADD_TEST(func_mount_scrap)
  ADD_TEST(func_find_free_page)
  ADD_TEST(func_write_phdr)
  ADD_TEST(func_write_phdr_fill)
  ADD_TEST(func_creat)
  ADD_TEST(func_creat_full)
  ADD_TEST(func_fd)
  ADD_TEST(func_delete)
  ADD_TEST(func_move)
  ADD_TEST(func_open)
  ADD_TEST(func_append_read)
  ADD_TEST(func_modify_ohdr)
  ADD_TEST(func_modify_page)
  ADD_TEST(func_modify_pagespan)
  ADD_TEST(func_modify_pagespan_nobreak)
  ADD_TEST(func_modify_beyond)
  ADD_TEST(func_truncate)
  ADD_TEST(func_rename)
  ADD_TEST(func_gc)
  ADD_TEST(func_gc_big_hog)
  ADD_TEST(func_gc_full)
  ADD_TEST(func_gc_long_run)
  ADD_TEST(func_check_aborted_delete)
  ADD_TEST(func_check_orphans)
  ADD_TEST(func_check_aborted_append)
  ADD_TEST(func_check_aborted_modify)
  ADD_TEST(func_check_aborted_erase)
SUITE_END(niffs_func_tests)
