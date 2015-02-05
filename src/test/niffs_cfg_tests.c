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
  int res = niffs_emul_init();
  TEST_CHECK(res == NIFFS_OK);
  return TEST_RES_OK;
} TEST_END(cfg_init_virgin)

SUITE_END(niffs_cfg_tests)
