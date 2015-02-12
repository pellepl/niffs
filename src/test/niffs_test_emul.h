/*
 * niffs_test_emul.h
 *
 *  Created on: Feb 3, 2015
 *      Author: petera
 */

#ifndef NIFFS_TEST_EMUL_H_
#define NIFFS_TEST_EMUL_H_

#include "niffs_test_config.h"
#include "niffs.h"
#include "niffs_internal.h"

extern niffs fs;

int niffs_emul_init(void);
void niffs_emul_dump_pix(niffs *fs, niffs_page_ix pix);
u8_t *niffs_emul_create_data(u32_t seed, u32_t len);
void niffs_emul_destroy_data(u8_t *data);

#endif /* NIFFS_TEST_EMUL_H_ */
