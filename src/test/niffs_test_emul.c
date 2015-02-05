/*
 * niffs_test_emul.c
 *
 *  Created on: Feb 3, 2015
 *      Author: petera
 */
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <string.h>
#include "niffs_test_emul.h"

static u8_t _flash[EMUL_SECTORS * EMUL_SECTOR_SIZE];
static u8_t buf[32];
niffs fs;


static int emul_hal_erase_f(u8_t *addr, u32_t len) {
  if (addr < &_flash[0]) return ERR_NIFFS_TEST_BAD_ADDR;
  if (addr+len > &_flash[0] + EMUL_SECTORS * EMUL_SECTOR_SIZE) return ERR_NIFFS_TEST_BAD_ADDR;
  if ((addr - &_flash[0]) % EMUL_SECTOR_SIZE) return ERR_NIFFS_TEST_BAD_ADDR;
  if (len != EMUL_SECTOR_SIZE) return ERR_NIFFS_TEST_BAD_ADDR;
  memset(addr, 0xff, len);
  return NIFFS_OK;
}

static int emul_hal_write_f(u8_t *addr, u8_t *src, u32_t len) {
  if (addr < &_flash[0]) return ERR_NIFFS_TEST_BAD_ADDR;
  if (addr+len >= &_flash[0] + EMUL_SECTORS * EMUL_SECTOR_SIZE) return ERR_NIFFS_TEST_BAD_ADDR;
  if (len == 0) return ERR_NIFFS_TEST_BAD_ADDR;
#ifdef TEST_CHECK_UNALIGNED_ACCESS
  if (len % NIFFS_WORD_ALIGN != 0) return ERR_NIFFS_TEST_UNLIGNED_WRITE_LEN;
  if ((uintptr_t)addr % NIFFS_WORD_ALIGN != 0) return ERR_NIFFS_TEST_UNLIGNED_WRITE_ADDR;
#endif
  int i;
  for (i = 0;  i < len; i++) {
    u8_t b = *src;
#ifdef TEST_CHECK_WRITE_ON_NONERASED_DATA_OTHER_THAN_ZERO
    if (src != 0 && *addr != 0xff) return ERR_NIFFS_TEST_WRITE_TO_NONERASED_DATA;
#endif
    //printf("write %02x to %p [%02x] => ", b, addr, *addr);
    *addr &= (b);
    //printf("%02x\n", *addr);
    addr++;
    src++;
  }
  return NIFFS_OK;
}

int niffs_emul_init(void) {
  return NIFFS_init(&fs, (u8_t *)&_flash[0], EMUL_SECTORS, EMUL_SECTOR_SIZE, TEST_PARAM_PAGE_SIZE, buf, sizeof(buf),
      emul_hal_erase_f, emul_hal_write_f);
}


