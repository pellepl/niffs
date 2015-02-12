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

static int niffs_ensure_free_pages(niffs *fs, u32_t pages) {
  int res = NIFFS_OK;
  if (fs->free_pages < fs->pages_per_sector) {
    // crammed, even the spare sector is full
    // TODO gc
    return ERR_NIFFS_FULL;
  }
  if (pages > (fs->dele_pages + fs->free_pages - fs->pages_per_sector)) {
    // this will never fit without deleting stuff
    return ERR_NIFFS_FULL;
  }
  if (pages > fs->free_pages || fs->free_pages - pages < fs->pages_per_sector) {
    // TODO gc
  }
  return res;
}

static niffs_file_desc *niffs_get_free_fd(niffs *fs, int *ix) {
  u32_t i;
  for (i = 0; i < fs->descs_len; i++) {
    if (fs->descs[i].obj_id == 0) {
      *ix = i;
      return &fs->descs[i];
    }
  }
  return 0;
}

static int niffs_find_free_id_v(niffs *fs, niffs_page_ix pix, niffs_page_hdr *phdr, void *v_arg) {
  (void)pix;
  if (!_NIFFS_IS_FREE(phdr) && !_NIFFS_IS_DELE(phdr)) {
    if (_NIFFS_IS_ID_VALID(phdr)) {
      niffs_obj_id id = phdr->id.obj_id;
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

static void niffs_reserve_watch_pix(niffs *fs, niffs_page_ix *pix) {
  NIFFS_ASSERT(fs->watch_pix == 0);
  fs->watch_pix = pix;
}

static void niffs_release_watch_pix(niffs *fs) {
  NIFFS_ASSERT(fs->watch_pix != 0);
  fs->watch_pix = 0;
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
  if (_NIFFS_IS_FREE(phdr)) return ERR_NIFFS_DELETING_FREE_PAGE;
  if (_NIFFS_IS_DELE(phdr)) return ERR_NIFFS_DELETING_DELETED_PAGE;
  int res = fs->hal_wr((u8_t *)phdr + offsetof(niffs_page_hdr, id), (u8_t *)&delete_raw_id, sizeof(niffs_page_id_raw));
  if (res == NIFFS_OK) {
    fs->dele_pages++;
  }
  return res;
}

TESTATIC int niffs_move_page(niffs *fs, niffs_page_ix src_pix, niffs_page_ix dst_pix) {
  if (src_pix == dst_pix) return ERR_NIFFS_MOVING_TO_SAME_PAGE;

  niffs_page_hdr *src_phdr = (niffs_page_hdr *)_NIFFS_PIX_2_ADDR(fs, src_pix);
  niffs_page_hdr *dst_phdr = (niffs_page_hdr *)_NIFFS_PIX_2_ADDR(fs, dst_pix);

  if (_NIFFS_IS_FREE(src_phdr)) return ERR_NIFFS_MOVING_FREE_PAGE;
  if (_NIFFS_IS_DELE(src_phdr)) return ERR_NIFFS_MOVING_DELETED_PAGE;
  if (!_NIFFS_IS_FREE(dst_phdr)) return ERR_NIFFS_MOVING_TO_UNFREE_PAGE;

  // mark src as moving
  int res = NIFFS_OK;
  if (!_NIFFS_IS_MOVI(src_phdr)) {
    niffs_flag moving_flag = _NIFFS_FLAG_MOVING;
    res = fs->hal_wr((u8_t *)src_phdr + offsetof(niffs_page_hdr, flag), (u8_t *)&moving_flag, sizeof(niffs_flag));
    if (res != NIFFS_OK) return res;
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

  // update watch page index
  if (fs->watch_pix) {
    if (*fs->watch_pix == src_pix) {
      *fs->watch_pix = dst_pix;
    }
  }
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

  res = niffs_ensure_free_pages(fs, 1);
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
  strncpy((char *)ohdr.name, (char *)name, NIFFS_NAME_LEN);
  niffs_page_hdr *phdr_addr = (niffs_page_hdr *)_NIFFS_PIX_2_ADDR(fs, pix);

  res = fs->hal_wr((u8_t *)phdr_addr + offsetof(niffs_object_hdr, phdr) + sizeof(niffs_page_hdr), (u8_t *)&ohdr + offsetof(niffs_object_hdr, len),
      sizeof(niffs_object_hdr) - sizeof(niffs_page_hdr));

  return res;
}

typedef struct {
  char *name;
  niffs_page_ix pix;
  niffs_obj_id id;
  niffs_page_ix pix_mov;
  niffs_obj_id id_mov;
} niffs_open_arg;

static int niffs_open_v(niffs *fs, niffs_page_ix pix, niffs_page_hdr *phdr, void *v_arg) {
  (void)pix;
  if (!_NIFFS_IS_FREE(phdr) && !_NIFFS_IS_DELE(phdr)) {
    if (_NIFFS_IS_OBJ_HDR(phdr)) {
      // object header page
      niffs_object_hdr *ohdr = (niffs_object_hdr *)phdr;
      niffs_open_arg *arg = (niffs_open_arg *)v_arg;
      if (strcmp(arg->name, (char *)ohdr->name) == 0) {
        // found matching name
        if (arg->id_mov) {
          // had a previou moving page - delete this
          int res = niffs_delete_page(fs, arg->pix_mov);
          if (res != NIFFS_OK) return res;
          arg->id_mov = 0;
        }

        if (_NIFFS_IS_MOVI(phdr)) {
          arg->id_mov = ohdr->phdr.id.obj_id;
          arg->pix_mov = pix;
          return NIFFS_VIS_CONT;
        } else {
          arg->id = ohdr->phdr.id.obj_id;
          arg->pix = pix;
          return NIFFS_OK;
        }
      }
    }
  }
  return NIFFS_VIS_CONT;
}

TESTATIC int niffs_open(niffs *fs, char *name) {
  int fd_ix;
  int res = NIFFS_OK;

  if (name == 0) return ERR_NIFFS_NULL_PTR;

  niffs_file_desc *fd = niffs_get_free_fd(fs, &fd_ix);
  if (fd == 0) return ERR_NIFFS_OUT_OF_FILEDESCS;

  niffs_open_arg arg;
  memset(&arg, 0, sizeof(arg));
  arg.name = name;
  res = niffs_traverse(fs, 0, 0, niffs_open_v, &arg);
  if (res == NIFFS_VIS_END) {
    if (arg.id_mov != 0) {
      NIFFS_DBG("open: found only movi page\n");
      // tidy up found movi page
      // TODO res = niffs_tidy_movi_obj_hdr(fs, &arg.pix);
      if (res != NIFFS_OK) return res;
      arg.id = arg.id_mov;
      arg.pix = arg.pix_mov;
    } else {
      return ERR_NIFFS_FILE_NOT_FOUND;
    }
  } else if (res != NIFFS_OK) {
    return res;
  }

  memset(fd, 0, sizeof(niffs_file_desc));
  fd->obj_id = arg.id;
  fd->obj_pix = arg.pix;

  return res == NIFFS_OK ? fd_ix : res;
}

TESTATIC int niffs_append(niffs *fs, int fd_ix, u8_t *src, u32_t len) {
  int res = NIFFS_OK;
  if (fd_ix < 0 || fd_ix >= (int)fs->descs_len) return ERR_NIFFS_FILEDESC_BAD;
  if (fs->descs[fd_ix].obj_id == 0) return ERR_NIFFS_FILEDESC_CLOSED;
  if (len == 0) return NIFFS_OK;
  niffs_file_desc *fd = &fs->descs[fd_ix];
  niffs_object_hdr *ohdr = (niffs_object_hdr *)_NIFFS_PIX_2_ADDR(fs, fd->obj_pix);
  if (ohdr->phdr.id.obj_id != fd->obj_id) return ERR_NIFFS_FATAL_INCOHERENT_ID;

  u32_t file_offs = ohdr->len == NIFFS_UNDEF_LEN ? 0 : ohdr->len;
  if (file_offs == 0 && _NIFFS_OFFS_2_SPIX(fs, len) == 0) {
    // no need to allocate a new page, just fill in existing file
  } else {
    // one extra for new object header
    u32_t needed_pages = _NIFFS_OFFS_2_SPIX(fs, len + file_offs) - _NIFFS_OFFS_2_SPIX(fs, len) + 1 + 1;
    res = niffs_ensure_free_pages(fs, needed_pages);
  }
  if (res != NIFFS_OK) return res;

  u32_t offs = 0;
  u32_t written = 0;
  if (file_offs == 0 && _NIFFS_IS_WRIT(&ohdr->phdr)) {
    // changing existing file - write flag, mark obj header as MOVI
    niffs_flag flag = _NIFFS_FLAG_MOVING;
    res = fs->hal_wr((u8_t *)ohdr + offsetof(niffs_object_hdr, phdr) + offsetof(niffs_page_hdr, flag), (u8_t *)&flag, sizeof(niffs_flag));
  }

  // operate on per page basis
  while (res == NIFFS_OK && written < len) {
    u32_t avail;
    // case 1: newly created file, fill in object header
    if (file_offs + offs == 0) {
      // just fill up obj header
      avail = MIN(len, _NIFFS_SPIX_2_PDATA_LEN(fs, 0));
      // write flag ..
      niffs_flag flag = _NIFFS_FLAG_WRITTEN;
      res = fs->hal_wr((u8_t *)ohdr + offsetof(niffs_object_hdr, phdr) + offsetof(niffs_page_hdr, flag), (u8_t *)&flag, sizeof(niffs_flag));
      if (res != NIFFS_OK) return res;
      // .. data ..
      res = fs->hal_wr((u8_t *)ohdr + sizeof(niffs_object_hdr), (u8_t *)src, avail);
      if (res != NIFFS_OK) return res;
    }
    // case 2: add a new page
    else if (_NIFFS_OFFS_2_SPIX_OFFS(fs, file_offs + offs) == 0) {
      // find and write header..
      avail = MIN(len - written, _NIFFS_SPIX_2_PDATA_LEN(fs, 1));
      niffs_page_ix new_pix;
      res = niffs_find_free_page(fs, &new_pix, NIFFS_EXCL_SECT_NONE);
      if (res != NIFFS_OK) return res;
      niffs_page_hdr new_phdr;
      new_phdr.id.obj_id = fd->obj_id;
      new_phdr.id.spix = _NIFFS_OFFS_2_SPIX(fs, file_offs + offs);
      new_phdr.id.cycle_ix = 0;
      new_phdr.flag = _NIFFS_FLAG_CLEAN;
      res = niffs_write_phdr(fs, new_pix, &new_phdr);
      if (res != NIFFS_OK) return res;

      // .. write data ..
      res = fs->hal_wr((u8_t *)_NIFFS_PIX_2_ADDR(fs, new_pix) + sizeof(niffs_page_hdr), (u8_t *)src, avail);
      if (res != NIFFS_OK) return res;

      // finalize flag
      niffs_flag flag = _NIFFS_FLAG_WRITTEN;
      res = fs->hal_wr((u8_t *)_NIFFS_PIX_2_ADDR(fs, new_pix) + offsetof(niffs_page_hdr, flag), (u8_t *)&flag, sizeof(niffs_flag));
      if (res != NIFFS_OK) return res;
    }

    // case 3: rewrite a page
    // TODO

    src += avail;
    offs += avail;
    written += avail;
  }

  if (res != NIFFS_OK) return res;

  // TODO move original object hdr if necessary

  // .. write length
  u32_t length = len + file_offs;
  res = fs->hal_wr((u8_t *)ohdr + offsetof(niffs_object_hdr, len), (u8_t *)&length, sizeof(u32_t));

  return res;
}

// TODO check
// sector headers for magic
// _NIFFS_IS_FREE && _NIFFS_IS_WRIT => aborted in midst of moving

int NIFFS_init(niffs *fs, u8_t *phys_addr, u32_t sectors, u32_t sector_size, u32_t page_size,
    u8_t *buf, u32_t buf_len, niffs_file_desc *descs, u32_t file_desc_len,
    niffs_hal_erase_f erase_f, niffs_hal_write_f write_f) {
  fs->phys_addr = phys_addr;
  fs->sectors = sectors;
  fs->sector_size = sector_size;
  fs->buf = buf;
  fs->buf_len = buf_len;
  fs->hal_er = erase_f;
  fs->hal_wr = write_f;
  fs->descs = descs;
  fs->descs_len = file_desc_len;
  fs->last_free_pix = 0;

  u32_t pages_per_sector = sector_size / page_size;
  memset(descs, 0, file_desc_len * sizeof(niffs_file_desc));

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

