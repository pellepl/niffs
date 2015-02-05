/*
 * testsuites.c
 *
 *  Created on: Jun 19, 2013
 *      Author: petera
 */

#include "testrunner.h"

void add_suites() {
  ADD_SUITE(niffs_cfg_tests);
  ADD_SUITE(niffs_func_tests);
}
