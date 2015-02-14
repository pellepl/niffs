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
    if (b != 0 && *addr != 0xff) return ERR_NIFFS_TEST_WRITE_TO_NONERASED_DATA;
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
  dhead = 0;
  dlast = 0;
  return NIFFS_init(&fs, (u8_t *)&_flash[0], EMUL_SECTORS, EMUL_SECTOR_SIZE, TEST_PARAM_PAGE_SIZE,
      buf, sizeof(buf),
      descs, EMUL_FILE_DESCS,
      emul_hal_erase_f, emul_hal_write_f);
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
  printf("  flag:%04x  id:%04x  oid:%02x  spix:%02x  cyc:%i\n",
      phdr->flag, phdr->id.raw, phdr->id.obj_id, phdr->id.spix, phdr->id.cycle_ix);
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
  strncpy(e->name, name, sizeof(e->name));

  u32_t seed = 0;
  int i;
  for(i = 0; i < strlen(name); i++) {
    seed <<= 8;
    seed |= name[i];
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
  }

  return d;
}

void niffs_emul_destroy_all_data(void) {
  while (dhead) {
    free(dhead->data);
    dhead = dhead->next;
    free(dhead);
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
  return 0;
}
