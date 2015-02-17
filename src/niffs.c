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
  while (fs->free_pages < fs->pages_per_sector) {
    NIFFS_DBG("ensure warn: fs crammed, free pages:%i, need at least:%i\n", fs->free_pages, fs->pages_per_sector);
    // crammed, even the spare sector is dirty
    if (fs->dele_pages < (fs->pages_per_sector - fs->free_pages)) {
      // cannot ensure even the spare sector
      return ERR_NIFFS_FULL;
    }
    u32_t freed_pages;
    res = niffs_gc(fs, &freed_pages);
    if (res != NIFFS_OK) return res;
  }

  if (pages > (fs->dele_pages + fs->free_pages - fs->pages_per_sector)) {
    // this will never fit without deleting stuff
    return ERR_NIFFS_FULL;
  }

  if (pages > fs->free_pages || fs->free_pages - pages < fs->pages_per_sector) {
    // try cleaning away needed pages
    NIFFS_DBG("ensure: need %i free, have %i-%i\n", pages, fs->free_pages, fs->pages_per_sector);
    do {
      u32_t freed_pages;
      res = niffs_gc(fs, &freed_pages);
      if (res != NIFFS_OK) return res;
      pages -= MIN(pages, freed_pages);
    } while (pages > 0);
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
      niffs_obj_id oid = phdr->id.obj_id;
      --oid;
      fs->buf[oid/8] |= 1<<(oid&7);
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

static void niffs_inform_movement(niffs *fs, niffs_page_ix src_pix, niffs_page_ix dst_pix) {

  // update descriptors
  u32_t i;
  for (i = 0; i < fs->descs_len; i++) {
    if (fs->descs[i].obj_id != 0) {
      if (fs->descs[i].cur_pix == src_pix) {
        NIFFS_DBG("inform: pix update (fd%icur): %04x->%04x\n", i, fs->descs[i].cur_pix, dst_pix);
        fs->descs[i].cur_pix = dst_pix;
      }
      if (fs->descs[i].obj_pix == src_pix) {
        NIFFS_DBG("inform: pix update (fd%iobj): %04x->%04x\n", i, fs->descs[i].obj_pix, dst_pix);
        fs->descs[i].obj_pix = dst_pix;
      }
    }
  }
}

TESTATIC int niffs_find_free_id(niffs *fs, niffs_obj_id *oid, char *conflict_name) {
  if (oid == 0) return ERR_NIFFS_NULL_PTR;
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
        *oid = (cur_id + bit_ix) + 1;
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

typedef struct {
  niffs_page_ix pix;
  u8_t mov_found;
  niffs_page_ix pix_mov;
  niffs_obj_id oid;
  niffs_span_ix spix;
} niffs_find_page_arg;

static int niffs_find_page_v(niffs *fs, niffs_page_ix pix, niffs_page_hdr *phdr, void *v_arg) {
  niffs_find_page_arg *arg = (niffs_find_page_arg *)v_arg;
  if (!_NIFFS_IS_FREE(phdr) && !_NIFFS_IS_DELE(phdr) &&
      _NIFFS_IS_ID_VALID(phdr) &&
      phdr->id.obj_id == arg->oid && phdr->id.spix == arg->spix) {
    if (arg->mov_found) {
      // had a previous moving page - delete this
      int res = niffs_delete_page(fs, arg->pix_mov);
      if (res != NIFFS_OK) return res;
      arg->mov_found = 0;
    }

    if (_NIFFS_IS_MOVI(phdr)) {
      arg->mov_found = 1;
      arg->pix_mov = pix;
      return NIFFS_VIS_CONT;
    } else {
      arg->pix = pix;
      return NIFFS_OK;
    }
  }
  return NIFFS_VIS_CONT;
}

TESTATIC int niffs_find_page(niffs *fs, niffs_page_ix *pix, niffs_obj_id oid, niffs_span_ix spix, niffs_page_ix start_pix) {
  if (pix == 0) return ERR_NIFFS_NULL_PTR;

  niffs_find_page_arg arg;
  arg.oid = oid;
  arg.spix = spix;
  arg.mov_found = 0;
  int res = niffs_traverse(fs, start_pix, start_pix, niffs_find_page_v, &arg);
  if (res == NIFFS_VIS_END) {
    if (arg.mov_found) {
      NIFFS_DBG("find  : warn found MOVI page %04x when looking for obj id:%04x spix:%i\n", arg.pix_mov, oid, spix);
      *pix = arg.pix_mov;
    } else {
      res = ERR_NIFFS_PAGE_NOT_FOUND;
    }
  } else {
    *pix = arg.pix;
  }
  return res;
}

TESTATIC int niffs_erase_sector(niffs *fs, u32_t sector_ix) {
  niffs_sector_hdr shdr;
  niffs_sector_hdr *target_shdr = (niffs_sector_hdr *)_NIFFS_SECTOR_2_ADDR(fs, sector_ix);
  if (target_shdr->abra == _NIFFS_SECT_MAGIC(fs) && target_shdr->era_cnt != (niffs_erase_cnt)-1) {
    shdr.era_cnt = target_shdr->era_cnt+1;
    fs->max_era = MAX(shdr.era_cnt, fs->max_era);
  } else {
    shdr.era_cnt = fs->max_era;
  }
  shdr.abra = _NIFFS_SECT_MAGIC(fs);
  NIFFS_DBG("erase : sector %i era_cnt:%i\n", sector_ix, shdr.era_cnt);

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
  NIFFS_DBG("delete: pix %04x\n", pix);
  if (res == NIFFS_OK) {
    fs->dele_pages++;
  }
  return res;
}

TESTATIC int niffs_move_page(niffs *fs, niffs_page_ix src_pix, niffs_page_ix dst_pix, u8_t *data, u32_t len, niffs_flag force_flag) {
  if (src_pix == dst_pix) return ERR_NIFFS_MOVING_TO_SAME_PAGE;

  niffs_page_hdr *src_phdr = (niffs_page_hdr *)_NIFFS_PIX_2_ADDR(fs, src_pix);
  niffs_page_hdr *dst_phdr = (niffs_page_hdr *)_NIFFS_PIX_2_ADDR(fs, dst_pix);

  if (_NIFFS_IS_FREE(src_phdr)) return ERR_NIFFS_MOVING_FREE_PAGE;
  if (_NIFFS_IS_DELE(src_phdr)) return ERR_NIFFS_MOVING_DELETED_PAGE;
  if (!_NIFFS_IS_FREE(dst_phdr)) return ERR_NIFFS_MOVING_TO_UNFREE_PAGE;

  u8_t src_clear = _NIFFS_IS_CLEA(src_phdr);
  u8_t src_movi = _NIFFS_IS_MOVI(src_phdr);

  NIFFS_DBG("move  : pix %04x->%04x flag:%s\n", src_pix, dst_pix, _NIFFS_IS_CLEA(src_phdr) ? ("CLEA") :
      (_NIFFS_IS_MOVI(src_phdr) ? "MOVI" : "WRIT"));

  // mark src as moving
  int res = NIFFS_OK;
  if (!_NIFFS_IS_MOVI(src_phdr)) {
    niffs_flag moving_flag = _NIFFS_FLAG_MOVING;
    res = fs->hal_wr((u8_t *)src_phdr + offsetof(niffs_page_hdr, flag), (u8_t *)&moving_flag, sizeof(niffs_flag));
    if (res != NIFFS_OK) return res;
  }

  // write dst..
  // .. write flag ..
  niffs_flag flag = _NIFFS_FLAG_CLEAN;
  if (force_flag == NIFFS_FLAG_MOVE_KEEP) {
    if (!src_clear) {
      flag = src_movi ? _NIFFS_FLAG_MOVING : _NIFFS_FLAG_WRITTEN;
    }
  } else {
    flag = force_flag;
  }
  if (flag != _NIFFS_FLAG_CLEAN) {
    res = fs->hal_wr((u8_t *)dst_phdr + offsetof(niffs_page_hdr, flag), (u8_t *)&flag, sizeof(niffs_flag));
    if (res != NIFFS_OK) return res;
  }

  fs->free_pages--;
  if (data == 0 && (!src_clear || !src_phdr->id.spix != 0)) {
    // .. page data ..
    res = fs->hal_wr((u8_t *)dst_phdr + sizeof(niffs_page_hdr), (u8_t *)src_phdr  + sizeof(niffs_page_hdr), fs->page_size - sizeof(niffs_page_hdr));
    if (res != NIFFS_OK) return res;
  } else if (data) {
    // .. else, user data ..
    res = fs->hal_wr((u8_t *)dst_phdr + sizeof(niffs_page_hdr), data, len);
    if (res != NIFFS_OK) return res;
  }
  // .. and id
  res = fs->hal_wr((u8_t *)dst_phdr + offsetof(niffs_page_hdr, id), (u8_t *)src_phdr  + offsetof(niffs_page_hdr, id), sizeof(niffs_page_hdr_id));
  if (res != NIFFS_OK) return res;

  niffs_inform_movement(fs, src_pix, dst_pix);

  // delete src
  res = niffs_delete_page(fs, src_pix);

  return res;
}

TESTATIC int niffs_write_page(niffs *fs, niffs_page_ix pix, niffs_page_hdr *phdr, u8_t *data, u32_t len) {
  niffs_page_hdr *phdr_addr = (niffs_page_hdr *)_NIFFS_PIX_2_ADDR(fs, pix);

  if (phdr->id.obj_id == 0 || phdr->id.obj_id == (niffs_obj_id)-1) {
    return ERR_NIFFS_WR_PHDR_BAD_ID;
  }
  if (!_NIFFS_IS_FREE(phdr_addr) || !_NIFFS_IS_CLEA(phdr_addr)) {
    return ERR_NIFFS_WR_PHDR_UNFREE_PAGE;
  }

  NIFFS_ASSERT(data == 0 || len <= _NIFFS_SPIX_2_PDATA_LEN(fs, 1));

  NIFFS_DBG("write : pix %04x %s oid:%04x spix:%i\n", pix, data ? "DATA":"NODATA", phdr->id.obj_id, phdr->id.spix);

  int res;
  if (!_NIFFS_IS_CLEA(phdr)) {
    // first, write flag
    res = fs->hal_wr((u8_t *)phdr_addr + offsetof(niffs_page_hdr, flag), (u8_t *)&phdr->flag, sizeof(niffs_flag));
    if (res != NIFFS_OK) return res;
  }
  // .. data ..
  if (data) {
    res = fs->hal_wr((u8_t *)phdr_addr + sizeof(niffs_page_hdr), data, len);
    if (res != NIFFS_OK) return res;
  }

  // .. then id
  res = fs->hal_wr((u8_t *)phdr_addr + offsetof(niffs_page_hdr, id), (u8_t *)&phdr->id, sizeof(niffs_page_hdr_id));

  return res;
}

TESTATIC int niffs_write_phdr(niffs *fs, niffs_page_ix pix, niffs_page_hdr *phdr) {
  return niffs_write_page(fs, pix, phdr, 0, 0);
}

TESTATIC int niffs_create(niffs *fs, char *name) {
  niffs_obj_id oid;
  niffs_page_ix pix;
  int res;

  if (name == 0) return ERR_NIFFS_NULL_PTR;

  res = niffs_ensure_free_pages(fs, 1);
  if (res != NIFFS_OK) return res;

  res = niffs_find_free_id(fs, &oid, name);
  if (res != NIFFS_OK) return res;

  res = niffs_find_free_page(fs, &pix, NIFFS_EXCL_SECT_NONE);
  if (res != NIFFS_OK) return res;

  NIFFS_DBG("create: pix %04x oid:%04x name:%s\n", pix, oid, name);

  // write page header for object header
  niffs_object_hdr ohdr;
  ohdr.phdr.flag = _NIFFS_FLAG_CLEAN;
  ohdr.phdr.id.obj_id = oid;
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
  niffs_obj_id oid;
  niffs_page_ix pix_mov;
  niffs_obj_id oid_mov;
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
        if (arg->oid_mov) {
          // had a previous moving page - delete this
          int res = niffs_delete_page(fs, arg->pix_mov);
          if (res != NIFFS_OK) return res;
          arg->oid_mov = 0;
        }

        if (_NIFFS_IS_MOVI(phdr)) {
          arg->oid_mov = ohdr->phdr.id.obj_id;
          arg->pix_mov = pix;
          return NIFFS_VIS_CONT;
        } else {
          arg->oid = ohdr->phdr.id.obj_id;
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
    if (arg.oid_mov != 0) {
      NIFFS_DBG("open  : found only movi page @ %04x\n", arg.pix_mov);
      // tidy up found movi page
      // TODO res = niffs_tidy_movi_obj_hdr(fs, &arg.pix);
      if (res != NIFFS_OK) return res;
      arg.oid = arg.oid_mov;
      arg.pix = arg.pix_mov;
    } else {
      return ERR_NIFFS_FILE_NOT_FOUND;
    }
  } else if (res != NIFFS_OK) {
    return res;
  }

  memset(fd, 0, sizeof(niffs_file_desc));
  fd->obj_id = arg.oid;
  fd->obj_pix = arg.pix;
  fd->cur_pix = arg.pix;

  return res == NIFFS_OK ? fd_ix : res;
}

TESTATIC int niffs_close(niffs *fs, int fd_ix) {
  int res = NIFFS_OK;

  if (fd_ix < 0 || fd_ix >= (int)fs->descs_len) return ERR_NIFFS_FILEDESC_BAD;
  if (fs->descs[fd_ix].obj_id == 0) return ERR_NIFFS_FILEDESC_CLOSED;

  niffs_file_desc *fd = &fs->descs[fd_ix];

  memset(fd, 0, sizeof(niffs_file_desc));

  return res;
}

TESTATIC int niffs_read_ptr(niffs *fs, int fd_ix, u8_t **data, u32_t *avail) {
  if (fd_ix < 0 || fd_ix >= (int)fs->descs_len) return ERR_NIFFS_FILEDESC_BAD;
  if (fs->descs[fd_ix].obj_id == 0) return ERR_NIFFS_FILEDESC_CLOSED;
  niffs_file_desc *fd = &fs->descs[fd_ix];
  niffs_object_hdr *ohdr = (niffs_object_hdr *)_NIFFS_PIX_2_ADDR(fs, fd->obj_pix);
  if (fd->offs >= ohdr->len) {
    *data = 0;
    *avail = 0;
    return ERR_NIFFS_END_OF_FILE;
  }
  if (_NIFFS_IS_DELE(&ohdr->phdr)) return ERR_NIFFS_PAGE_DELETED;
  if (_NIFFS_IS_FREE(&ohdr->phdr)) return ERR_NIFFS_PAGE_FREE;
  if (ohdr->phdr.id.obj_id != fd->obj_id) return ERR_NIFFS_INCOHERENT_ID;

  niffs_page_hdr *phdr = (niffs_page_hdr *)_NIFFS_PIX_2_ADDR(fs, fd->cur_pix);
  u32_t rem_tot = ohdr->len - fd->offs;
  u32_t rem_page = _NIFFS_SPIX_2_PDATA_LEN(fs, phdr->id.spix) - _NIFFS_OFFS_2_PDATA_OFFS(fs, fd->offs);
  u32_t avail_data = MIN(rem_tot, rem_page);

  // make sure span index is coherent
  if (phdr->id.spix != _NIFFS_OFFS_2_SPIX(fs, fd->offs)) {
    niffs_page_ix pix;
    int res = niffs_find_page(fs, &pix, fd->obj_id, _NIFFS_OFFS_2_SPIX(fs, fd->offs), fd->cur_pix);
    if (res != NIFFS_OK) return res;
    fd->cur_pix = pix;
    phdr = (niffs_page_hdr *)_NIFFS_PIX_2_ADDR(fs, fd->cur_pix);
  }

  *data = (u8_t *)phdr + _NIFFS_OFFS_2_PDATA_OFFS(fs, fd->offs) +
      (phdr->id.spix == 0 ? sizeof(niffs_object_hdr) : sizeof(niffs_page_hdr));
  *avail = avail_data;

  return avail_data;
}

TESTATIC int niffs_seek(niffs *fs, int fd_ix, u8_t whence, s32_t offset) {
  int res = NIFFS_OK;
  if (fd_ix < 0 || fd_ix >= (int)fs->descs_len) return ERR_NIFFS_FILEDESC_BAD;
  if (fs->descs[fd_ix].obj_id == 0) return ERR_NIFFS_FILEDESC_CLOSED;
  niffs_file_desc *fd = &fs->descs[fd_ix];
  niffs_object_hdr *ohdr = (niffs_object_hdr *)_NIFFS_PIX_2_ADDR(fs, fd->obj_pix);

  s32_t coffs;
  switch (whence) {
  default:
  case NIFFS_SEEK_SET:
    coffs = offset;
    break;
  case NIFFS_SEEK_CUR:
    coffs = fd->offs + offset;
    break;
  case NIFFS_SEEK_END:
    coffs = ohdr->len + offset;
    break;
  }

  if (coffs < 0) {
    coffs = 0;
  } else {
    coffs = MIN(ohdr->len, (u32_t)coffs);
  }

  if (_NIFFS_OFFS_2_SPIX(fs, (u32_t)coffs) != _NIFFS_OFFS_2_SPIX(fs, fd->offs)) {
    // new page
    if (!((u32_t)coffs == ohdr->len && _NIFFS_OFFS_2_PDATA_OFFS(fs, (u32_t)coffs) == 0)) {
      niffs_page_ix seek_pix;
      res = niffs_find_page(fs, &seek_pix, fd->obj_id, _NIFFS_OFFS_2_SPIX(fs, (u32_t)coffs), fd->cur_pix);
      if (res != NIFFS_OK) return res;
      fd->cur_pix = seek_pix;
    }
  }
  fd->offs = (u32_t)coffs;

  return res;
}

TESTATIC int niffs_append(niffs *fs, int fd_ix, u8_t *src, u32_t len) {
  int res = NIFFS_OK;
  if (fd_ix < 0 || fd_ix >= (int)fs->descs_len) return ERR_NIFFS_FILEDESC_BAD;
  if (fs->descs[fd_ix].obj_id == 0) return ERR_NIFFS_FILEDESC_CLOSED;
  if (len == 0) return NIFFS_OK;
  niffs_file_desc *fd = &fs->descs[fd_ix];
  niffs_object_hdr *orig_ohdr = (niffs_object_hdr *)_NIFFS_PIX_2_ADDR(fs, fd->obj_pix);
  if (orig_ohdr->phdr.id.obj_id != fd->obj_id) return ERR_NIFFS_INCOHERENT_ID;

  // CHECK SPACE
  u32_t file_offs = orig_ohdr->len == NIFFS_UNDEF_LEN ? 0 : orig_ohdr->len;
  if (file_offs == 0 && _NIFFS_OFFS_2_SPIX(fs, len-1) == 0) {
    // no need to allocate a new page, just fill in existing file
  } else {
    // one extra for new object header
    u32_t needed_pages = _NIFFS_OFFS_2_SPIX(fs, len + file_offs) - _NIFFS_OFFS_2_SPIX(fs, file_offs) +
        (_NIFFS_OFFS_2_PDATA_OFFS(fs, len + file_offs) == 0 ? -1 : 0) +
        (file_offs == 0 ? 0 : 1);
    res = niffs_ensure_free_pages(fs, needed_pages);
  }
  if (res != NIFFS_OK) return res;

  // repopulate if moved by gc
  orig_ohdr = (niffs_object_hdr *)_NIFFS_PIX_2_ADDR(fs, fd->obj_pix);
  if (orig_ohdr->phdr.id.obj_id != fd->obj_id) return ERR_NIFFS_INCOHERENT_ID;

  u32_t data_offs = 0;
  u32_t written = 0;
  if (file_offs > 0 && _NIFFS_IS_WRIT(&orig_ohdr->phdr)) {
    // changing existing file - write flag, mark obj header as MOVI
    niffs_flag flag = _NIFFS_FLAG_MOVING;
    res = fs->hal_wr((u8_t *)orig_ohdr + offsetof(niffs_object_hdr, phdr) + offsetof(niffs_page_hdr, flag), (u8_t *)&flag, sizeof(niffs_flag));
  }

  niffs_object_hdr *new_ohdr = 0;

  // WRITE DATA
  // operate on per page basis
  while (res == NIFFS_OK && written < len) {
    u32_t avail;

    // case 1: newly created empty file, fill in object header
    if (file_offs + data_offs == 0) {
      // just fill up obj header
      avail = MIN(len, _NIFFS_SPIX_2_PDATA_LEN(fs, 0));
      NIFFS_DBG("append: obj hdr pix %04x oid:%04x spix:0 len:%i\n", fd->obj_pix, fd->obj_id, avail);
      // .. data ..
      res = fs->hal_wr((u8_t *)orig_ohdr + sizeof(niffs_object_hdr), src, avail);
      if (res != NIFFS_OK) return res;

      new_ohdr = orig_ohdr;
    }

    // case 2: add a new page
    else if (_NIFFS_OFFS_2_PDATA_OFFS(fs, file_offs + data_offs) == 0) {
      // find new page
      avail = MIN(len - written, _NIFFS_SPIX_2_PDATA_LEN(fs, 1));
      niffs_page_ix new_pix;
      res = niffs_find_free_page(fs, &new_pix, NIFFS_EXCL_SECT_NONE);
      if (res != NIFFS_OK) return res;


      niffs_page_hdr new_phdr;
      new_phdr.id.obj_id = fd->obj_id;
      new_phdr.id.spix = _NIFFS_OFFS_2_SPIX(fs, file_offs + data_offs);
      new_phdr.id.cycle_ix = 0;
      new_phdr.flag = _NIFFS_FLAG_WRITTEN;
      NIFFS_DBG("append: full page pix %04x oid:%04x spix:%i len:%i\n", new_pix, fd->obj_id, new_phdr.id.spix, avail);

      res = niffs_write_page(fs, new_pix, &new_phdr, src, avail);
      if (res != NIFFS_OK) return res;
      fs->free_pages--;
      fd->cur_pix = new_pix;
    }

    // case 3: rewrite a page
    else {
      niffs_page_ix src_pix;
      if (_NIFFS_OFFS_2_SPIX(fs, file_offs + data_offs) == 0) {
        // rewriting object header page
        src_pix = fd->obj_pix;
      } else {
        // rewriting plain data page, so go get it
        res = niffs_find_page(fs, &src_pix, fd->obj_id, _NIFFS_OFFS_2_SPIX(fs, file_offs + data_offs), fd->cur_pix);
        if (res != NIFFS_OK) return res;
      }

      // find new page
      avail = MIN(len - written,
          _NIFFS_SPIX_2_PDATA_LEN(fs, _NIFFS_OFFS_2_SPIX(fs, file_offs + data_offs)) -
          _NIFFS_OFFS_2_PDATA_OFFS(fs, file_offs + data_offs));
      niffs_page_ix new_pix;
      res = niffs_find_free_page(fs, &new_pix, NIFFS_EXCL_SECT_NONE);
      if (res != NIFFS_OK) return res;

      // modify page
      if (_NIFFS_OFFS_2_SPIX(fs, file_offs + data_offs) == 0) {
        NIFFS_DBG("append: modify obj hdr pix %04x oid:%04x spix:%i len:%i\n", fd->obj_pix, fd->obj_id, 0, avail);
        // rewriting object header, include object header data
        // copy header and current data
        memcpy(fs->buf, (u8_t *)_NIFFS_PIX_2_ADDR(fs, src_pix), sizeof(niffs_object_hdr) + file_offs);
        // copy from new data
        memcpy(&fs->buf[sizeof(niffs_object_hdr) + file_offs], src, avail);

        // reset new object header to be written
        niffs_object_hdr *new_ohdr_data = (niffs_object_hdr *)(fs->buf);
        new_ohdr_data->len = NIFFS_UNDEF_LEN;
        new_ohdr_data->phdr.flag = _NIFFS_FLAG_CLEAN;
        new_ohdr = (niffs_object_hdr *)_NIFFS_PIX_2_ADDR(fs, new_pix);
        res = niffs_write_page(fs, new_pix, &new_ohdr_data->phdr, &fs->buf[sizeof(niffs_page_hdr)],
            _NIFFS_SPIX_2_PDATA_LEN(fs, 1));
        if (res != NIFFS_OK) return res;
        fs->free_pages--;
      } else {
        // rewrite plain data page
        // copy from src
        memcpy(fs->buf,
            (u8_t *)_NIFFS_PIX_2_ADDR(fs, src_pix) +
              (_NIFFS_OFFS_2_SPIX(fs, file_offs + data_offs) == 0 ? sizeof(niffs_object_hdr) : sizeof(niffs_page_hdr)),
            _NIFFS_OFFS_2_PDATA_OFFS(fs, file_offs + data_offs));
        // copy from new data
        memcpy(&fs->buf[ _NIFFS_OFFS_2_PDATA_OFFS(fs, file_offs + data_offs)],
            src, avail);
        NIFFS_DBG("append: modify page pix %04x oid:%04x spix:%i len:%i\n", src_pix, fd->obj_id, (u32_t)_NIFFS_OFFS_2_SPIX(fs, file_offs + data_offs), avail);
        // move page, rewrite data
        res = niffs_move_page(fs, src_pix, new_pix, fs->buf, _NIFFS_SPIX_2_PDATA_LEN(fs, 1), _NIFFS_FLAG_WRITTEN);
        if (res != NIFFS_OK) return res;
      }

      fd->cur_pix = new_pix;
    }

    src += avail;
    data_offs += avail;
    written += avail;
    fd->offs = written;
  }

  if (res != NIFFS_OK) return res;

  // HEADER UPDATE
  // move original object header if necessary
  if (new_ohdr == 0) {
    // find free page
    niffs_page_ix new_pix;
    res = niffs_find_free_page(fs, &new_pix, NIFFS_EXCL_SECT_NONE);
    if (res != NIFFS_OK) return res;

    new_ohdr = (niffs_object_hdr *)_NIFFS_PIX_2_ADDR(fs, new_pix);

    // copy from old hdr
    memcpy(fs->buf, (u8_t *)orig_ohdr, fs->page_size);

    ((niffs_object_hdr *)fs->buf)->len = len + file_offs;

    // move header page, rewrite length data
    res = niffs_move_page(fs, fd->obj_pix, new_pix, fs->buf + sizeof(niffs_page_hdr), fs->page_size - sizeof(niffs_page_hdr), _NIFFS_FLAG_WRITTEN);
    if (res != NIFFS_OK) return res;
  } else {
    // just fill in clean object header
    // .. write length..
    u32_t length = len + file_offs;
    res = fs->hal_wr((u8_t *)new_ohdr + offsetof(niffs_object_hdr, len), (u8_t *)&length, sizeof(u32_t));
    if (res != NIFFS_OK) return res;
    // .. write flag..
    niffs_flag flag = _NIFFS_FLAG_WRITTEN;
    res = fs->hal_wr((u8_t *)new_ohdr + offsetof(niffs_object_hdr, phdr) + offsetof(niffs_page_hdr, flag), (u8_t *)&flag, sizeof(niffs_flag));
    if (res != NIFFS_OK) return res;
    // check if object header moved
    if (new_ohdr != orig_ohdr) {
      niffs_page_ix old_obj_pix = fd->obj_pix;
      niffs_inform_movement(fs, old_obj_pix, _NIFFS_ADDR_2_PIX(fs, new_ohdr));
      // .. and remove old
      res = niffs_delete_page(fs, old_obj_pix);
      if (res != NIFFS_OK) return res;
    }
  }

  return res;
}

TESTATIC int niffs_modify(niffs *fs, int fd_ix, u32_t offset, u8_t *src, u32_t len) {
  int res = NIFFS_OK;
  if (fd_ix < 0 || fd_ix >= (int)fs->descs_len) return ERR_NIFFS_FILEDESC_BAD;
  if (fs->descs[fd_ix].obj_id == 0) return ERR_NIFFS_FILEDESC_CLOSED;
  if (len == 0) return NIFFS_OK;
  niffs_file_desc *fd = &fs->descs[fd_ix];
  niffs_object_hdr *orig_ohdr = (niffs_object_hdr *)_NIFFS_PIX_2_ADDR(fs, fd->obj_pix);
  if (orig_ohdr->phdr.id.obj_id != fd->obj_id) return ERR_NIFFS_INCOHERENT_ID;
  u32_t file_offs = orig_ohdr->len == NIFFS_UNDEF_LEN ? 0 : orig_ohdr->len;
  if (offset + len > file_offs) {
    return ERR_NIFFS_MODIFY_BEYOND_FILE;
  }

  // CHECK SPACE
  niffs_span_ix start = _NIFFS_OFFS_2_SPIX(fs, offset);
  niffs_span_ix end = _NIFFS_OFFS_2_SPIX(fs, offset+len);

  u32_t needed_pages = end-start+1;
  res = niffs_ensure_free_pages(fs, needed_pages);

  if (res != NIFFS_OK) return res;

  // repopulate if moved by gc
  orig_ohdr = (niffs_object_hdr *)_NIFFS_PIX_2_ADDR(fs, fd->obj_pix);
  if (orig_ohdr->phdr.id.obj_id != fd->obj_id) return ERR_NIFFS_INCOHERENT_ID;

  u32_t written = 0;

  // WRITE DATA
  niffs_page_ix search_pix = fd->obj_pix;
  // operate on per page basis
  while (res == NIFFS_OK && written < len) {
    u32_t avail;
    niffs_span_ix spix = _NIFFS_OFFS_2_SPIX(fs, offset + written);
    u32_t pdata_len = _NIFFS_SPIX_2_PDATA_LEN(fs, spix);
    u32_t pdata_offs = _NIFFS_OFFS_2_PDATA_OFFS(fs, offset + written);
    avail = pdata_len - pdata_offs;
    avail = MIN(len - written, avail);

    u32_t buf_offs = 0;
    if (spix == 0) {
      // copy object hdr data
      memcpy(fs->buf, (u8_t *)orig_ohdr + sizeof(niffs_page_hdr), sizeof(niffs_object_hdr) - sizeof(niffs_page_hdr));
      buf_offs = sizeof(niffs_object_hdr) - sizeof(niffs_page_hdr);
    }

    // find original page
    niffs_page_ix orig_pix;
    res = niffs_find_page(fs, &orig_pix, fd->obj_id, spix, search_pix);
    if (res != NIFFS_OK) return res;
    search_pix = orig_pix;

    NIFFS_DBG("modify: pix:%04x oid:%04x spix:%i offs:%i len:%i\n", orig_pix, fd->obj_id, spix, pdata_offs, avail);

    niffs_page_ix new_pix;

    if (spix == 0 || avail < pdata_len) {
      // in midst of a page
      u8_t *orig_data = _NIFFS_PIX_2_ADDR(fs, orig_pix);
      orig_data += spix == 0 ? sizeof(niffs_object_hdr) : sizeof(niffs_page_hdr);

      if (pdata_offs > 0) {
        // copy original start
        memcpy(&fs->buf[buf_offs], orig_data, pdata_offs);
      }

      // copy new data
      memcpy(&fs->buf[buf_offs + pdata_offs], src, avail);

      if (pdata_offs + avail < pdata_len) {
        // copy original end
        memcpy(&fs->buf[buf_offs + pdata_offs + avail], &orig_data[pdata_offs + avail], pdata_len - (pdata_offs + avail));
      }

      // find dst page & move src
      res = niffs_find_free_page(fs, &new_pix, NIFFS_EXCL_SECT_NONE);
      if (res != NIFFS_OK) return res;
      res = niffs_move_page(fs, orig_pix, new_pix, fs->buf, pdata_len +
          (spix == 0 ? sizeof(niffs_object_hdr) - sizeof(niffs_page_hdr) : 0), _NIFFS_FLAG_WRITTEN);
      if (res != NIFFS_OK) return res;
    } else {
      // a full page
      res = niffs_find_free_page(fs, &new_pix, NIFFS_EXCL_SECT_NONE);
      if (res != NIFFS_OK) return res;
      res = niffs_move_page(fs, orig_pix, new_pix, src, avail, _NIFFS_FLAG_WRITTEN);
      if (res != NIFFS_OK) return res;
    }


    written += avail;
    src += avail;
    fd->offs = written;
    fd->cur_pix = new_pix;
  }

  return res;
}

typedef struct {
  niffs_obj_id oid;
  niffs_span_ix ge_spix;
} niffs_trunc_arg;

static int niffs_trunc_v(niffs *fs, niffs_page_ix pix, niffs_page_hdr *phdr, void *v_arg) {
  if (!_NIFFS_IS_FREE(phdr) && !_NIFFS_IS_DELE(phdr)) {
    niffs_trunc_arg *arg = (niffs_trunc_arg *)v_arg;
    if (phdr->id.obj_id == arg->oid && phdr->id.spix >= arg->ge_spix) {
      int res = niffs_delete_page(fs, pix);
      if (res != NIFFS_OK) {
        NIFFS_DBG("trunc  warn: could not delete %04x when trunc %04x/%04x : %i\n", pix, phdr->id.obj_id, phdr->id.spix, res);
      }
    }
  }
  return NIFFS_VIS_CONT;
}

TESTATIC int niffs_truncate(niffs *fs, int fd_ix, u32_t new_len) {
  int res = NIFFS_OK;

  if (fd_ix < 0 || fd_ix >= (int)fs->descs_len) return ERR_NIFFS_FILEDESC_BAD;
  if (fs->descs[fd_ix].obj_id == 0) return ERR_NIFFS_FILEDESC_CLOSED;
  niffs_file_desc *fd = &fs->descs[fd_ix];
  niffs_object_hdr *orig_ohdr = (niffs_object_hdr *)_NIFFS_PIX_2_ADDR(fs, fd->obj_pix);
  if (orig_ohdr->phdr.id.obj_id != fd->obj_id) return ERR_NIFFS_INCOHERENT_ID;
  if (new_len == orig_ohdr->len) return NIFFS_OK;
  if (new_len > orig_ohdr->len) return ERR_NIFFS_TRUNCATE_BEYOND_FILE;


  // CHECK SPACE
  if (new_len == 0) {
    // no need to allocate a new page, just remove the lot
  } else {
    // one extra for new object header
    res = niffs_ensure_free_pages(fs, 1);
    if (res != NIFFS_OK) return res;
  }

  // repopulate if moved by gc
  orig_ohdr = (niffs_object_hdr *)_NIFFS_PIX_2_ADDR(fs, fd->obj_pix);
  if (orig_ohdr->phdr.id.obj_id != fd->obj_id) return ERR_NIFFS_INCOHERENT_ID;


  NIFFS_DBG("trunc : make oid %04x %i bytes\n", orig_ohdr->phdr.id.obj_id, new_len);

  // MARK HEADER
  if (new_len) {
    // changing existing file - write flag, mark obj header as MOVI
    niffs_flag flag = _NIFFS_FLAG_MOVING;
    res = fs->hal_wr((u8_t *)orig_ohdr + offsetof(niffs_object_hdr, phdr) + offsetof(niffs_page_hdr, flag), (u8_t *)&flag, sizeof(niffs_flag));
    if (res != NIFFS_OK) return res;

    // rewrite new object header, new length
    niffs_page_ix new_pix;
    res = niffs_find_free_page(fs, &new_pix, NIFFS_EXCL_SECT_NONE);
    if (res != NIFFS_OK) return res;

    // copy from old hdr
    memcpy(fs->buf, (u8_t *)orig_ohdr, fs->page_size);

    ((niffs_object_hdr *)fs->buf)->len = new_len;

    // move header page, rewrite length data
    res = niffs_move_page(fs, fd->obj_pix, new_pix, fs->buf + sizeof(niffs_page_hdr), fs->page_size - sizeof(niffs_page_hdr), _NIFFS_FLAG_WRITTEN);
    if (res != NIFFS_OK) return res;
  } else {
    // removing, zero length
    u32_t length = 0;
    res = fs->hal_wr((u8_t *)_NIFFS_PIX_2_ADDR(fs, fd->obj_pix) + offsetof(niffs_object_hdr, len), (u8_t *)&length, sizeof(u32_t));
    if (res != NIFFS_OK) return res;
  }

  // REMOVE PAGES
  niffs_span_ix del_start_spix = _NIFFS_OFFS_2_SPIX(fs, new_len);

  if (_NIFFS_OFFS_2_PDATA_OFFS(fs, new_len) || del_start_spix == 0) {
    del_start_spix++;
  }

  niffs_trunc_arg trunc_arg = {
      .oid = fd->obj_id,
      .ge_spix = del_start_spix
  };

  // might seem unnecessary when spix > EOF, but this is a part of cleaning away garbage as well
  res = niffs_traverse(fs, 0, 0, niffs_trunc_v, &trunc_arg);
  if (res == NIFFS_VIS_END) res = NIFFS_OK;

  if (new_len == 0) {
    // remove header
    res = niffs_delete_page(fs, fd->obj_pix);
  }

  return res;
}

typedef struct {
  u32_t sector;
  niffs_erase_cnt era_cnt;
  u32_t dele_pages;
  u32_t free_pages;
  u32_t busy_pages;
} niffs_gc_sector_cand;

static int niffs_gc_find_candidate_sector(niffs *fs, niffs_gc_sector_cand *cand) {
  u32_t sector;
  u8_t found = 0;

  // find candidate sector
  s32_t cand_score = 0x80000000;
  for (sector = 0; sector < fs->sectors; sector++) {
    niffs_sector_hdr *shdr = (niffs_sector_hdr *)_NIFFS_SECTOR_2_ADDR(fs, sector);
    if (shdr->abra != _NIFFS_SECT_MAGIC(fs)) {
      continue;
    }

    u32_t p_free = 0;
    u32_t p_dele = 0;
    u32_t p_busy = 0;

    niffs_page_ix ipix;
    for (ipix = 0; ipix < fs->pages_per_sector; ipix++) {
      niffs_page_ix pix = _NIFFS_PIX_AT_SECTOR(fs, sector) + ipix;
      niffs_page_hdr *phdr = (niffs_page_hdr *)_NIFFS_PIX_2_ADDR(fs, pix);
      if (_NIFFS_IS_FREE(phdr) && _NIFFS_IS_CLEA(phdr)) {
        p_free++;
      } else if (_NIFFS_IS_DELE(phdr)) {
        p_dele++;
      } else {
        p_busy++;
      }
    }

    // never gc a sector that is totally free
    if (p_free == fs->pages_per_sector) continue;

    u32_t era_cnt_diff = fs->max_era - shdr->era_cnt;

    // nb: this might select a sector being full with busy pages
    //     but having too low an erase count - this will free
    //     zero pages, but will move long-lived files hogging a
    //     full sector which ruins the wear leveling
    s32_t score = NIFFS_GC_SCORE(era_cnt_diff,
        (100*p_free)/fs->pages_per_sector,
        (100*p_dele)/fs->pages_per_sector,
        (100*p_busy)/fs->pages_per_sector);
    if (score > cand_score && p_busy <= fs->free_pages) {
      cand_score = score;
      cand->sector = sector;
      cand->era_cnt = shdr->era_cnt;
      cand->free_pages = p_free;
      cand->dele_pages = p_dele;
      cand->busy_pages = p_busy;
      found = 1;
    }
  }

  if (found) {
    NIFFS_DBG("gc    : found candidate sector %i era_cnt:%i (free:%i dele:%i busy:%i)\n", cand->sector, cand->era_cnt, cand->free_pages, cand->dele_pages, cand->busy_pages);
  } else {
    NIFFS_DBG("gc    : found no candidate sector\n");
  }

  return found ? NIFFS_OK : ERR_NIFFS_NO_GC_CANDIDATE;
}

TESTATIC int niffs_gc(niffs *fs, u32_t *freed_pages) {
  niffs_gc_sector_cand cand;
  int res = niffs_gc_find_candidate_sector(fs, &cand);
  if (res != NIFFS_OK) return res;

  // move all busy pages within sector
  niffs_page_ix ipix;
  for (ipix = 0; ipix < fs->pages_per_sector; ipix++) {
    niffs_page_ix pix = _NIFFS_PIX_AT_SECTOR(fs, cand.sector) + ipix;
    niffs_page_hdr *phdr = (niffs_page_hdr *)_NIFFS_PIX_2_ADDR(fs, pix);
    if (!_NIFFS_IS_FREE(phdr) && !_NIFFS_IS_DELE(phdr)) {
      niffs_page_ix new_pix;
      // find dst page & move src
      res = niffs_find_free_page(fs, &new_pix, cand.sector);
      if (res != NIFFS_OK) return res;
      res = niffs_move_page(fs, pix, new_pix, 0, 0, NIFFS_FLAG_MOVE_KEEP);
      if (res != NIFFS_OK) return res;
    }
  }

  // erase sector
  res = niffs_erase_sector(fs, cand.sector);
  if (res != NIFFS_OK) return res;

  // update stats
  fs->dele_pages -= cand.dele_pages;
  fs->dele_pages -= cand.busy_pages; // this is added by moving all busy pages in erased sector
  fs->free_pages += fs->pages_per_sector;
  *freed_pages = cand.dele_pages;

  NIFFS_DBG("gc    : freed %i pages\n", *freed_pages);

  return res;
}

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
    NIFFS_DBG("conf  : page size over/underflow\n");
    return ERR_NIFFS_BAD_CONF;
  }
  if (sizeof(niffs_page_id_raw)*8 < 2 + NIFFS_OBJ_ID_BITS + NIFFS_SPAN_IX_BITS) {
    NIFFS_DBG("conf  : niffs_page_id_raw type too small\n");
    return ERR_NIFFS_BAD_CONF;
  }
  if ((((fs->sector_size - sizeof(niffs_sector_hdr)) / (fs->page_size)) * fs->sectors) - 2 > (1<<NIFFS_OBJ_ID_BITS)) {
    NIFFS_DBG("conf  : niffs_obj_id type too small\n");
    return ERR_NIFFS_BAD_CONF;
  }
  if (sizeof(niffs_span_ix)*8 < NIFFS_SPAN_IX_BITS) {
    NIFFS_DBG("conf  : niffs_span_ix type too small\n");
    return ERR_NIFFS_BAD_CONF;
  }
  if (buf_len < page_size || buf_len < (((sector_size * sectors) / page_size)+7) / 8) {
    NIFFS_DBG("conf  : buffer length too small, need %i bytes\n", MAX(page_size, (((sector_size * sectors) / page_size)+7) / 8));
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
  fs->max_era = 0;
  u32_t s;
  u32_t bad_sectors = 0;

  for (s = 0; s < fs->sectors; s++) {
    niffs_sector_hdr *shdr = (niffs_sector_hdr *)_NIFFS_SECTOR_2_ADDR(fs, s);
    if (shdr->abra != _NIFFS_SECT_MAGIC(fs) || shdr->era_cnt == (niffs_erase_cnt)-1) {
      bad_sectors++;
      continue;
    }
    fs->max_era = MAX(shdr->era_cnt, fs->max_era);
  }

  // we allow one bad sector only would we lose power during erase of a sector
  if (bad_sectors > 1) {
    return ERR_NIFFS_NOT_A_FILESYSTEM;
  }

  for (s = 0; s < fs->sectors; s++) {
    niffs_sector_hdr *shdr = (niffs_sector_hdr *)_NIFFS_SECTOR_2_ADDR(fs, s);
    if (shdr->abra != _NIFFS_SECT_MAGIC(fs) || shdr->era_cnt == (niffs_erase_cnt)-1) {
      int res = niffs_erase_sector(fs, s);
      if (res != NIFFS_OK) return res;
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
  NIFFS_DUMP_OUT("NIFFS\n");
  NIFFS_DUMP_OUT("sector size : %i\n", fs->sector_size);
  NIFFS_DUMP_OUT("sectors     : %i\n", fs->sectors);
  NIFFS_DUMP_OUT("pages/sector: %i\n", fs->pages_per_sector);
  NIFFS_DUMP_OUT("page size   : %i\n", fs->page_size);
  NIFFS_DUMP_OUT("phys addr   : %p\n", fs->phys_addr);
  NIFFS_DUMP_OUT("free pages  : %i\n", fs->free_pages);
  NIFFS_DUMP_OUT("dele pages  : %i\n", fs->dele_pages);
  u32_t s;
  for (s = 0; s < fs->sectors; s++) {
    niffs_sector_hdr *shdr = (niffs_sector_hdr *)_NIFFS_SECTOR_2_ADDR(fs, s);
    NIFFS_DUMP_OUT("sector %i @ %p  era_cnt:%i  magic:%s\n", s, shdr, shdr->era_cnt, shdr->abra ==  _NIFFS_SECT_MAGIC(fs) ? "OK" : "BAD");
    niffs_page_ix ipix;
    for (ipix = 0; ipix < fs->pages_per_sector; ipix++) {
      niffs_page_ix pix = _NIFFS_PIX_AT_SECTOR(fs, s) + ipix;
      niffs_object_hdr *ohdr = (niffs_object_hdr *)_NIFFS_PIX_2_ADDR(fs, pix);
      NIFFS_DUMP_OUT("  %04x fl:%04x id:%04x ", pix, ohdr->phdr.flag, ohdr->phdr.id.raw);
      if (_NIFFS_IS_FREE(&ohdr->phdr)) NIFFS_DUMP_OUT("FREE ");
      if (_NIFFS_IS_DELE(&ohdr->phdr)) NIFFS_DUMP_OUT("DELE ");
      if (_NIFFS_IS_CLEA(&ohdr->phdr)) NIFFS_DUMP_OUT("CLEA ");
      if (_NIFFS_IS_WRIT(&ohdr->phdr)) NIFFS_DUMP_OUT("WRIT ");
      if (_NIFFS_IS_MOVI(&ohdr->phdr)) NIFFS_DUMP_OUT("MOVI ");
      if (!_NIFFS_IS_FREE(&ohdr->phdr)) {
        NIFFS_DUMP_OUT("\n    obj.id:%04x  sp.ix:%02x  cyc:%01x  ", ohdr->phdr.id.obj_id, ohdr->phdr.id.spix, ohdr->phdr.id.cycle_ix);
        if (ohdr->phdr.id.spix == 0 && _NIFFS_IS_ID_VALID(&ohdr->phdr) && !_NIFFS_IS_DELE(&ohdr->phdr)) {
          NIFFS_DUMP_OUT("len:%08x  name:%s  ", ohdr->len, ohdr->name);
        }
      }
      NIFFS_DUMP_OUT("\n");

    }

  }
}
#endif

