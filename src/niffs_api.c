/*
 * niffs_api.c
 *
 *  Created on: Feb 22, 2015
 *      Author: petera
 */

#include "niffs.h"
#include "niffs_internal.h"

int NIFFS_creat(niffs *fs, char *name, niffs_mode mode) {
  (void)mode;
  if (!fs->mounted) return ERR_NIFFS_NOT_MOUNTED;
  int res;
  res = niffs_create(fs, name);
  return res;
}

int NIFFS_open(niffs *fs, char *name, u8_t flags, niffs_mode mode) {
  (void)mode;
  if (!fs->mounted) return ERR_NIFFS_NOT_MOUNTED;
  int res = NIFFS_OK;
  int fd_ix = niffs_open(fs, name, flags);
  if (fd_ix < 0) {
    if (res == ERR_NIFFS_FILE_NOT_FOUND && (flags & NIFFS_CREAT)) {
      res = niffs_create(fs, name);
    }
    res = fd_ix;
  }

  if (res != NIFFS_OK) return res;

  if (flags & NIFFS_TRUNC) {
    niffs_file_desc *fd;
    res = niffs_get_filedesc(fs, fd_ix, &fd);
    if (res != NIFFS_OK) return res;
    niffs_object_hdr *ohdr = (niffs_object_hdr *)_NIFFS_PIX_2_ADDR(fs, fd->obj_pix);
    if (ohdr->len != NIFFS_UNDEF_LEN) {
      res = niffs_truncate(fs, fd_ix, 0);
      if (res != NIFFS_OK) {
        (void)niffs_close(fs, fd_ix);
        return res;
      }
      res = niffs_create(fs, name);
      if (res != NIFFS_OK) {
        return res;
      }
      fd_ix = niffs_open(fs, name, flags);
    }
  }

  return res < 0 ? res : fd_ix;
}

int NIFFS_read_ptr(niffs *fs, int fd, u8_t **ptr, u32_t *len) {
  if (!fs->mounted) return ERR_NIFFS_NOT_MOUNTED;
  return niffs_read_ptr(fs, fd, ptr, len);
}

int NIFFS_read(niffs *fs, int fd, u8_t *dst, u32_t len){
  if (!fs->mounted) return ERR_NIFFS_NOT_MOUNTED;
  return 0; // TODO
}

int NIFFS_lseek(niffs *fs, int fd, s32_t offs, int whence) {
  if (!fs->mounted) return ERR_NIFFS_NOT_MOUNTED;
  return niffs_seek(fs, fd, offs, whence);
}

int NIFFS_remove(niffs *fs, char *name) {
  if (!fs->mounted) return ERR_NIFFS_NOT_MOUNTED;
  int res;

  int fd = NIFFS_open(fs, name, 0, 0);
  if (fd < 0) return fd;
  res = niffs_truncate(fs, fd, 0);
  (void)niffs_close(fs, fd);

  return res;
}

int NIFFS_fremove(niffs *fs, int fd) {
  if (!fs->mounted) return ERR_NIFFS_NOT_MOUNTED;
  return niffs_truncate(fs, fd, 0);
}

int NIFFS_write(niffs *fs, int fd, u8_t *data, u32_t len) {
  if (!fs->mounted) return ERR_NIFFS_NOT_MOUNTED;
  return 0; // TODO
}

int NIFFS_fflush(niffs *fs, int fd) {
  if (!fs->mounted) return ERR_NIFFS_NOT_MOUNTED;
  (void)
  return NIFFS_OK;
}

int NIFFS_stat(niffs *fs, char *name, niffs_stat *s) {
  if (!fs->mounted) return ERR_NIFFS_NOT_MOUNTED;
  int res;

  int fd = NIFFS_open(fs, name, 0, 0);
  if (fd < 0) return fd;
  res = NIFFS_fstat(fs, fd, s);
  (void)niffs_close(fs, fd);

  return res;
}

int NIFFS_fstat(niffs *fs, int fd_ix, niffs_stat *s) {
  if (!fs->mounted) return ERR_NIFFS_NOT_MOUNTED;
  int res;

  niffs_file_desc *fd;
  res = niffs_get_filedesc(fs, fd_ix, &fd);
  if (res != NIFFS_OK) return res;

  niffs_object_hdr *ohdr = (niffs_object_hdr *)_NIFFS_PIX_2_ADDR(fs, fd->obj_pix);

  s->obj_id = ohdr->phdr.id.obj_id;
  s->size = ohdr->len == NIFFS_UNDEF_LEN ? 0 : ohdr->len;
  strncpy((char *)s->name, (char *)ohdr->name, NIFFS_NAME_LEN);

  return NIFFS_OK;
}

int NIFFS_close(niffs *fs, int fd) {
  if (!fs->mounted) return ERR_NIFFS_NOT_MOUNTED;
  return niffs_close(fs, fd);
}

int NIFFS_rename(niffs *fs, char *old, char *new) {
  if (!fs->mounted) return ERR_NIFFS_NOT_MOUNTED;
  return niffs_rename(fs, old, new);
}

niffs_DIR *NIFFS_opendir(niffs *fs, char *name, niffs_DIR *d){
  (void)name;
  if (!fs->mounted) return 0;
  d->fs = fs;
  d->pix = 0;
  return d;
}

int NIFFS_closedir(niffs_DIR *d){
  if (!d->fs->mounted) return ERR_NIFFS_NOT_MOUNTED;
  return NIFFS_OK;
}

static int niffs_readdir_v(niffs *fs, niffs_page_ix pix, niffs_page_hdr *phdr, void *v_arg) {
  (void)fs;
  struct niffs_dirent *e = (struct niffs_dirent *)v_arg;
  if (_NIFFS_IS_FLAG_VALID(phdr) && !_NIFFS_IS_FREE(phdr) && !_NIFFS_IS_DELE(phdr)) {
    if (_NIFFS_IS_OBJ_HDR(phdr)) {
      // object header page
      niffs_object_hdr *ohdr = (niffs_object_hdr *)phdr;
      e->obj_id = ohdr->phdr.id.obj_id;
      e->pix = pix;
      e->size = ohdr->len == NIFFS_UNDEF_LEN ? 0 : ohdr->len;
      strncpy((char *)e->name, (char *)ohdr->name, NIFFS_NAME_LEN);
      return NIFFS_OK;
    }
  }
  return NIFFS_VIS_CONT;
}

struct niffs_dirent *NIFFS_readdir(niffs_DIR *d, struct niffs_dirent *e){
  if (!d->fs->mounted) return 0;
  struct niffs_dirent *ret = 0;

  int res = niffs_traverse(d->fs, d->pix, 0, niffs_readdir_v, e);
  if (res == NIFFS_OK) {
    d->pix = e->pix + 1;
    ret = e;
  } else if (res == NIFFS_VIS_END) {
    // end of stream
  }

  return ret;
}
