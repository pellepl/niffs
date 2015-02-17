/*
 * niffs_func_tests.c
 *
 *  Created on: Feb 3, 2015
 *      Author: petera
 */

#include "testrunner.h"
#include "niffs_test_emul.h"

SUITE(niffs_func_tests)

void setup(test *t) {
  (void)niffs_emul_init();
}

void teardown(test *t) {
  niffs_emul_destroy_all_data();
}

#ifdef NIFFS_DUMP
TEST(func_dump) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(res, NIFFS_OK);
  NIFFS_dump(&fs);
  return TEST_RES_OK;
} TEST_END(func_dump)
#endif

TEST(func_init_virgin) {
  return TEST_RES_OK;
} TEST_END(func_init_virgin)

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
} TEST_END(func_format_virgin)

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
} TEST_END(func_find_free_page)

TEST(func_write_phdr) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);

  niffs_page_hdr phdr;
  phdr.flag = _NIFFS_FLAG_WRITTEN;
  phdr.id.cycle_ix = 0;
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
} TEST_END(func_write_phdr)

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
} TEST_END(func_write_phdr_fill)

TEST(func_creat) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);

  TEST_CHECK_EQ(niffs_create(&fs, 0), ERR_NIFFS_NULL_PTR);
  TEST_CHECK_EQ(niffs_create(&fs, "test"), NIFFS_OK);
  TEST_CHECK_EQ(niffs_create(&fs, "test"), ERR_NIFFS_NAME_CONFLICT);

  return TEST_RES_OK;
} TEST_END(func_creat)

TEST(func_creat_full) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);

  int i;
  for (i = 0; i < (fs.sectors-1) * fs.pages_per_sector; i++) {
    char fname[16];
    sprintf(fname, "t%i", i);
    res = niffs_create(&fs, fname);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  res = niffs_create(&fs, "overflow");
  TEST_CHECK_EQ(res,  ERR_NIFFS_FULL);

  return TEST_RES_OK;
} TEST_END(func_creat_full)

TEST(func_delete) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);
  TEST_CHECK_EQ(niffs_delete_page(&fs, 0), ERR_NIFFS_DELETING_FREE_PAGE);
  TEST_CHECK_EQ(niffs_create(&fs, "moo"), NIFFS_OK);
  TEST_CHECK_EQ(niffs_delete_page(&fs, 0), NIFFS_OK);
  TEST_CHECK_EQ(niffs_delete_page(&fs, 0), ERR_NIFFS_DELETING_DELETED_PAGE);

  return TEST_RES_OK;
} TEST_END(func_delete)

TEST(func_move) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);
  TEST_CHECK_EQ(niffs_move_page(&fs, 0, 0, 0, 0, NIFFS_FLAG_MOVE_KEEP), ERR_NIFFS_MOVING_TO_SAME_PAGE);
  TEST_CHECK_EQ(niffs_move_page(&fs, 0, 1, 0, 0, NIFFS_FLAG_MOVE_KEEP), ERR_NIFFS_MOVING_FREE_PAGE);
  TEST_CHECK_EQ(niffs_create(&fs, "moo"), NIFFS_OK);
  TEST_CHECK_EQ(niffs_move_page(&fs, 0, 1, 0, 0, NIFFS_FLAG_MOVE_KEEP), NIFFS_OK);
  TEST_CHECK_EQ(niffs_move_page(&fs, 0, 1, 0, 0, NIFFS_FLAG_MOVE_KEEP), ERR_NIFFS_MOVING_DELETED_PAGE);
  TEST_CHECK_EQ(niffs_move_page(&fs, 1, 0, 0, 0, NIFFS_FLAG_MOVE_KEEP), ERR_NIFFS_MOVING_TO_UNFREE_PAGE);

  return TEST_RES_OK;
} TEST_END(func_move)

TEST(func_open) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);

  int fd = niffs_open(&fs, "test");
  TEST_CHECK_EQ(fd, ERR_NIFFS_FILE_NOT_FOUND);

  res = niffs_create(&fs, "test");
  TEST_CHECK_EQ(res,  NIFFS_OK);

  fd = niffs_open(&fs, "test");
  TEST_CHECK(fd >= 0);

  return TEST_RES_OK;
} TEST_END(func_open)

TEST(func_append_read) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);

  u32_t free_pages_clean = fs.free_pages;

  res = niffs_create(&fs, "test");
  TEST_CHECK_EQ(res,  NIFFS_OK);

  // append to empty file, almost a page
  int fd = niffs_open(&fs, "test");
  TEST_CHECK(fd >= 0);

  u32_t len = _NIFFS_SPIX_2_PDATA_LEN(&fs, 0)-4;
  u8_t *d = niffs_emul_create_data("test", len);
  res = niffs_append(&fs, fd, d, len);
  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);

  fd = niffs_open(&fs, "test");
  TEST_CHECK(fd >= 0);

  u8_t *rptr;
  u32_t rlen;
  u32_t ix = 0;

  while (ix < len) {
    res = niffs_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    res = memcmp(rptr, &d[ix], rlen);
    TEST_CHECK_EQ(res,  0);
    ix += rlen;
    res = niffs_seek(&fs, fd, NIFFS_SEEK_SET, ix);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
  TEST_CHECK_EQ(ix, len);
  TEST_CHECK_EQ(free_pages_clean - fs.free_pages, 1);
  TEST_CHECK_EQ(fs.dele_pages, 0);

  // append one page file, rest of page
  fd = niffs_open(&fs, "test");
  TEST_CHECK(fd >= 0);

  u32_t len2 = 4;
  u8_t *d2 = niffs_emul_create_data("test_rest", len2);
  res = niffs_append(&fs, fd, d2, len2);
  TEST_CHECK_EQ(res,  NIFFS_OK);

  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);

  fd = niffs_open(&fs, "test");
  TEST_CHECK(fd >= 0);

  ix = 0;

  while (ix < len) {
    res = niffs_read_ptr(&fs, fd, &rptr, &rlen);
    u32_t alen = MIN(rlen, len-ix);
    TEST_CHECK(res > 0);
    res = memcmp(rptr, &d[ix], alen);
    TEST_CHECK_EQ(res,  0);
    ix += alen;
    res = niffs_seek(&fs, fd, NIFFS_SEEK_SET, ix);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  while (ix < len + len2) {
    res = niffs_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    u32_t alen = MIN(rlen, len-(ix-len));
    res = memcmp(rptr, &d2[ix-len], alen);
    TEST_CHECK_EQ(res,  0);
    ix += alen;
    res = niffs_seek(&fs, fd, NIFFS_SEEK_SET, ix);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
  TEST_CHECK_EQ(ix, len+len2);
  TEST_CHECK_EQ(free_pages_clean - fs.free_pages, 1+1);
  TEST_CHECK_EQ(fs.dele_pages, 1);

  // append one and a half pages
  fd = niffs_open(&fs, "test");
  TEST_CHECK(fd >= 0);

  u32_t len3 = _NIFFS_SPIX_2_PDATA_LEN(&fs, 1) + _NIFFS_SPIX_2_PDATA_LEN(&fs, 1)/2;
  u8_t *d3 = niffs_emul_create_data("test_1_5_more", len3);
  res = niffs_append(&fs, fd, d3, len3);
  TEST_CHECK_EQ(res,  NIFFS_OK);

  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);

  fd = niffs_open(&fs, "test");
  TEST_CHECK(fd >= 0);

  ix = 0;

  while (ix < len) {
    res = niffs_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    u32_t alen = MIN(rlen, len-ix);
    res = memcmp(rptr, &d[ix], alen);
    TEST_CHECK_EQ(res,  0);
    ix += alen;
    res = niffs_seek(&fs, fd, NIFFS_SEEK_SET, ix);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  while (ix < len + len2) {
    res = niffs_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    u32_t alen = MIN(rlen, len2-(ix-len));
    res = memcmp(rptr, &d2[ix-len], alen);
    TEST_CHECK_EQ(res,  0);
    ix += alen;
    res = niffs_seek(&fs, fd, NIFFS_SEEK_SET, ix);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  while (ix < len + len2 + len3) {
    res = niffs_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    u32_t alen = MIN(rlen, len3-(ix-len-len2));
    res = memcmp(rptr, &d3[ix-len-len2], alen);
    TEST_CHECK_EQ(res,  0);
    ix += alen;
    res = niffs_seek(&fs, fd, NIFFS_SEEK_SET, ix);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
  TEST_CHECK_EQ(ix, len+len2+len3);
  TEST_CHECK_EQ(free_pages_clean - fs.free_pages, 1+1+3);
  TEST_CHECK_EQ(fs.dele_pages, 2);

  // append just a little more data
  fd = niffs_open(&fs, "test");
  TEST_CHECK(fd >= 0);

  u32_t len4 = 8;
  u8_t *d4 = niffs_emul_create_data("littlemore", len4);
  res = niffs_append(&fs, fd, d4, len4);
  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);

  fd = niffs_open(&fs, "test");
  TEST_CHECK(fd >= 0);

  ix = 0;

  while (ix < len) {
     res = niffs_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    u32_t alen = MIN(rlen, len-ix);
    res = memcmp(rptr, &d[ix], alen);
    TEST_CHECK_EQ(res,  0);
    ix += alen;
    res = niffs_seek(&fs, fd, NIFFS_SEEK_SET, ix);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  while (ix < len + len2) {
    res = niffs_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    u32_t alen = MIN(rlen, len2-(ix-len));
     res = memcmp(rptr, &d2[ix-len], alen);
    TEST_CHECK_EQ(res,  0);
    ix += alen;
    res = niffs_seek(&fs, fd, NIFFS_SEEK_SET, ix);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  while (ix < len + len2 + len3) {
    res = niffs_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    u32_t alen = MIN(rlen, len3-(ix-len-len2));
    res = memcmp(rptr, &d3[ix-len-len2], alen);
    TEST_CHECK_EQ(res,  0);
    ix += alen;
    res = niffs_seek(&fs, fd, NIFFS_SEEK_SET, ix);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  while (ix < len + len2 + len3 + len4) {
     res = niffs_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    u32_t alen = MIN(rlen, len4-(ix-len-len2-len3));
    res = memcmp(rptr, &d4[ix-len-len2-len3], alen);
    TEST_CHECK_EQ(res,  0);
    ix += alen;
    res = niffs_seek(&fs, fd, NIFFS_SEEK_SET, ix);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
  TEST_CHECK_EQ(ix, len+len2+len3+len4);
  TEST_CHECK_EQ(fs.dele_pages, 4);
  TEST_CHECK_EQ(free_pages_clean - fs.free_pages, 1+1+3+2);

  return TEST_RES_OK;
} TEST_END(func_append_read)

TEST(func_modify_ohdr) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);

  res = niffs_create(&fs, "modify");
  TEST_CHECK_EQ(res,  NIFFS_OK);

  // append to empty file, one page
  int fd = niffs_open(&fs, "modify");
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

  fd = niffs_open(&fs, "modify");
  TEST_CHECK(fd >= 0);

  res = niffs_modify(&fs, fd, b_ix, dm, e_ix - b_ix);
  TEST_CHECK_EQ(res,  0);

  u8_t *rptr;
  u32_t rlen;
  u32_t ix = 0;

  TEST_CHECK_EQ(niffs_seek(&fs, fd, NIFFS_SEEK_SET, 0), NIFFS_OK);
  while (ix < len) {
    res = niffs_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    res = memcmp(rptr, &d[ix], rlen);
    TEST_CHECK_EQ(res,  0);
    ix += rlen;
    res = niffs_seek(&fs, fd, NIFFS_SEEK_SET, ix);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
  TEST_CHECK_EQ(ix, len);


  return TEST_RES_OK;
} TEST_END(func_modify_ohdr)

TEST(func_modify_page) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);

  res = niffs_create(&fs, "modify");
  TEST_CHECK_EQ(res,  NIFFS_OK);

  // append to empty file, two pages
  int fd = niffs_open(&fs, "modify");
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

  fd = niffs_open(&fs, "modify");
  TEST_CHECK(fd >= 0);

  res = niffs_modify(&fs, fd, b_ix, dm, e_ix - b_ix);
  TEST_CHECK_EQ(res,  0);

  u8_t *rptr;
  u32_t rlen;
  u32_t ix = 0;

  TEST_CHECK_EQ(niffs_seek(&fs, fd, NIFFS_SEEK_SET, 0), NIFFS_OK);
  while (ix < len) {
    res = niffs_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    res = memcmp(rptr, &d[ix], rlen);
    TEST_CHECK_EQ(res,  0);
    ix += rlen;
    res = niffs_seek(&fs, fd, NIFFS_SEEK_SET, ix);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
  TEST_CHECK_EQ(ix, len);


  return TEST_RES_OK;
} TEST_END(func_modify_page)

TEST(func_modify_pagespan) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);

  res = niffs_create(&fs, "modify");
  TEST_CHECK_EQ(res,  NIFFS_OK);

  // append to empty file, two pages
  int fd = niffs_open(&fs, "modify");
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

  fd = niffs_open(&fs, "modify");
  TEST_CHECK(fd >= 0);

  res = niffs_modify(&fs, fd, b_ix, dm, e_ix - b_ix);
  TEST_CHECK_EQ(res,  0);

  u8_t *rptr;
  u32_t rlen;
  u32_t ix = 0;

  TEST_CHECK_EQ(niffs_seek(&fs, fd, NIFFS_SEEK_SET, 0), NIFFS_OK);
  while (ix < len) {
    res = niffs_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    res = memcmp(rptr, &d[ix], rlen);
    TEST_CHECK_EQ(res,  0);
    ix += rlen;
    res = niffs_seek(&fs, fd, NIFFS_SEEK_SET, ix);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
  TEST_CHECK_EQ(ix, len);


  return TEST_RES_OK;
} TEST_END(func_modify_pagespan)

TEST(func_modify_pagespan_nobreak) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);

  res = niffs_create(&fs, "modify");
  TEST_CHECK_EQ(res,  NIFFS_OK);

  // append to empty file, four pages
  int fd = niffs_open(&fs, "modify");
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

  fd = niffs_open(&fs, "modify");
  TEST_CHECK(fd >= 0);

  res = niffs_modify(&fs, fd, b_ix, dm, e_ix - b_ix);
  TEST_CHECK_EQ(res,  0);

  u8_t *rptr;
  u32_t rlen;
  u32_t ix = 0;

  TEST_CHECK_EQ(niffs_seek(&fs, fd, NIFFS_SEEK_SET, 0), NIFFS_OK);
  while (ix < len) {
    res = niffs_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    res = memcmp(rptr, &d[ix], rlen);
    TEST_CHECK_EQ(res,  0);
    ix += rlen;
    res = niffs_seek(&fs, fd, NIFFS_SEEK_SET, ix);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
  TEST_CHECK_EQ(ix, len);


  return TEST_RES_OK;
} TEST_END(func_modify_pagespan_nobreak)

TEST(func_modify_beyond) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);

  res = niffs_create(&fs, "modify");
  TEST_CHECK_EQ(res,  NIFFS_OK);

  // append to empty file, one page
  int fd = niffs_open(&fs, "modify");
  TEST_CHECK(fd >= 0);

  u32_t len = _NIFFS_SPIX_2_PDATA_LEN(&fs, 0);
  u8_t *d = niffs_emul_create_data("modify", len);

  res = niffs_modify(&fs, fd, 0, d, len);
  TEST_CHECK_EQ(res, ERR_NIFFS_MODIFY_BEYOND_FILE);

  res = niffs_append(&fs, fd, d, len);
  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);

  fd = niffs_open(&fs, "modify");
  TEST_CHECK(fd >= 0);

  u8_t *dm = niffs_emul_create_data("modified", len);

  res = niffs_modify(&fs, fd, 4, dm, len);
  TEST_CHECK_EQ(res, ERR_NIFFS_MODIFY_BEYOND_FILE);

  // modify midst of ohdr page

  fd = niffs_open(&fs, "modify");
  TEST_CHECK(fd >= 0);

  u8_t *rptr;
  u32_t rlen;
  u32_t ix = 0;

  while (ix < len) {
    res = niffs_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    res = memcmp(rptr, &d[ix], rlen);
    TEST_CHECK_EQ(res,  0);
    ix += rlen;
    res = niffs_seek(&fs, fd, NIFFS_SEEK_SET, ix);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
  TEST_CHECK_EQ(ix, len);


  return TEST_RES_OK;
} TEST_END(func_modify_beyond)

TEST(func_truncate) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);

  res = niffs_create(&fs, "trunc");
  TEST_CHECK_EQ(res,  NIFFS_OK);

  // append to empty file, three pages
  int fd = niffs_open(&fs, "trunc");
  TEST_CHECK(fd >= 0);

  u32_t len = _NIFFS_SPIX_2_PDATA_LEN(&fs, 0) + _NIFFS_SPIX_2_PDATA_LEN(&fs, 1) * 2;
  u8_t *d = niffs_emul_create_data("trunc", len);

  res = niffs_append(&fs, fd, d, len);
  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);


  u8_t *rptr;
  u32_t rlen;
  u32_t ix = 0;

  fd = niffs_open(&fs, "trunc");
  TEST_CHECK(fd >= 0);
  while (ix < len) {
    res = niffs_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    res = memcmp(rptr, &d[ix], rlen);
    TEST_CHECK_EQ(res,  0);
    ix += rlen;
    res = niffs_seek(&fs, fd, NIFFS_SEEK_SET, ix);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
  TEST_CHECK_EQ(ix, len);
  TEST_CHECK_EQ(fs.dele_pages, 0);

  // truncate half a page

  len -= _NIFFS_SPIX_2_PDATA_LEN(&fs, 1) / 2;
  fd = niffs_open(&fs, "trunc");
  TEST_CHECK(fd >= 0);
  TEST_CHECK_EQ(niffs_truncate(&fs, fd, len), NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);

  fd = niffs_open(&fs, "trunc");
  TEST_CHECK(fd >= 0);
  ix = 0;
  while (ix < len) {
    res = niffs_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    res = memcmp(rptr, &d[ix], rlen);
    TEST_CHECK_EQ(res,  0);
    ix += rlen;
    res = niffs_seek(&fs, fd, NIFFS_SEEK_SET, ix);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
  TEST_CHECK_EQ(ix, len);
  TEST_CHECK_EQ(fs.dele_pages, 1); // rewritten obj hdr

  // truncate to two pages

  len = _NIFFS_SPIX_2_PDATA_LEN(&fs, 0) * 2;
  fd = niffs_open(&fs, "trunc");
  TEST_CHECK(fd >= 0);
  TEST_CHECK_EQ(niffs_truncate(&fs, fd, len), NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);

  fd = niffs_open(&fs, "trunc");
  TEST_CHECK(fd >= 0);
  ix = 0;
  while (ix < len) {
    res = niffs_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    res = memcmp(rptr, &d[ix], rlen);
    TEST_CHECK_EQ(res,  0);
    ix += rlen;
    res = niffs_seek(&fs, fd, NIFFS_SEEK_SET, ix);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
  TEST_CHECK_EQ(ix, len);
  TEST_CHECK_EQ(fs.dele_pages, 1 + 2); // rewritten obj hdr + deleted page

  // truncate, rm

  len = 0;
  fd = niffs_open(&fs, "trunc");
  TEST_CHECK(fd >= 0);
  TEST_CHECK_EQ(niffs_truncate(&fs, fd, len), NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);

  fd = niffs_open(&fs, "trunc");
  TEST_CHECK_EQ(fd, ERR_NIFFS_FILE_NOT_FOUND);
  TEST_CHECK_EQ(fs.dele_pages, 1 + 2 + 2); // rewritten obj hdr + deleted page

  return TEST_RES_OK;
} TEST_END(func_truncate)

TEST(func_gc) {
  int res = NIFFS_format(&fs);
  TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);
  int i;
  for (i = 0; i < (fs.sectors-1) * fs.pages_per_sector; i++) {
    char fname[16];
    sprintf(fname, "t%i", i);
    res = niffs_create(&fs, fname);
    TEST_CHECK_EQ(res,  NIFFS_OK);
    u32_t len = _NIFFS_SPIX_2_PDATA_LEN(&fs, 0);
    u8_t *data = niffs_emul_create_data(fname, len);
    int fd = niffs_open(&fs, fname);
    TEST_CHECK(fd >= 0);
    res = niffs_append(&fs, fd, data, len);
    TEST_CHECK_EQ(res,  NIFFS_OK);
    TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
  }

  res = niffs_create(&fs, "overflow");
  TEST_CHECK_EQ(res, ERR_NIFFS_FULL);

  TEST_CHECK_EQ(fs.free_pages, fs.pages_per_sector);
  TEST_CHECK_EQ(fs.dele_pages, 0);

  int fd = niffs_open(&fs, "t0");
  TEST_CHECK(fd >= 0);
  TEST_CHECK_EQ(niffs_truncate(&fs, fd, 0), NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);

  res = niffs_create(&fs, "overflow");
  TEST_CHECK_EQ(res, NIFFS_OK);

  TEST_CHECK_EQ(fs.free_pages, fs.pages_per_sector);
  TEST_CHECK_EQ(fs.dele_pages, 0);

  fd = niffs_open(&fs, "t2");
  TEST_CHECK(fd >= 0);
  TEST_CHECK_EQ(niffs_truncate(&fs, fd, 0), NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
  fd = niffs_open(&fs, "t3");
  TEST_CHECK(fd >= 0);
  TEST_CHECK_EQ(niffs_truncate(&fs, fd, 0), NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
  fd = niffs_open(&fs, "t4");
  TEST_CHECK(fd >= 0);
  TEST_CHECK_EQ(niffs_truncate(&fs, fd, 0), NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);

  u32_t len = _NIFFS_SPIX_2_PDATA_LEN(&fs, 0) + _NIFFS_SPIX_2_PDATA_LEN(&fs, 1)*3 - 5;
  u8_t *data = niffs_emul_create_data("overflow", len);
  fd = niffs_open(&fs, "overflow");
  TEST_CHECK(fd >= 0);
  res = niffs_append(&fs, fd, data, len);
  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);

  TEST_CHECK_EQ(fs.free_pages, fs.pages_per_sector);
  TEST_CHECK_EQ(fs.dele_pages, 0);

  fd = niffs_open(&fs, "overflow");
  TEST_CHECK(fd >= 0);
  u32_t ix = 0;
  u8_t *rptr;
  u32_t rlen;
  while (ix < len) {
    res = niffs_read_ptr(&fs, fd, &rptr, &rlen);
    TEST_CHECK(res > 0);
    res = memcmp(rptr, &data[ix], rlen);
    TEST_CHECK_EQ(res,  0);
    ix += rlen;
    res = niffs_seek(&fs, fd, NIFFS_SEEK_SET, ix);
    TEST_CHECK_EQ(res,  NIFFS_OK);
  }

  TEST_CHECK_EQ(res,  NIFFS_OK);
  TEST_CHECK_EQ(niffs_close(&fs, fd), NIFFS_OK);
  TEST_CHECK_EQ(ix, len);

  return TEST_RES_OK;
} TEST_END(func_gc)

SUITE_END(niffs_func_tests)
