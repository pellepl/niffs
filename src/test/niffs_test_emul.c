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

u8_t __dbg = NIFFS_DBG_DEFAULT;
static u8_t _flash[EMUL_SECTORS * EMUL_SECTOR_SIZE];
static u8_t buf[EMUL_BUF_SIZE];
static niffs_file_desc descs[EMUL_FILE_DESCS];
niffs fs;

typedef struct fdata_s{
  char name[32];
  u8_t *data;
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
      printf("writing illegally to address %p: %02x @ %02x\n", addr, b, *addr);
      return ERR_NIFFS_TEST_WRITE_TO_NONERASED_DATA;
    }
#endif
    //printf("write %02x to %p [%02x] => ", b, addr, *addr);
    *addr &= (b);
    //printf("%02x\n", *addr);
    addr++;
    src++;
    if (valid_byte_writes > 0) {
      --valid_byte_writes;
      if (valid_byte_writes == 0) {
        NIFFS_DBG("*** emulated write abort\n");
        return ERR_NIFFS_TEST_ABORTED_WRITE;
      }
    }
  }
  return NIFFS_OK;
}

int niffs_emul_init(void) {
  dhead = 0;
  dlast = 0;
  memset(_flash, 0xff, sizeof(_flash));
  return NIFFS_init(&fs, (u8_t *)&_flash[0], EMUL_SECTORS, EMUL_SECTOR_SIZE, EMUL_PAGE_SIZE,
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

void memrand(u8_t *d, u32_t len, u32_t seed) {
  u32_t i;
  srand(seed);
  for (i = 0; i < len; i++){
    d[i] = rand();
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

  if (dhead == 0) {
    dhead = e;
    dlast = e;
  } else {
    dlast->next = e;
    dlast = e;
  }
  return d;
}

static fdata *cursor = 0;

void niffs_emul_reset_data_cursor(void) {
  cursor = 0;
}

char *niffs_emul_get_next_data_name(void) {
  if (cursor == 0) {
    cursor = dhead;
  } else {
    cursor = cursor->next;
  }
  return cursor == 0 ? 0 : cursor->name;
}

u8_t *niffs_emul_extend_data(char *name, u32_t extra_len, u8_t *extra_data) {
  fdata *cur_e = dhead;
  fdata *e = 0;
  while (cur_e) {
    if (strcmp(name, cur_e->name) == 0) {
      e = cur_e;
      break;
    }
  }
  NIFFS_ASSERT(e);

  u8_t *ndata = malloc(e->len + extra_len);
  NIFFS_ASSERT(ndata);
  memcpy(ndata, e->data, e->len);
  if (extra_data) memcpy(ndata + e->len, extra_data, extra_len);
  free(e->data);
  e->data = ndata;
  e->len = e->len + extra_len;
  return e->data;
}

u8_t *niffs_emul_write_data(char *name, u32_t offs, u8_t *data, u32_t len) {
  fdata *cur_e = dhead;
  fdata *e = 0;
  while (cur_e) {
    if (strcmp(name, cur_e->name) == 0) {
      e = cur_e;
      break;
    }
  }
  NIFFS_ASSERT(e);

  if (e->len < offs+len) {
    (void)niffs_emul_extend_data(name, offs+len-e->len, 0);
  }

  memcpy(&e->data[offs], data, len);

  return e->data;
}

u8_t *niffs_emul_write_rand_data(char *name, u32_t offs, u32_t seed, u32_t len) {
  fdata *cur_e = dhead;
  fdata *e = 0;
  while (cur_e) {
    if (strcmp(name, cur_e->name) == 0) {
      e = cur_e;
      break;
    }
  }
  NIFFS_ASSERT(e);

  if (e->len < offs+len) {
    (void)niffs_emul_extend_data(name, offs+len-e->len, 0);
  }

  memrand(&e->data[offs], len, seed);

  return e->data;
}

void niffs_emul_destroy_data(char *name) {
  fdata *prev_e = 0;
  fdata *cur_e = dhead;
  while (cur_e) {
    if (strcmp(name, cur_e->name) == 0) {
      break;
    }
    prev_e = cur_e;
    cur_e = cur_e->next;
  }
  NIFFS_ASSERT(cur_e);

  if (dhead == cur_e) {
    free(cur_e->data);
    dhead = cur_e->next;
    free(cur_e);
  } else {
    free(cur_e->data);
    prev_e->next = cur_e->next;
    free(cur_e);
  }
  if (dlast == cur_e) {
    dlast = prev_e;
  }
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
    e = e->next;
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

int niffs_emul_create_file(niffs *fs, char *name, u32_t len) {
  int res;
  int fd = NIFFS_open(fs, name, NIFFS_O_CREAT | NIFFS_O_TRUNC | NIFFS_O_RDWR, 0);
  if (fd < 0) return fd;
  u8_t *data = niffs_emul_create_data(name, len);
  if (data == 0) return ERR_NIFFS_TEST_FATAL;
  res = NIFFS_write(fs, fd, data, len);
  if (res < 0) return res;
  res = NIFFS_lseek(fs, fd, 0, NIFFS_SEEK_SET);
  if (res < 0) return res;
  niffs_stat s;
  res = NIFFS_fstat(fs, fd, &s);
  if (s.size != len) {
    return ERR_NIFFS_TEST_FATAL;
  }
  u8_t buf[64];
  u32_t offs = 0;
  do {
    u32_t rlen = MIN(len - offs, sizeof(buf));
    res = NIFFS_read(fs, fd, &buf[0], rlen);
    if (res == 0) return ERR_NIFFS_TEST_EOF;
    if (res < 0) return res;
    if (res != rlen) {
      return ERR_NIFFS_TEST_FATAL;
    }
    if (memcmp(buf, &data[offs], rlen) != 0) {
      u32_t ix;
      u32_t mismatch_ix;
      memdump(buf, sizeof(buf));
      memdump(&data[offs < 32 ? 0 : offs-32], sizeof(buf));
      for (ix = 0; ix < rlen; ix++) {
        if (buf[ix] != data[offs+ix]) {
          mismatch_ix = ix;
        }
      }
      NIFFS_DBG("mismatch @ offset %i\n", offs+mismatch_ix);
      return ERR_NIFFS_TEST_REF_DATA_MISMATCH;
    }
    offs += rlen;
  } while (offs < len);
  (void)NIFFS_close(fs, fd);
  return res >= 0 ? NIFFS_OK : res;
}

int niffs_emul_verify_file(niffs *fs, char *name) {
  u32_t dlen;
  u8_t *data = niffs_emul_get_data(name, &dlen);
  if (data == 0) {
    return ERR_NIFFS_TEST_FATAL;
  }
  return niffs_emul_verify_file_against_data(fs, name, data);
}

int niffs_emul_verify_file_against_data(niffs *fs, char *name, u8_t *data) {
  int fd = NIFFS_open(fs, name, NIFFS_O_RDONLY, 0);
  if (fd < 0) return fd;
  if (data == 0) {
    return ERR_NIFFS_TEST_FATAL;
  }
  niffs_stat s;
  int res = NIFFS_fstat(fs, fd, &s);
  if (res != NIFFS_OK) {
    return ERR_NIFFS_TEST_FATAL;
  }

  u8_t buf[8];
  u32_t offs = 0;
  do {
    u32_t rlen = MIN(s.size - offs, sizeof(buf));
    res = NIFFS_read(fs, fd, &buf[0], rlen);
    if (res == 0) return ERR_NIFFS_TEST_EOF;
    if (res < 0) return res;
    if (res != rlen) {
      return ERR_NIFFS_TEST_FATAL;
    }
    if (memcmp(buf, &data[offs], rlen) != 0) {
      u32_t ix;
      u32_t mismatch_ix;
      memdump(buf, sizeof(buf));
      memdump(&data[offs < 32 ? 0 : offs-32], sizeof(buf));
      for (ix = 0; ix < rlen; ix++) {
        if (buf[ix] != data[offs+ix]) {
          mismatch_ix = ix;
        }
      }
      NIFFS_DBG("mismatch @ offset %i\n", offs+mismatch_ix);
      return ERR_NIFFS_TEST_REF_DATA_MISMATCH;
    }
    offs += rlen;
  } while (offs < s.size);
  (void)NIFFS_close(fs, fd);
  return res >= 0 ? NIFFS_OK : res;
}

int niffs_emul_remove_all_zerosized_files(niffs *fs) {
  niffs_DIR d;
  struct niffs_dirent e;
  struct niffs_dirent *pe = &e;

  NIFFS_opendir(fs, "/", &d);
  while ((pe = NIFFS_readdir(&d, pe))) {
    if (pe->size == 0) {
      int res = NIFFS_remove(fs, pe->name);
      if (res != NIFFS_OK) return res;
    }
  }
  NIFFS_closedir(&d);
  return NIFFS_OK;
}
