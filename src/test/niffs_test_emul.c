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
static u8_t buf[TEST_PARAM_PAGE_SIZE];
static niffs_file_desc descs[EMUL_FILE_DESCS];
niffs fs;

typedef struct fdata_s{
  char name[32];
  void *data;
  u32_t len;
  struct fdata_s *next;
} fdata;

static fdata *dhead = 0;
static fdata *dlast = 0;
static u32_t valid_byte_writes = 0;

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
//  if (len % NIFFS_WORD_ALIGN != 0) {
//    printf("unaligned write length %08x\n", len);
//    return ERR_NIFFS_TEST_UNLIGNED_WRITE_LEN;
//  }
#ifdef TEST_CHECK_UNALIGNED_ACCESS
  if ((uintptr_t)addr % NIFFS_WORD_ALIGN != 0) {
    printf("unaligned write address %p\n", addr);
    return ERR_NIFFS_TEST_UNLIGNED_WRITE_ADDR;
  }
#endif
  int i;
  for (i = 0;  i < len; i++) {
    u8_t b = *src;
#ifdef TEST_CHECK_WRITE_ON_NONERASED_DATA_OTHER_THAN_ZERO
    if (b != 0 && *addr != 0xff) {
      printf("writing illegaly to address %p: %02x @ %02x\n", addr, b, *addr);
      return ERR_NIFFS_TEST_WRITE_TO_NONERASED_DATA;
    }
#endif
    //printf("write %02x to %p [%02x] => ", b, addr, *addr);
    *addr &= (b);
    //printf("%02x\n", *addr);
    addr++;
    src++;
    if (valid_byte_writes) {
      --valid_byte_writes;
      if (valid_byte_writes == 0) return ERR_NIFFS_TEST_ABORTED_WRITE;
    }
  }
  return NIFFS_OK;
}

int niffs_emul_init(void) {
  dhead = 0;
  dlast = 0;
  memset(_flash, 0xff, sizeof(_flash));
  return NIFFS_init(&fs, (u8_t *)&_flash[0], EMUL_SECTORS, EMUL_SECTOR_SIZE, TEST_PARAM_PAGE_SIZE,
      buf, sizeof(buf),
      descs, EMUL_FILE_DESCS,
      emul_hal_erase_f, emul_hal_write_f);
  valid_byte_writes = 0;
  return 0;
}

void niffs_emul_rand_filesystem(void) {
  u32_t i;
  for (i = 0; i < sizeof(_flash); i++) {
    _flash[i] = rand();
  }
}

void niffs_emul_set_write_byte_limit(u32_t limit) {
  valid_byte_writes = limit;
}

void memdump(u8_t *addr, u32_t len) {
  u8_t *a = addr;
  while (a < addr + len) {
    int i;
    for (i = 0; i < 16; i++) {
      if (&a[i] < addr+len) printf("%02x", a[i]);
      else printf("  ");
    }
    printf("  ");
    for (i = 0; i < 16; i++) {
      if (&a[i] < addr+len) printf("%c", a[i] >= ' ' && a[i] < 0x7f ? a[i] : '.');
      else printf(" ");
    }
    printf("\n");
    a += 16;
  }
}


void niffs_emul_dump_pix(niffs *fs, niffs_page_ix pix) {
  niffs_page_hdr *phdr = (niffs_page_hdr *)_NIFFS_PIX_2_ADDR(fs, pix);
  printf("PIX:%04i @ %p\n", pix, phdr);
  printf("  flag:%04x  id:%04x  oid:%02x  spix:%02x\n",
      phdr->flag, phdr->id.raw, phdr->id.obj_id, phdr->id.spix);
  u8_t *data = (u8_t *)phdr + sizeof(niffs_page_hdr);
  u32_t len = _NIFFS_SPIX_2_PDATA_LEN(fs, 1);
  if (!_NIFFS_IS_FREE(phdr) && !_NIFFS_IS_DELE(phdr) &&_NIFFS_IS_ID_VALID(phdr) && phdr->id.spix == 0) {
    niffs_object_hdr *ohdr = (niffs_object_hdr *)phdr;
    printf("  length:%08x  name:%s\n", ohdr->len, ohdr->name);
    data = (u8_t *)phdr + sizeof(niffs_object_hdr);
    len = _NIFFS_SPIX_2_PDATA_LEN(fs, 0);
  }

  memdump(data, len);
}

u8_t *niffs_emul_create_data(char *name, u32_t len) {
  fdata *e = malloc(sizeof(fdata));
  NIFFS_ASSERT(e);
  strncpy(e->name, name, 32);

  u32_t seed = 0x20050715;
  int i;
  for(i = 0; i < strlen(e->name); i++) {
    seed ^= ((seed & 0xf0000000) >> 28);
    seed <<= 4;
    seed ^= e->name[i];
  }

  u8_t *d = malloc(len);
  NIFFS_ASSERT(d);
  e->len = len;
  e->data = d;
  e->next = 0;

  srand(seed);
  for (i = 0; i < len; i++){
    d[i] = rand();
  }

  if (dhead == NULL) {
    dhead = e;
    dlast = e;
  } else {
    dlast->next = e;
    dlast = e;
  }
  return d;
}

void niffs_emul_destroy_all_data(void) {
  while (dhead) {
    free(dhead->data);
    fdata *pdhead = dhead;
    dhead = dhead->next;
    free(pdhead);
  }
  dlast = 0;
}

u8_t *niffs_emul_get_data(char *name, u32_t *len) {
  fdata *e = dhead;
  while (e) {
    if (strcmp(name, e->name) == 0) {
      if (len) *len = e->len;
      return e->data;
    }
  }
  NIFFS_ASSERT(0);
  return 0;
}

void niffs_emul_get_sector_erase_count_info(niffs *fs, u32_t *s_era_min, u32_t *s_era_max) {
  u32_t s;
  *s_era_min = 0xffffffff;
  *s_era_max = 0;
  for (s = 0; s < fs->sectors; s++) {
    niffs_sector_hdr *shdr = (niffs_sector_hdr *)_NIFFS_SECTOR_2_ADDR(fs, s);
    if (shdr->era_cnt != (niffs_erase_cnt)-1) {
      *s_era_min = MIN(*s_era_min, shdr->era_cnt);
      *s_era_max = MAX(*s_era_max, shdr->era_cnt);
    }
  }
}
