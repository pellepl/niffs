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
  TEST_CHECK(res == NIFFS_OK);
  NIFFS_dump(&fs);
  return TEST_RES_OK;
} TEST_END(func_dump)
#endif

TEST(func_init_virgin) {
  return TEST_RES_OK;
} TEST_END(func_init_virgin)

TEST(func_format_virgin) {
  int res = NIFFS_format(&fs);
  TEST_CHECK(res == NIFFS_OK);
  TEST_CHECK(NIFFS_mount(&fs) == NIFFS_OK);

  niffs_page_ix pix;
  res = niffs_find_free_page(&fs, &pix, NIFFS_EXCL_SECT_NONE);
  TEST_CHECK(res == NIFFS_OK);
  TEST_CHECK(pix == 0);

  niffs_obj_id id;
  res = niffs_find_free_id(&fs, &id, 0);
  TEST_CHECK(res == NIFFS_OK);
  TEST_CHECK(id == 1);

  return TEST_RES_OK;
} TEST_END(func_format_virgin)

TEST(func_find_free_page) {
  int res = NIFFS_format(&fs);
  TEST_CHECK(res == NIFFS_OK);
  TEST_CHECK(NIFFS_mount(&fs) == NIFFS_OK);

  u32_t s;
  for (s = 0; s < fs.sectors; s++) {
    niffs_page_ix pix;
    res = niffs_find_free_page(&fs, &pix, s);
    TEST_CHECK(res == NIFFS_OK);
    TEST_CHECK(s != _NIFFS_PIX_2_SECTOR(&fs, pix));
  }

  return TEST_RES_OK;
} TEST_END(func_find_free_page)

TEST(func_write_phdr) {
  int res = NIFFS_format(&fs);
  TEST_CHECK(NIFFS_mount(&fs) == NIFFS_OK);

  niffs_page_hdr phdr;
  phdr.flag = _NIFFS_FLAG_WRITTEN;
  phdr.id.cycle_ix = 0;
  phdr.id.spix = 0;

  niffs_page_ix pix = 0;
  phdr.id.obj_id = 0;
  res = niffs_write_phdr(&fs, pix, &phdr);
  TEST_CHECK(res == ERR_NIFFS_WR_PHDR_BAD_ID);
  phdr.id.obj_id = (niffs_obj_id)-1;
  res = niffs_write_phdr(&fs, pix, &phdr);
  TEST_CHECK(res == ERR_NIFFS_WR_PHDR_BAD_ID);

  phdr.id.obj_id = 1;
  res = niffs_write_phdr(&fs, pix, &phdr);
  TEST_CHECK(res == NIFFS_OK);

  res = niffs_write_phdr(&fs, pix, &phdr);
  TEST_CHECK(res == ERR_NIFFS_WR_PHDR_UNFREE_PAGE);

  return TEST_RES_OK;
} TEST_END(func_write_phdr)

TEST(func_write_phdr_fill) {
  int res = NIFFS_format(&fs);
  TEST_CHECK(NIFFS_mount(&fs) == NIFFS_OK);

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
    TEST_CHECK(res == NIFFS_OK);
  } while (res == NIFFS_OK);

  TEST_CHECK(res == ERR_NIFFS_NO_FREE_PAGE || res == ERR_NIFFS_NO_FREE_ID);

  return TEST_RES_OK;
} TEST_END(func_write_phdr_fill)

TEST(func_creat) {
  int res = NIFFS_format(&fs);
  TEST_CHECK(NIFFS_mount(&fs) == NIFFS_OK);

  TEST_CHECK(niffs_create(&fs, 0) == ERR_NIFFS_NULL_PTR);
  TEST_CHECK(niffs_create(&fs, "test") == NIFFS_OK);
  TEST_CHECK(niffs_create(&fs, "test") == ERR_NIFFS_NAME_CONFLICT);

  return TEST_RES_OK;
} TEST_END(func_creat)

TEST(func_creat_full) {
  int res = NIFFS_format(&fs);
  TEST_CHECK(NIFFS_mount(&fs) == NIFFS_OK);

  int i;
  for (i = 0; i < (fs.sectors-1) * fs.pages_per_sector; i++) {
    char fname[16];
    sprintf(fname, "t%i", i);
    res = niffs_create(&fs, fname);
    TEST_CHECK(res == NIFFS_OK);
  }

  res = niffs_create(&fs, "overflow");
  TEST_CHECK(res == ERR_NIFFS_FULL);

  return TEST_RES_OK;
} TEST_END(func_creat_full)

TEST(func_delete_basic) {
  int res = NIFFS_format(&fs);
  TEST_CHECK(NIFFS_mount(&fs) == NIFFS_OK);
  TEST_CHECK(niffs_delete_page(&fs, 0) == ERR_NIFFS_DELETING_FREE_PAGE);
  TEST_CHECK(niffs_create(&fs, "moo") == NIFFS_OK);
  TEST_CHECK(niffs_delete_page(&fs, 0) == NIFFS_OK);
  TEST_CHECK(niffs_delete_page(&fs, 0) == ERR_NIFFS_DELETING_DELETED_PAGE);

  return TEST_RES_OK;
} TEST_END(func_delete_basic)

TEST(func_move_basic) {
  int res = NIFFS_format(&fs);
  TEST_CHECK(NIFFS_mount(&fs) == NIFFS_OK);
  TEST_CHECK(niffs_move_page(&fs, 0, 0, 0, 0) == ERR_NIFFS_MOVING_TO_SAME_PAGE);
  TEST_CHECK(niffs_move_page(&fs, 0, 1, 0, 0) == ERR_NIFFS_MOVING_FREE_PAGE);
  TEST_CHECK(niffs_create(&fs, "moo") == NIFFS_OK);
  TEST_CHECK(niffs_move_page(&fs, 0, 1, 0, 0) == NIFFS_OK);
  TEST_CHECK(niffs_move_page(&fs, 0, 1, 0, 0) == ERR_NIFFS_MOVING_DELETED_PAGE);
  TEST_CHECK(niffs_move_page(&fs, 1, 0, 0, 0) == ERR_NIFFS_MOVING_TO_UNFREE_PAGE);

  return TEST_RES_OK;
} TEST_END(func_move_basic)

TEST(func_open_basic) {
  int res = NIFFS_format(&fs);
  TEST_CHECK(NIFFS_mount(&fs) == NIFFS_OK);

  int fd = niffs_open(&fs, "test");
  TEST_CHECK(fd == ERR_NIFFS_FILE_NOT_FOUND);

  res = niffs_create(&fs, "test");
  TEST_CHECK(res == NIFFS_OK);

  fd = niffs_open(&fs, "test");
  TEST_CHECK(fd >= 0);

  return TEST_RES_OK;
} TEST_END(func_open_basic)

TEST(func_append_basic) {
  int res = NIFFS_format(&fs);
  TEST_CHECK(NIFFS_mount(&fs) == NIFFS_OK);

  res = niffs_create(&fs, "test");
  TEST_CHECK(res == NIFFS_OK);

  int fd = niffs_open(&fs, "test");
  TEST_CHECK(fd >= 0);

  u32_t len = _NIFFS_SPIX_2_PDATA_LEN(&fs, 0) - 4;
  u8_t *d = niffs_emul_create_data("test", len);
  res = niffs_append(&fs, fd, d, len);
  NIFFS_dump(&fs);
  niffs_emul_dump_pix(&fs, 0);
  niffs_emul_dump_pix(&fs, 1);
  TEST_CHECK(res == NIFFS_OK);

  printf("\n\n\n");

  res = niffs_append(&fs, fd, d, len);
  NIFFS_dump(&fs);
  niffs_emul_dump_pix(&fs, 0);
  niffs_emul_dump_pix(&fs, 1);
  niffs_emul_dump_pix(&fs, 2);
  niffs_emul_dump_pix(&fs, 3);
  niffs_emul_dump_pix(&fs, 4);
  niffs_emul_dump_pix(&fs, 5);
  TEST_CHECK(res == NIFFS_OK);

  return TEST_RES_OK;
} TEST_END(func_append_basic)

SUITE_END(niffs_func_tests)
