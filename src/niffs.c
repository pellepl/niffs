#include "niffs_internal.h"

#define NIFFS_VIS_CONT        1
#define NIFFS_VIS_END         2

typedef int (* niffs_visitor_f)(niffs *fs, niffs_page_ix pix, niffs_page_hdr *phdr, void *v_arg);

static int niffs_traverse(niffs *fs, niffs_page_ix pix_start, niffs_page_ix pix_end, niffs_visitor_f v, void *v_arg) {
  int res = NIFFS_OK;
  int v_res = NIFFS_OK;
  niffs_page_ix pix = pix_start;
  do {
    v_res = v(fs, pix, (niffs_page_hdr *)_NIFFS_PIX_2_ADDR(fs, pix), v_arg);
    if (v_res != NIFFS_VIS_CONT) {
      res = v_res;
      break;
    }
    // next page, wrap if necessary
    pix++;
    if (pix >= fs->pages_per_sector * fs->sectors) {
      pix = 0;
    }
  } while (pix != pix_end);

  if (v_res == NIFFS_VIS_CONT) {
    res = NIFFS_VIS_END;
  }

  return res;
}

static int niffs_ensure_free_page(niffs *fs) {
  int res = NIFFS_OK;
  if (fs->free_pages <= fs->pages_per_sector) {
    if (fs->dele_pages == 0) {
      return ERR_NIFFS_FULL;
    }
    // else do gc TODO
  }
  return res;
}

static int niffs_find_free_id_v(niffs *fs, niffs_page_ix pix, niffs_page_hdr *phdr, void *v_arg) {
  (void)pix;
  if (!_NIFFS_IS_FREE(phdr) && !_NIFFS_IS_DELE(phdr)) {
    niffs_obj_id id = phdr->id.obj_id;
    if (id != 0 && id != (niffs_obj_id)-1) {
      --id;
      fs->buf[id/8] |= 1<<(id&7);
      if (v_arg && phdr->id.spix == 0) {
        // object header page
        niffs_object_hdr *ohdr = (niffs_object_hdr *)phdr;
        if (strcmp((char *)v_arg, (char *)ohdr->name) == 0) {
          return ERR_NIFFS_NAME_CONFLICT;
        }
      }
    }
  }
  return NIFFS_VIS_CONT;
}

TESTATIC int niffs_find_free_id(niffs *fs, niffs_obj_id *id, char *conflict_name) {
  if (id == 0) return ERR_NIFFS_NULL_PTR;
  memset(fs->buf, 0, fs->buf_len);
  int res = niffs_traverse(fs, 0, 0, niffs_find_free_id_v, (void *)conflict_name);

  if (res != NIFFS_VIS_END) return res;

  u32_t max_id = (((fs->sector_size - sizeof(niffs_sector_hdr)) / (fs->page_size)) * fs->sectors) - 2;
  u32_t cur_id;
  for (cur_id = 0; cur_id < max_id; cur_id += 8) {
    if (fs->buf[cur_id/8] == 0xff) continue;
    u8_t bit_ix;
    for (bit_ix = 0; bit_ix < 8; bit_ix++) {
      if ((fs->buf[cur_id/8] & (1<<bit_ix)) == 0 && (cur_id + bit_ix) + 1 < max_id) {
        *id = (cur_id + bit_ix) + 1;
        return NIFFS_OK;
      }
    }
  }

  return ERR_NIFFS_NO_FREE_ID;
}

typedef struct {
  niffs_page_ix *pix;
  u32_t excl_sector;
} niffs_find_free_page_arg;

static int niffs_find_free_page_v(niffs *fs, niffs_page_ix pix, niffs_page_hdr *phdr, void *v_arg) {
  niffs_find_free_page_arg *arg = (niffs_find_free_page_arg *)v_arg;
  if (arg->excl_sector != NIFFS_EXCL_SECT_NONE && _NIFFS_PIX_2_SECTOR(fs, pix) == arg->excl_sector) {
    return NIFFS_VIS_CONT;
  }
  if (_NIFFS_IS_FREE(phdr) && _NIFFS_IS_CLEA(phdr)) {
    *arg->pix = pix;
    return NIFFS_OK;
  }
  return NIFFS_VIS_CONT;
}

TESTATIC int niffs_find_free_page(niffs *fs, niffs_page_ix *pix, u32_t excl_sector) {
  if (pix == 0) return ERR_NIFFS_NULL_PTR;

  niffs_find_free_page_arg arg;
  arg.pix = pix;
  arg.excl_sector = excl_sector;
  int res = niffs_traverse(fs, fs->last_free_pix, fs->last_free_pix, niffs_find_free_page_v, &arg);
  if (res == NIFFS_VIS_END) {
    res = ERR_NIFFS_NO_FREE_PAGE;
  } else {
    fs->last_free_pix = *pix;
  }
  return res;
}

TESTATIC int niffs_erase_sector(niffs *fs, u32_t sector_ix) {
  niffs_sector_hdr shdr;
  niffs_sector_hdr *target_shdr = (niffs_sector_hdr *)_NIFFS_SECTOR_2_ADDR(fs, sector_ix);
  if (target_shdr->abra == _NIFFS_SECT_MAGIC(fs)) {
    shdr.era_cnt = target_shdr->era_cnt+1;
  } else {
    shdr.era_cnt = 0;
  }
  shdr.abra = _NIFFS_SECT_MAGIC(fs);
  int res = fs->hal_er(_NIFFS_SECTOR_2_ADDR(fs, sector_ix), fs->sector_size);
  if (res == NIFFS_OK) {
    res = fs->hal_wr((u8_t *)target_shdr, (u8_t *)&shdr, sizeof(niffs_sector_hdr));
  }
  return res;
}

TESTATIC int niffs_delete_page(niffs *fs, niffs_page_ix pix) {
  niffs_page_id_raw delete_raw_id = _NIFFS_PAGE_DELE_ID;
  niffs_page_hdr *phdr = (niffs_page_hdr *) _NIFFS_PIX_2_ADDR(fs, pix);
  if (_NIFFS_IS_FREE(phdr)) {
    return ERR_NIFFS_DELETING_FREE_PAGE;
  }
  if (_NIFFS_IS_DELE(phdr)) {
    return ERR_NIFFS_DELETING_DELETED_PAGE;
  }
  int res = fs->hal_wr((u8_t *)phdr + offsetof(niffs_page_hdr, id), (u8_t *)&delete_raw_id, sizeof(niffs_page_id_raw));
  if (res == NIFFS_OK) {
    fs->dele_pages++;
  }
  return res;
}

TESTATIC int niffs_move_page(niffs *fs, niffs_page_ix src_pix, niffs_page_ix dst_pix) {
  niffs_page_hdr *src_phdr = (niffs_page_hdr *)_NIFFS_PIX_2_ADDR(fs, src_pix);
  niffs_page_hdr *dst_phdr = (niffs_page_hdr *)_NIFFS_PIX_2_ADDR(fs, dst_pix);

  if (_NIFFS_IS_FREE(src_phdr)) {
    return ERR_NIFFS_MOVING_FREE_PAGE;
  }
  if (_NIFFS_IS_DELE(src_phdr)) {
    return ERR_NIFFS_MOVING_DELETED_PAGE;
  }
  if (!_NIFFS_IS_FREE(dst_phdr)) {
    return ERR_NIFFS_MOVING_TO_UNFREE_PAGE;
  }

  // mark src as moving
  int res = NIFFS_OK;
  if (!_NIFFS_IS_MOVI(src_phdr)) {
    niffs_flag moving_flag = _NIFFS_FLAG_MOVING;
    res = fs->hal_wr((u8_t *)src_phdr + offsetof(niffs_page_hdr, flag), (u8_t *)&moving_flag, sizeof(niffs_flag));
    if (res == NIFFS_OK) return res;
  }

  // write dst..
  if (!_NIFFS_IS_CLEA(src_phdr)) {
    // .. written flag ..
    niffs_flag written_flag = _NIFFS_FLAG_WRITTEN;
    res = fs->hal_wr((u8_t *)dst_phdr + offsetof(niffs_page_hdr, flag), (u8_t *)&written_flag, sizeof(niffs_flag));
    if (res != NIFFS_OK) return res;
    fs->free_pages--;
    // .. data ..
    res = fs->hal_wr((u8_t *)dst_phdr + sizeof(niffs_page_hdr), (u8_t *)src_phdr  + sizeof(niffs_page_hdr), fs->page_size - sizeof(niffs_page_hdr));
    if (res != NIFFS_OK) return res;
  }
  // .. and id
  res = fs->hal_wr((u8_t *)dst_phdr + offsetof(niffs_page_hdr, id), (u8_t *)src_phdr  + offsetof(niffs_page_hdr, id), sizeof(niffs_page_hdr_id));
  if (res != NIFFS_OK) return res;

  // delete src
  res = niffs_delete_page(fs, src_pix);

  return res;
}

TESTATIC int niffs_write_phdr(niffs *fs, niffs_page_ix pix, niffs_page_hdr *phdr) {
  niffs_page_hdr *phdr_addr = (niffs_page_hdr *)_NIFFS_PIX_2_ADDR(fs, pix);

  if (phdr->id.obj_id == 0 || phdr->id.obj_id == (niffs_obj_id)-1) {
    return ERR_NIFFS_WR_PHDR_BAD_ID;
  }
  if (!_NIFFS_IS_FREE(phdr_addr) || !_NIFFS_IS_CLEA(phdr_addr)) {
    return ERR_NIFFS_WR_PHDR_UNFREE_PAGE;
  }

  int res;
  if (!_NIFFS_IS_CLEA(phdr)) {
    // first, write flag
    res = fs->hal_wr((u8_t *)phdr_addr + offsetof(niffs_page_hdr, flag), (u8_t *)&phdr->flag, sizeof(niffs_flag));
    if (res != NIFFS_OK) return res;
  }

  // .. then id
  res = fs->hal_wr((u8_t *)phdr_addr + offsetof(niffs_page_hdr, id), (u8_t *)&phdr->id, sizeof(niffs_page_hdr_id));

  return res;
}

TESTATIC int niffs_create(niffs *fs, char *name) {
  niffs_obj_id id;
  niffs_page_ix pix;
  int res;

  if (name == 0) return ERR_NIFFS_NULL_PTR;

  res = niffs_ensure_free_page(fs);
  if (res != NIFFS_OK) return res;

  res = niffs_find_free_id(fs, &id, name);
  if (res != NIFFS_OK) return res;

  res = niffs_find_free_page(fs, &pix, NIFFS_EXCL_SECT_NONE);
  if (res != NIFFS_OK) return res;

  // write page header for object header
  niffs_object_hdr ohdr;
  ohdr.phdr.flag = _NIFFS_FLAG_CLEAN;
  ohdr.phdr.id.obj_id = id;
  ohdr.phdr.id.spix = 0;
  ohdr.phdr.id.cycle_ix = 0;
  res = niffs_write_phdr(fs, pix, &ohdr.phdr);
  if (res != NIFFS_OK) return res;
  fs->free_pages--;

  // write clean object header
  ohdr.len = NIFFS_UNDEF_LEN;
  strcpy((char *)ohdr.name, (char *)name);
  niffs_page_hdr *phdr_addr = (niffs_page_hdr *)_NIFFS_PIX_2_ADDR(fs, pix);

  res = fs->hal_wr((u8_t *)phdr_addr + offsetof(niffs_object_hdr, len), (u8_t *)&ohdr + offsetof(niffs_object_hdr, len),
      sizeof(niffs_object_hdr) - sizeof(niffs_page_hdr_id));

  return res;
}

#if 0

TESTATIC int niffs_append(niffs *fs, niffs_file_desc *fd) {

}
#endif
// TODO check
// sector headers for magic
// _NIFFS_IS_FREE && _NIFFS_IS_WRIT => aborted in midst of moving

int NIFFS_init(niffs *fs, u8_t *phys_addr, u32_t sectors, u32_t sector_size, u32_t page_size,
    u8_t *buf, u32_t buf_len, niffs_hal_erase_f erase_f, niffs_hal_write_f write_f) {
  fs->phys_addr = phys_addr;
  fs->sectors = sectors;
  fs->sector_size = sector_size;
  fs->buf = buf;
  fs->buf_len = buf_len;
  fs->hal_er = erase_f;
  fs->hal_wr = write_f;
  fs->last_free_pix = 0;

  u32_t pages_per_sector = sector_size / page_size;

  // calculate actual page size incl page header - leave room for sector header
  if (sector_size % page_size < sizeof(niffs_sector_hdr)) {
    fs->page_size = page_size
        /* int part */  - (sizeof(niffs_sector_hdr) / pages_per_sector)
        /* frac part */ - ((sizeof(niffs_sector_hdr) % pages_per_sector) != 0 ? 1 : 0);
  } else {
    fs->page_size = page_size;
  }
  fs->page_size = fs->page_size & (~(NIFFS_WORD_ALIGN-1));

  // check validity
  if (fs->page_size == 0 || fs->page_size > fs->sector_size) {
    NIFFS_DBG("conf: page size over/underflow\n");
    return ERR_NIFFS_BAD_CONF;
  }
  if (sizeof(niffs_page_id_raw)*8 < 2 + NIFFS_OBJ_ID_BITS + NIFFS_SPAN_IX_BITS) {
    NIFFS_DBG("conf: niffs_page_id_raw type too small\n");
    return ERR_NIFFS_BAD_CONF;
  }
  if ((((fs->sector_size - sizeof(niffs_sector_hdr)) / (fs->page_size)) * fs->sectors) - 2 > (1<<NIFFS_OBJ_ID_BITS)) {
    NIFFS_DBG("conf: niffs_obj_id type too small\n");
    return ERR_NIFFS_BAD_CONF;
  }
  if (sizeof(niffs_span_ix)*8 < NIFFS_SPAN_IX_BITS) {
    NIFFS_DBG("conf: niffs_span_ix type too small\n");
    return ERR_NIFFS_BAD_CONF;
  }
  if (buf_len < (((sector_size * sectors) / page_size)+7) / 8) {

    NIFFS_DBG("conf: buffer length too small, need %i bytes\n", (((sector_size * sectors) / page_size)+7) / 8);
    return ERR_NIFFS_BAD_CONF;
  }

  fs->pages_per_sector = pages_per_sector;

  NIFFS_DBG("page size req:         %i\n", page_size);
  NIFFS_DBG("actual page size:      %i\n", fs->page_size);
  NIFFS_DBG("num unique obj ids:    %i\n", (1<<NIFFS_OBJ_ID_BITS)-2);
  NIFFS_DBG("max span ix:           %i\n", (1<<NIFFS_SPAN_IX_BITS));
  NIFFS_DBG("max file data length:  %i\n", (u32_t)((1<<NIFFS_SPAN_IX_BITS) * (fs->page_size - sizeof(niffs_page_hdr)) - sizeof(niffs_object_hdr)));
  NIFFS_DBG("max num files by size: %i\n", (u32_t)(((fs->sector_size - sizeof(niffs_sector_hdr)) / (fs->page_size)) * fs->sectors));

  return NIFFS_OK;
}

int NIFFS_format(niffs *fs) {
  int res = NIFFS_OK;
  u32_t s;
  for (s = 0; res == NIFFS_OK && s < fs->sectors; s++) {
    res = niffs_erase_sector(fs, s);
    if (res != NIFFS_OK) return res;
    if (((niffs_sector_hdr *)_NIFFS_SECTOR_2_ADDR(fs, s))->abra != _NIFFS_SECT_MAGIC(fs)) {
      NIFFS_DBG("\nfatal: erase sector %i hdr magic, expected %08x, was %08x\n",
          s,
          _NIFFS_SECT_MAGIC(fs),
          ((niffs_sector_hdr *)_NIFFS_SECTOR_2_ADDR(fs, s))->abra);
      res = ERR_NIFFS_SECTOR_UNFORMATTABLE;
    }
  }
  return res;
}

int NIFFS_mount(niffs *fs) {
  fs->free_pages = 0;
  fs->dele_pages = 0;
  u32_t s;
  for (s = 0; s < fs->sectors; s++) {
    niffs_sector_hdr *shdr = (niffs_sector_hdr *)_NIFFS_SECTOR_2_ADDR(fs, s);
    if (shdr->abra != _NIFFS_SECT_MAGIC(fs)) {
      return ERR_NIFFS_BAD_SECTOR;
    }
    niffs_page_ix ipix;
    for (ipix = 0; ipix < fs->pages_per_sector; ipix++) {
      niffs_page_ix pix = _NIFFS_PIX_AT_SECTOR(fs, s) + ipix;
      niffs_page_hdr *phdr = (niffs_page_hdr *)_NIFFS_PIX_2_ADDR(fs, pix);
      if (_NIFFS_IS_FREE(phdr)) {
        fs->free_pages++;
      }
      else if (_NIFFS_IS_DELE(phdr)) {
        fs->dele_pages++;
      }
    }
  }
  fs->mounted = 1;
  return NIFFS_OK;
}


#ifdef NIFFS_DUMP
void NIFFS_dump(niffs *fs) {
  u32_t s;
  NIFFS_DUMP_OUT("NIFFS\n");
  NIFFS_DUMP_OUT("sector size : %i\n", fs->sector_size);
  NIFFS_DUMP_OUT("sectors     : %i\n", fs->sectors);
  NIFFS_DUMP_OUT("pages/sector: %i\n", fs->pages_per_sector);
  NIFFS_DUMP_OUT("page size   : %i\n", fs->page_size);
  NIFFS_DUMP_OUT("phys addr   : %p\n", fs->phys_addr);
  NIFFS_DUMP_OUT("free pages  : %i\n", fs->free_pages);
  NIFFS_DUMP_OUT("dele pages  : %i\n", fs->dele_pages);
  for (s = 0; s < fs->sectors; s++) {
    niffs_sector_hdr *shdr = (niffs_sector_hdr *)_NIFFS_SECTOR_2_ADDR(fs, s);
    NIFFS_DUMP_OUT("sector %i @ %p  era_cnt:%i  magic:%s\n", s, shdr, shdr->era_cnt, shdr->abra ==  _NIFFS_SECT_MAGIC(fs) ? "OK" : "BAD");
    niffs_page_ix ipix;
    for (ipix = 0; ipix < fs->pages_per_sector; ipix++) {
      niffs_page_ix pix = _NIFFS_PIX_AT_SECTOR(fs, s) + ipix;
      niffs_object_hdr *ohdr = (niffs_object_hdr *)_NIFFS_PIX_2_ADDR(fs, pix);
      NIFFS_DUMP_OUT("  %03i fl:%04x id:%04x ", pix, ohdr->phdr.flag, ohdr->phdr.id.raw);
      if (_NIFFS_IS_FREE(&ohdr->phdr)) NIFFS_DUMP_OUT("FREE ");
      if (_NIFFS_IS_DELE(&ohdr->phdr)) NIFFS_DUMP_OUT("DELE ");
      if (_NIFFS_IS_CLEA(&ohdr->phdr)) NIFFS_DUMP_OUT("CLEA ");
      if (_NIFFS_IS_WRIT(&ohdr->phdr)) NIFFS_DUMP_OUT("WRIT ");
      if (_NIFFS_IS_MOVI(&ohdr->phdr)) NIFFS_DUMP_OUT("MOVI ");
      if (!_NIFFS_IS_FREE(&ohdr->phdr)) {
        NIFFS_DUMP_OUT("\n    obj.id:%04x  sp.ix:%02x  cyc:%01x  ", ohdr->phdr.id.obj_id, ohdr->phdr.id.spix, ohdr->phdr.id.cycle_ix);
        if (ohdr->phdr.id.spix == 0) {
          NIFFS_DUMP_OUT("len:%08x  name:%s  ", ohdr->len, ohdr->name);
        }
      }
      NIFFS_DUMP_OUT("\n");

    }

  }
}
#endif

