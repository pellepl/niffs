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
void niffs_emul_rand_filesystem(void);
void niffs_emul_dump_pix(niffs *fs, niffs_page_ix pix);
u8_t *niffs_emul_create_data(char *name, u32_t len);
void niffs_emul_destroy_all_data(void);
u8_t *niffs_emul_get_data(char *name, u32_t *len);
void niffs_emul_get_sector_erase_count_info(niffs *fs, u32_t *s_era_min, u32_t *s_era_max);
void niffs_emul_set_write_byte_limit(u32_t limit);

#endif /* NIFFS_TEST_EMUL_H_ */
