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

void memrand(u8_t *d, u32_t len, u32_t seed);
void memdump(u8_t *addr, u32_t len);

int niffs_emul_init(void);
void niffs_emul_rand_filesystem(void);
void niffs_emul_dump_pix(niffs *fs, niffs_page_ix pix);

u8_t *niffs_emul_create_data(char *name, u32_t len);
void niffs_emul_reset_data_cursor(void);
char *niffs_emul_get_next_data_name(void);
u8_t *niffs_emul_get_data(char *name, u32_t *len);
u8_t *niffs_emul_write_data(char *name, u32_t offs, u8_t *data, u32_t len);
u8_t *niffs_emul_write_rand_data(char *name, u32_t offs, u32_t seed, u32_t len);
u8_t *niffs_emul_extend_data(char *name, u32_t extra_len, u8_t *extra_data);
void niffs_emul_destroy_data(char *name);
void niffs_emul_destroy_all_data(void);

void niffs_emul_get_sector_erase_count_info(niffs *fs, u32_t *s_era_min, u32_t *s_era_max);
void niffs_emul_set_write_byte_limit(u32_t limit);
int niffs_emul_create_file(niffs *fs, char *name, u32_t len);
int niffs_emul_verify_file(niffs *fs, char *name);
int niffs_emul_verify_file_against_data(niffs *fs, char *name, u8_t *data);
int niffs_emul_remove_all_zerosized_files(niffs *fs);

int niffs_emul_read_ptr(niffs *fs, int fd_ix, u8_t **data, u32_t *avail);

#endif /* NIFFS_TEST_EMUL_H_ */
