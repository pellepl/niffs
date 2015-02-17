/*
 * niffs_cfg_test.c
 *
 *  Created on: Feb 5, 2015
 *      Author: petera
 */


#include "testrunner.h"
#include "niffs_test_emul.h"


SUITE(niffs_cfg_tests)

void setup(test *t) {
}

void teardown(test *t) {
}

TEST(cfg_init_virgin) {
  int res;
  static u8_t dummy[1024*2];
  static niffs_file_desc fds[4];
  res = NIFFS_init(&fs, dummy, 2, 1024, 2048,
        dummy, 1024,
        fds, 4,
        0, 0);
  TEST_CHECK_EQ(res, ERR_NIFFS_BAD_CONF);

  res = NIFFS_init(&fs, dummy, 2, 1024, 64,
        dummy, 2,
        fds, 4,
        0, 0);
  TEST_CHECK_EQ(res, ERR_NIFFS_BAD_CONF);

  res = niffs_emul_init();
  TEST_CHECK_EQ(res, NIFFS_OK);
  return TEST_RES_OK;
} TEST_END(cfg_init_virgin)

SUITE_END(niffs_cfg_tests)
