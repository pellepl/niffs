/*
 * niffs_sys_tests.c
 *
 *  Created on: Feb 23, 2015
 *      Author: petera
 */

#include "testrunner.h"
#include "niffs_test_emul.h"

SUITE(niffs_sys_tests)

static void setup(test *t) {
  (void)niffs_emul_init();
  NIFFS_format(&fs);
  NIFFS_mount(&fs);
}

static void teardown(test *t) {
  niffs_emul_destroy_all_data();
}


TEST(sys_missing_file)
{
  int fd = NIFFS_open(&fs, "this_wont_exist", NIFFS_O_RDONLY, 0);
  TEST_CHECK_LT(fd, 0);
  return TEST_RES_OK;
}
TEST_END


TEST(sys_bad_fd)
{
  int res;
  niffs_stat s;
  int fd = NIFFS_open(&fs, "this_wont_exist", NIFFS_O_RDONLY, 0);
  TEST_CHECK_LT(fd, 0);
  res = NIFFS_fstat(&fs, fd, &s);
  TEST_CHECK_EQ(res, ERR_NIFFS_FILEDESC_BAD);
  res = NIFFS_fremove(&fs, fd);
  TEST_CHECK_EQ(res, ERR_NIFFS_FILEDESC_BAD);
  res = NIFFS_lseek(&fs, fd, 0, NIFFS_SEEK_CUR);
  TEST_CHECK_EQ(res, ERR_NIFFS_FILEDESC_BAD);
  res = NIFFS_read(&fs, fd, 0, 0);
  TEST_CHECK_EQ(res, ERR_NIFFS_FILEDESC_BAD);
  res = NIFFS_write(&fs, fd, 0, 0);
  TEST_CHECK_EQ(res, ERR_NIFFS_FILEDESC_BAD);
  return TEST_RES_OK;
}
TEST_END


TEST(sys_closed_fd)
{
  int res;
  niffs_stat s;
  res = niffs_emul_create_file(&fs, "file", _NIFFS_SPIX_2_PDATA_LEN(&fs, 1));
  TEST_CHECK_EQ(res, NIFFS_OK);
  int fd = NIFFS_open(&fs, "file", NIFFS_O_RDONLY, 0);
  TEST_CHECK_GE(fd, 0);
  NIFFS_close(&fs, fd);

  res = NIFFS_fstat(&fs, fd, &s);
  TEST_CHECK_EQ(res, ERR_NIFFS_FILEDESC_CLOSED);
  res = NIFFS_fremove(&fs, fd);
  TEST_CHECK_EQ(res, ERR_NIFFS_FILEDESC_CLOSED);
  res = NIFFS_lseek(&fs, fd, 0, NIFFS_SEEK_CUR);
  TEST_CHECK_EQ(res, ERR_NIFFS_FILEDESC_CLOSED);
  res = NIFFS_read(&fs, fd, 0, 0);
  TEST_CHECK_EQ(res, ERR_NIFFS_FILEDESC_CLOSED);
  res = NIFFS_write(&fs, fd, 0, 0);
  TEST_CHECK_EQ(res, ERR_NIFFS_FILEDESC_CLOSED);
  return TEST_RES_OK;
}
TEST_END

TEST(sys_deleted_same_fd)
{
  int res;
  niffs_stat s;
  int fd;
  res = niffs_emul_create_file(&fs, "remove", _NIFFS_SPIX_2_PDATA_LEN(&fs, 1));
  TEST_CHECK_EQ(res, NIFFS_OK);
  fd = NIFFS_open(&fs, "remove", NIFFS_O_RDWR, 0);
  TEST_CHECK_GE(fd, 0);
  fd = NIFFS_open(&fs, "remove", NIFFS_O_RDWR, 0);
  TEST_CHECK(fd >= 0);
  res = NIFFS_fremove(&fs, fd);
  TEST_CHECK(res >= 0);

  res = NIFFS_fstat(&fs, fd, &s);
  TEST_CHECK_EQ(res, ERR_NIFFS_FILEDESC_CLOSED);
  res = NIFFS_fremove(&fs, fd);
  TEST_CHECK_EQ(res, ERR_NIFFS_FILEDESC_CLOSED);
  res = NIFFS_lseek(&fs, fd, 0, NIFFS_SEEK_CUR);
  TEST_CHECK_EQ(res, ERR_NIFFS_FILEDESC_CLOSED);
  res = NIFFS_read(&fs, fd, 0, 0);
  TEST_CHECK_EQ(res, ERR_NIFFS_FILEDESC_CLOSED);
  res = NIFFS_write(&fs, fd, 0, 0);
  TEST_CHECK_EQ(res, ERR_NIFFS_FILEDESC_CLOSED);

  return TEST_RES_OK;
}
TEST_END


TEST(sys_deleted_other_fd)
{
  int res;
  niffs_stat s;
  int fd, fd_orig;
  res = niffs_emul_create_file(&fs, "remove", _NIFFS_SPIX_2_PDATA_LEN(&fs, 1));
  TEST_CHECK_EQ(res, NIFFS_OK);
  fd_orig = NIFFS_open(&fs, "remove", NIFFS_O_RDWR, 0);
  TEST_CHECK_GE(fd_orig, 0);
  fd = NIFFS_open(&fs, "remove", NIFFS_O_RDWR, 0);
  TEST_CHECK_GE(fd, 0);
  res = NIFFS_fremove(&fs, fd_orig);
  TEST_CHECK_EQ(res, NIFFS_OK);
  NIFFS_close(&fs, fd_orig);

  res = NIFFS_fstat(&fs, fd, &s);
  TEST_CHECK_EQ(res, ERR_NIFFS_FILEDESC_CLOSED);
  res = NIFFS_fremove(&fs, fd);
  TEST_CHECK_EQ(res, ERR_NIFFS_FILEDESC_CLOSED);
  res = NIFFS_lseek(&fs, fd, 0, NIFFS_SEEK_CUR);
  TEST_CHECK_EQ(res, ERR_NIFFS_FILEDESC_CLOSED);
  res = NIFFS_read(&fs, fd, 0, 0);
  TEST_CHECK_EQ(res, ERR_NIFFS_FILEDESC_CLOSED);
  res = NIFFS_write(&fs, fd, 0, 0);
  TEST_CHECK_EQ(res, ERR_NIFFS_FILEDESC_CLOSED);

  return TEST_RES_OK;
}
TEST_END


TEST(sys_fd_overflow)
{
  int res;
  niffs_stat s;
  res = niffs_emul_create_file(&fs, "file", _NIFFS_SPIX_2_PDATA_LEN(&fs, 1));
  TEST_CHECK_EQ(res, NIFFS_OK);
  int i = 0;
  while (i < fs.descs_len) {
    int fd = NIFFS_open(&fs, "file", NIFFS_O_RDONLY, 0);
    TEST_CHECK_GE(fd, 0);
    i++;
  }
  int fd = NIFFS_open(&fs, "file", NIFFS_O_RDONLY, 0);
  TEST_CHECK_EQ(fd, ERR_NIFFS_OUT_OF_FILEDESCS);

  return TEST_RES_OK;
}
TEST_END



TEST(sys_file_by_open)
{
  int res;
  niffs_stat s;
  int fd = NIFFS_open(&fs, "filebopen", NIFFS_O_CREAT, 0);
  TEST_CHECK_GE(fd, 0);
  res = NIFFS_fstat(&fs, fd, &s);
  TEST_CHECK_EQ(res, NIFFS_OK);
  TEST_CHECK_EQ(strcmp((char*)s.name, "filebopen"), 0);
  TEST_CHECK_EQ(s.size, 0);
  NIFFS_close(&fs, fd);

  fd = NIFFS_open(&fs, "filebopen", NIFFS_O_RDONLY, 0);
  TEST_CHECK_GE(fd, 0);
  res = NIFFS_fstat(&fs, fd, &s);
  TEST_CHECK_EQ(res, NIFFS_OK);
  TEST_CHECK_EQ(strcmp((char*)s.name, "filebopen"), 0);
  TEST_CHECK_EQ(s.size, 0);
  NIFFS_close(&fs, fd);

  return TEST_RES_OK;
}
TEST_END

TEST(sys_file_by_open_flags)
{
  u8_t buf[8];
  int res = niffs_emul_create_file(&fs, "reffile", _NIFFS_SPIX_2_PDATA_LEN(&fs, 1));
  TEST_CHECK_EQ(res, NIFFS_OK);

  // creat excl on created file
  int fd = NIFFS_open(&fs, "reffile", NIFFS_O_RDWR | NIFFS_O_CREAT | NIFFS_O_EXCL | NIFFS_O_TRUNC, 0);
  TEST_CHECK_EQ(fd, ERR_NIFFS_FILE_EXISTS);
  niffs_stat s;
  res = NIFFS_stat(&fs, "reffile", &s);
  TEST_CHECK_EQ(res, NIFFS_OK);
  TEST_CHECK_EQ(s.size, (u32_t)_NIFFS_SPIX_2_PDATA_LEN(&fs, 1));

  // read only
  fd = NIFFS_open(&fs, "reffile", NIFFS_O_RDONLY, 0);
  TEST_CHECK_GE(fd, 0);
  res = NIFFS_read(&fs, fd, buf, sizeof(buf));
  TEST_CHECK_EQ(res, (s32_t)sizeof(buf));
  res = NIFFS_write(&fs, fd, buf, sizeof(buf));
  TEST_CHECK_EQ(res, ERR_NIFFS_NOT_WRITABLE);
  res = NIFFS_fremove(&fs, fd);
  TEST_CHECK_EQ(res, ERR_NIFFS_NOT_WRITABLE);
  (void)NIFFS_close(&fs, fd);

  // creat on created file
  fd = NIFFS_open(&fs, "reffile", NIFFS_O_RDWR | NIFFS_O_CREAT, 0);
  TEST_CHECK_GE(fd, 0);
  res = NIFFS_fstat(&fs, fd, &s);
  TEST_CHECK_EQ(res, NIFFS_OK);
  TEST_CHECK_EQ(s.size, (u32_t)_NIFFS_SPIX_2_PDATA_LEN(&fs, 1));
  (void)NIFFS_close(&fs, fd);
  res = niffs_emul_verify_file(&fs, "reffile");
  TEST_CHECK_EQ(res, NIFFS_OK);

  // write only
  memset(buf, 0xee, sizeof(buf));
  fd = NIFFS_open(&fs, "reffile", NIFFS_O_WRONLY, 0);
  TEST_CHECK_GE(fd, 0);
  res = NIFFS_read(&fs, fd, buf, sizeof(buf));
  TEST_CHECK_EQ(res, ERR_NIFFS_NOT_READABLE);
  res = NIFFS_write(&fs, fd, buf, sizeof(buf));
  TEST_CHECK_EQ(res, (s32_t)sizeof(buf));
  (void)NIFFS_close(&fs, fd);
  res = niffs_emul_verify_file(&fs, "reffile");
  TEST_CHECK_EQ(res, ERR_NIFFS_TEST_REF_DATA_MISMATCH);

  // creat trunc on created file
  fd = NIFFS_open(&fs, "reffile", NIFFS_O_RDWR | NIFFS_O_CREAT | NIFFS_O_TRUNC, 0);
  TEST_CHECK_EQ(fd, NIFFS_OK);
  res = NIFFS_fstat(&fs, fd, &s);
  TEST_CHECK_EQ(res, NIFFS_OK);
  TEST_CHECK_EQ(s.size, 0);
  (void)NIFFS_close(&fs, fd);

  // append
  fd = NIFFS_open(&fs, "reffile", NIFFS_O_RDWR | NIFFS_O_CREAT | NIFFS_O_TRUNC | NIFFS_O_APPEND, 0);
  TEST_CHECK_EQ(fd, NIFFS_OK);
  res = NIFFS_fstat(&fs, fd, &s);
  TEST_CHECK_EQ(res, NIFFS_OK);
  TEST_CHECK_EQ(s.size, 0);
  u32_t dlen;
  u8_t *data = niffs_emul_get_data("reffile", &dlen);
  u32_t offs = 0;
  while (offs < dlen) {
    u32_t wlen = MIN(dlen - offs, 7);
    res = NIFFS_write(&fs, fd, &data[offs], wlen);
    TEST_CHECK_EQ(res, wlen);
    res = NIFFS_lseek(&fs, fd, 0, NIFFS_SEEK_SET);
    TEST_CHECK_EQ(res, NIFFS_OK);
    offs += wlen;
  }
  (void)NIFFS_close(&fs, fd);
  res = niffs_emul_verify_file(&fs, "reffile");
  TEST_CHECK_EQ(res, NIFFS_OK);

  return TEST_RES_OK;
}
TEST_END

TEST(sys_file_by_creat)
{
  int res;
  res = niffs_emul_create_file(&fs, "filebcreat", _NIFFS_SPIX_2_PDATA_LEN(&fs, 1));
  TEST_CHECK_EQ(res, NIFFS_OK);
  res = NIFFS_creat(&fs, "filebcreat", 0);
  TEST_CHECK_EQ(res, ERR_NIFFS_NAME_CONFLICT);
  return TEST_RES_OK;
}
TEST_END

TEST(sys_list_dir)
{
  int res;

  char *files[4] = {
      "file1",
      "file2",
      "file3",
      "file4"
  };
  int file_cnt = sizeof(files)/sizeof(char *);

  int i;

  for (i = 0; i < file_cnt; i++) {
    res = niffs_emul_create_file(&fs, files[i], _NIFFS_SPIX_2_PDATA_LEN(&fs, 1));
    TEST_CHECK_EQ(res, NIFFS_OK);
  }

  niffs_DIR d;
  struct niffs_dirent e;
  struct niffs_dirent *pe = &e;

  NIFFS_opendir(&fs, "/", &d);
  int found = 0;
  while ((pe = NIFFS_readdir(&d, pe))) {
    printf("  %s [%04x] size:%i type:%02x\n", pe->name, pe->obj_id, pe->size, pe->type);
    for (i = 0; i < file_cnt; i++) {
      if (strcmp(files[i], pe->name) == 0) {
        found++;
        break;
      }
    }
  }
  NIFFS_closedir(&d);

  TEST_CHECK_EQ(found, file_cnt);

  return TEST_RES_OK;
}
TEST_END

TEST(sys_write) {
  int res;
  int fd;
  fd = NIFFS_open(&fs, "testfile", NIFFS_O_RDWR | NIFFS_O_CREAT | NIFFS_O_TRUNC | NIFFS_O_APPEND, 0);
  TEST_CHECK_GE(fd, 0);

  u8_t test_hdr[4] = {1,2,3,4};
  res = NIFFS_write(&fs, fd, (u8_t *)&test_hdr, sizeof(test_hdr));
  TEST_CHECK_EQ(res, (int)sizeof(test_hdr));

  u8_t test_data[8+3] = {0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x20,0x21,0x22};
  u8_t pin;
  for (pin = 0; pin < 24; pin++) {
    u8_t *cfg = (u8_t *)test_data;
    res = NIFFS_write(&fs, fd, cfg, sizeof(test_data));
    TEST_CHECK_EQ(res, (int)sizeof(test_data));
  }
  TEST_CHECK_EQ(NIFFS_close(&fs, fd), NIFFS_OK);;

  return TEST_RES_OK;
}
TEST_END



TEST(sys_simultaneous_write) {
  int res;
  res = NIFFS_creat(&fs, "simul1", 0);
  TEST_CHECK_EQ(res, NIFFS_OK);

  int fd1 = NIFFS_open(&fs, "simul1", NIFFS_O_RDWR, 0);
  TEST_CHECK_GE(fd1, 0);
  int fd2 = NIFFS_open(&fs, "simul1", NIFFS_O_RDWR, 0);
  TEST_CHECK_GE(fd2, 0);
  int fd3 = NIFFS_open(&fs, "simul1", NIFFS_O_RDWR, 0);
  TEST_CHECK_GE(fd3, 0);

  u8_t data1 = 1;
  u8_t data2 = 2;
  u8_t data3 = 3;

  res = NIFFS_write(&fs, fd1, &data1, 1);
  TEST_CHECK_GE(res, 0);
  NIFFS_close(&fs, fd1);
  res = NIFFS_write(&fs, fd2, &data2, 1);
  TEST_CHECK_GE(res, 0);
  NIFFS_close(&fs, fd2);
  res = NIFFS_write(&fs, fd3, &data3, 1);
  TEST_CHECK_GE(res, 0);
  NIFFS_close(&fs, fd3);

  niffs_stat s;
  res = NIFFS_stat(&fs, "simul1", &s);
  TEST_CHECK_EQ(res, NIFFS_OK);

  TEST_CHECK_EQ(s.size, 1);

  u8_t rdata;
  int fd = NIFFS_open(&fs, "simul1", NIFFS_O_RDONLY, 0);
  TEST_CHECK_GE(fd, 0);
  res = NIFFS_read(&fs, fd, &rdata, 1);
  TEST_CHECK_GE(res, 0);

  TEST_CHECK_EQ(rdata, data3);

  return TEST_RES_OK;
}
TEST_END

TEST(sys_simultaneous_write_append) {
  int res = NIFFS_creat(&fs, "simul2", 0);
  TEST_CHECK_EQ(res, NIFFS_OK);

  int fd1 = NIFFS_open(&fs, "simul2", NIFFS_O_RDWR | NIFFS_O_APPEND, 0);
  TEST_CHECK_GE(fd1, 0);
  int fd2 = NIFFS_open(&fs, "simul2", NIFFS_O_RDWR | NIFFS_O_APPEND, 0);
  TEST_CHECK_GE(fd2, 0);
  int fd3 = NIFFS_open(&fs, "simul2", NIFFS_O_RDWR | NIFFS_O_APPEND, 0);
  TEST_CHECK_GE(fd3, 0);

  u8_t data1 = 1;
  u8_t data2 = 2;
  u8_t data3 = 3;

  res = NIFFS_write(&fs, fd1, &data1, 1);
  TEST_CHECK_EQ(res, 1);
  NIFFS_close(&fs, fd1);
  res = NIFFS_write(&fs, fd2, &data2, 1);
  TEST_CHECK_EQ(res, 1);
  NIFFS_close(&fs, fd2);
  res = NIFFS_write(&fs, fd3, &data3, 1);
  TEST_CHECK_EQ(res, 1);
  NIFFS_close(&fs, fd3);

  niffs_stat s;
  res = NIFFS_stat(&fs, "simul2", &s);
  TEST_CHECK_EQ(res, NIFFS_OK);

  TEST_CHECK_EQ(s.size, 3);

  u8_t rdata[3];
  int fd = NIFFS_open(&fs, "simul2", NIFFS_O_RDONLY, 0);
  TEST_CHECK_GE(fd, 0);
  res = NIFFS_read(&fs, fd, (u8_t*)&rdata, 3);
  TEST_CHECK_EQ(res, 3);

  TEST_CHECK_EQ(rdata[0], data1);
  TEST_CHECK_EQ(rdata[1], data2);
  TEST_CHECK_EQ(rdata[2], data3);

  return TEST_RES_OK;
}
TEST_END

TEST(sys_rename) {
  int res;

  char *src_name = "baah";
  char *dst_name = "booh";
  char *dst_name2 = "beeh";
  int size = _NIFFS_SPIX_2_PDATA_LEN(&fs, 1);

  res = niffs_emul_create_file(&fs, src_name, size);
  TEST_CHECK_EQ(res, NIFFS_OK);

  res = NIFFS_rename(&fs, src_name, dst_name);
  TEST_CHECK_EQ(res, NIFFS_OK);

  res = NIFFS_rename(&fs, dst_name, dst_name);
  TEST_CHECK_EQ(res, ERR_NIFFS_NAME_CONFLICT);

  res = NIFFS_rename(&fs, src_name, dst_name2);
  TEST_CHECK_EQ(res, ERR_NIFFS_FILE_NOT_FOUND);

  return TEST_RES_OK;
} TEST_END

TEST(sys_remove_single_by_path)
{
  int res;
  int fd;
  res = niffs_emul_create_file(&fs, "remove", 8);
  TEST_CHECK_EQ(res, NIFFS_OK);
  res = NIFFS_remove(&fs, "remove");
  TEST_CHECK_EQ(res, NIFFS_OK);
  fd = NIFFS_open(&fs, "remove", NIFFS_O_RDONLY, 0);
  TEST_CHECK_EQ(fd, ERR_NIFFS_FILE_NOT_FOUND);

  return TEST_RES_OK;
}
TEST_END


TEST(sys_remove_single_by_fd)
{
  int res;
  int fd;
  res = niffs_emul_create_file(&fs, "remove", 8);
  TEST_CHECK_EQ(res, NIFFS_OK);
  fd = NIFFS_open(&fs, "remove", NIFFS_O_RDWR, 0);
  TEST_CHECK_GE(fd, 0);
  res = NIFFS_fremove(&fs, fd);
  TEST_CHECK_EQ(res, NIFFS_OK);
  (void)NIFFS_close(&fs, fd);
  fd = NIFFS_open(&fs, "remove", NIFFS_O_RDONLY, 0);
  TEST_CHECK_EQ(fd, ERR_NIFFS_FILE_NOT_FOUND);

  return TEST_RES_OK;
}
TEST_END

TEST(sys_read_beyond)
{
  int res;
  int fd;
  u8_t buf[8];
  u32_t len = sizeof(buf);
  res = niffs_emul_create_file(&fs, "read", len);
  TEST_CHECK_EQ(res, NIFFS_OK);
  fd = NIFFS_open(&fs, "read", NIFFS_O_RDONLY, 0);
  TEST_CHECK_GE(fd, 0);
  res = NIFFS_read(&fs, fd, buf, len);
  TEST_CHECK_EQ(res, len);
  res = NIFFS_read(&fs, fd, buf, len);
  TEST_CHECK_EQ(res, 0);  /*
    If the starting position is at or after the end-of-file, 0 shall be returned.
    [http://pubs.opengroup.org/onlinepubs/009695399/functions/read.html] */

  return TEST_RES_OK;
}
TEST_END

TEST(sys_read_empty)
{
  int res;
  int fd;
  u8_t buf[8];
  u32_t len = sizeof(buf);
  res = NIFFS_creat(&fs, "empty", 0);
  TEST_CHECK_EQ(res, NIFFS_OK);
  niffs_stat s;
  res = NIFFS_stat(&fs, "empty", &s);
  TEST_CHECK_EQ(s.size, 0);
  fd = NIFFS_open(&fs, "empty", NIFFS_O_RDONLY, 0);
  TEST_CHECK_GE(fd, 0);
  res = NIFFS_read(&fs, fd, buf, len);
  TEST_CHECK_EQ(res, 0);  /*
    If the starting position is at or after the end-of-file, 0 shall be returned.
    [http://pubs.opengroup.org/onlinepubs/009695399/functions/read.html] */

  return TEST_RES_OK;
}
TEST_END

TEST(sys_read_ptr_beyond)
{
  int res;
  int fd;
  u32_t len = 8;
  res = niffs_emul_create_file(&fs, "read", len);
  TEST_CHECK_EQ(res, NIFFS_OK);
  fd = NIFFS_open(&fs, "read", NIFFS_O_RDONLY, 0);
  TEST_CHECK_GE(fd, 0);
  u8_t *rptr;
  u32_t rlen;
  res = niffs_emul_read_ptr(&fs, fd, &rptr, &rlen);
  TEST_CHECK_EQ(res, len);
  TEST_CHECK_EQ(rlen, len);
  res = NIFFS_lseek(&fs, fd, 0, NIFFS_SEEK_END);
  TEST_CHECK_EQ(res, NIFFS_OK);
  res = niffs_emul_read_ptr(&fs, fd, &rptr, &rlen);
  TEST_CHECK_EQ(res, ERR_NIFFS_END_OF_FILE);
  TEST_CHECK_EQ(rlen, 0);

  return TEST_RES_OK;
}
TEST_END

TEST(sys_read_ptr_empty)
{
  int res;
  int fd;
  u32_t len = 8;
  res = NIFFS_creat(&fs, "empty", 0);
  TEST_CHECK_EQ(res, NIFFS_OK);
  niffs_stat s;
  res = NIFFS_stat(&fs, "empty", &s);
  TEST_CHECK_EQ(s.size, 0);
  fd = NIFFS_open(&fs, "empty", NIFFS_O_RDONLY, 0);
  TEST_CHECK_GE(fd, 0);
  u8_t *rptr;
  u32_t rlen;
  res = niffs_emul_read_ptr(&fs, fd, &rptr, &rlen);
  TEST_CHECK_EQ(res, ERR_NIFFS_END_OF_FILE);
  TEST_CHECK_EQ(rlen, 0);

  return TEST_RES_OK;
}
TEST_END

TEST(sys_write_flush)
{
  int res;
  int fd;
  u32_t len = 8;
  fd = NIFFS_open(&fs, "write", NIFFS_O_CREAT | NIFFS_O_RDWR, 0);
  TEST_CHECK_GE(fd, 0);
  u8_t *data = niffs_emul_create_data("write", len);
  TEST_CHECK(data);

  u32_t i;
  for (i = 0; i < len; i++) {
    res = NIFFS_write(&fs, fd, &data[i], 1);
    TEST_CHECK_EQ(res, 1);
    niffs_stat s;
    res = NIFFS_fstat(&fs, fd, &s);
    TEST_CHECK_EQ(res, NIFFS_OK);
    TEST_CHECK_EQ(s.size, i+1);
  }

  res = NIFFS_fflush(&fs, fd);
  TEST_CHECK_EQ(res, NIFFS_OK);

  (void)NIFFS_close(&fs, fd);

  res = niffs_emul_verify_file(&fs, "write");
  TEST_CHECK_EQ(res, NIFFS_OK);

  return TEST_RES_OK;
}
TEST_END

TEST(sys_file_uniqueness)
{
  int res;
  int fd;
  char fname[NIFFS_NAME_LEN];
  int files = (fs.sectors - 1) * (fs.pages_per_sector);
  int i;
  printf("  creating %i files\n", files);
  for (i = 0; i < files; i++) {
    char content[20];
    sprintf(fname, "file%i", i);
    sprintf(content, "%i", i);
    fd = NIFFS_open(&fs, fname, NIFFS_O_CREAT | NIFFS_O_APPEND | NIFFS_O_RDWR, 0);
    TEST_CHECK_GE(fd, 0);
    res = NIFFS_write(&fs, fd, content, strlen(content)+1);
    TEST_CHECK_EQ(res, (u32_t)strlen(content)+1);
    (void)NIFFS_close(&fs, fd);
  }
  printf("  checking %i files\n", files);
  for (i = 0; i < files; i++) {
    char content[20];
    char ref_content[20];
    sprintf(fname, "file%i", i);
    sprintf(content, "%i", i);
    fd = NIFFS_open(&fs, fname, NIFFS_O_RDONLY, 0);
    TEST_CHECK_GE(fd, 0);
    res = NIFFS_read(&fs, fd, ref_content, strlen(content)+1);
    TEST_CHECK_EQ(res, (u32_t)strlen(content)+1);
    TEST_CHECK_EQ(strcmp(ref_content, content), 0);
    (void)NIFFS_close(&fs, fd);
  }
  printf("  removing %i files\n", files/2);
  for (i = 0; i < files; i += 2) {
    sprintf(fname, "file%i", i);
    res = NIFFS_remove(&fs, fname);
    TEST_CHECK_EQ(res, 0);
  }
  printf("  creating %i files\n", files/2);
  for (i = 0; i < files; i += 2) {
    char content[20];
    sprintf(fname, "file%i", i);
    sprintf(content, "new%i", i);
    fd = NIFFS_open(&fs, fname, NIFFS_O_CREAT | NIFFS_O_APPEND | NIFFS_O_RDWR, 0);
    TEST_CHECK_GE(fd, 0);
    res = NIFFS_write(&fs, fd, content, strlen(content)+1);
    TEST_CHECK_EQ(res, (u32_t)strlen(content)+1);
    (void)NIFFS_close(&fs, fd);
  }
  printf("  checking %i files\n", files);
  for (i = 0; i < files; i++) {
    char content[20];
    char ref_content[20];
    sprintf(fname, "file%i", i);
    if ((i & 1) == 0) {
      sprintf(content, "new%i", i);
    } else {
      sprintf(content, "%i", i);
    }
    fd = NIFFS_open(&fs, fname, NIFFS_O_RDONLY, 0);
    TEST_CHECK_GE(fd, 0);
    res = NIFFS_read(&fs, fd, ref_content, strlen(content)+1);
    TEST_CHECK_EQ(res, (u32_t)strlen(content)+1);
    TEST_CHECK_EQ(strcmp(ref_content, content), 0);
    (void)NIFFS_close(&fs, fd);
  }

  return TEST_RES_OK;
}
TEST_END

TEST(sys_lseek_simple_modification) {
  int res;
  int fd;
  char *fname = "seekfile";
  int i;
  int len = 4096;
  fd = NIFFS_open(&fs, fname, NIFFS_O_TRUNC | NIFFS_O_CREAT | NIFFS_O_RDWR, 0);
  TEST_CHECK_GE(fd, 0);

  u8_t *buf = niffs_emul_create_data("seekfile", len);
  res = NIFFS_write(&fs, fd, buf, len);
  TEST_CHECK_EQ(res, len);

  res = niffs_emul_verify_file(&fs, "seekfile");
  TEST_CHECK_EQ(res, NIFFS_OK);

  res = NIFFS_lseek(&fs, fd, len/2, NIFFS_SEEK_SET);
  TEST_CHECK_EQ(res, NIFFS_OK);

  u8_t *nbuf = niffs_emul_create_data("newdata", len/2);
  res = NIFFS_write(&fs, fd, nbuf, len/2);
  TEST_CHECK_EQ(res, len/2);

  niffs_stat s;
  res = NIFFS_fstat(&fs, fd, &s);
  TEST_CHECK_EQ(res, NIFFS_OK);
  TEST_CHECK_EQ(s.size, len);

  u8_t *ref = malloc(len);
  memcpy(ref, buf, len/2);
  memcpy(&ref[len/2], nbuf, len/2);

  res = niffs_emul_verify_file_against_data(&fs, "seekfile", ref);
  free(ref);
  TEST_CHECK_EQ(res, NIFFS_OK);

  (void)NIFFS_close(&fs, fd);

  return TEST_RES_OK;
}
TEST_END

TEST(sys_lseek_modification_append) {
  int res;
  int fd;
  char *fname = "seekfile";
  int i;
  int len = 4096;
  fd = NIFFS_open(&fs, fname, NIFFS_O_TRUNC | NIFFS_O_CREAT | NIFFS_O_RDWR, 0);
  TEST_CHECK_GE(fd, 0);

  u8_t *buf = niffs_emul_create_data("seekfile", len);
  res = NIFFS_write(&fs, fd, buf, len);
  TEST_CHECK_EQ(res, len);

  res = niffs_emul_verify_file(&fs, "seekfile");
  TEST_CHECK_EQ(res, NIFFS_OK);

  res = NIFFS_lseek(&fs, fd, len/2, NIFFS_SEEK_SET);
  TEST_CHECK_EQ(res, NIFFS_OK);

  u8_t *nbuf = niffs_emul_create_data("newdata", len);
  res = NIFFS_write(&fs, fd, nbuf, len);
  TEST_CHECK_EQ(res, len);

  niffs_stat s;
  res = NIFFS_fstat(&fs, fd, &s);
  TEST_CHECK_EQ(res, NIFFS_OK);
  TEST_CHECK_EQ(s.size, len + len/2);

  u8_t *ref = malloc(len/2 + len);
  memcpy(ref, buf, len/2);
  memcpy(&ref[len/2], nbuf, len);

  res = niffs_emul_verify_file_against_data(&fs, "seekfile", ref);
  free(ref);
  TEST_CHECK_EQ(res, NIFFS_OK);

  (void)NIFFS_close(&fs, fd);

  return TEST_RES_OK;
}
TEST_END

TEST(sys_lseek_modification_append_multi) {
  int res;
  int fd;
  char *fname = "seekfile";
  int len = 1024;
  int runs = (fs.sectors-1) * fs.pages_per_sector * _NIFFS_SPIX_2_PDATA_LEN(&fs, 1) / len;

  u8_t *refbuf = niffs_emul_create_data("null", len + runs * (len / 2));

  fd = NIFFS_open(&fs, fname, NIFFS_O_TRUNC | NIFFS_O_CREAT | NIFFS_O_RDWR, 0);
  TEST_CHECK_GE(fd, 0);

  u8_t *buf = niffs_emul_create_data("seekfile", len);
  res = NIFFS_write(&fs, fd, buf, len);
  TEST_CHECK_EQ(res, len);
  memcpy(refbuf, buf, len);
  u32_t offs = len;

  while (runs--) {
    res = NIFFS_lseek(&fs, fd, -len/2, NIFFS_SEEK_END);
    TEST_CHECK_EQ(res, NIFFS_OK);
    offs -= len/2;

    char seedname[32];
    sprintf(seedname, "mod%i", runs);
    u8_t *bufmod = niffs_emul_create_data(seedname, len);
    res = NIFFS_write(&fs, fd, bufmod, len);
    TEST_CHECK_EQ(res, len);
    memcpy(&refbuf[offs], bufmod, len);
    offs += len;

    res = niffs_emul_verify_file_against_data(&fs, "seekfile", refbuf);
    TEST_CHECK_GE(res, 0);
  }

  (void)NIFFS_close(&fs, fd);

  return TEST_RES_OK;
}
TEST_END

TEST(sys_lseek) {
  int res;
  int fd;
  char *fname = "seekfile";
  int len = ((fs.sectors-1) * fs.pages_per_sector / 2) * _NIFFS_SPIX_2_PDATA_LEN(&fs, 1);

  fd = NIFFS_open(&fs, fname, NIFFS_O_TRUNC | NIFFS_O_CREAT | NIFFS_O_RDWR, 0);
  TEST_CHECK_GE(fd, 0);

  u8_t *refbuf = niffs_emul_create_data("seekfile", len);
  res = NIFFS_write(&fs, fd, refbuf, len);
  TEST_CHECK_EQ(res, len);

  niffs_stat s;
  TEST_CHECK_EQ(NIFFS_fstat(&fs, fd, &s), NIFFS_OK);

  int offs;
  int i;

  for (i = 1; i < s.size; i++) {
    res = NIFFS_lseek(&fs, fd, -i, NIFFS_SEEK_END);
    TEST_CHECK_EQ(res, NIFFS_OK);
    offs = s.size - i;
    TEST_CHECK_EQ(NIFFS_ftell(&fs, fd), offs);
  }

  for (i = 0; i < s.size; i++) {
    res = NIFFS_lseek(&fs, fd, i, NIFFS_SEEK_SET);
    TEST_CHECK_EQ(res, NIFFS_OK);
    offs = i;
    TEST_CHECK_EQ(NIFFS_ftell(&fs, fd), offs);
  }

  res = NIFFS_lseek(&fs, fd, 0, NIFFS_SEEK_SET);
  TEST_CHECK_EQ(res, NIFFS_OK);
  offs = 0;
  TEST_CHECK_EQ(NIFFS_ftell(&fs, fd), offs);
  for (i = 0; i < s.size; i++) {
    res = NIFFS_lseek(&fs, fd, 1, NIFFS_SEEK_CUR);
    TEST_CHECK_EQ(res, NIFFS_OK);
    offs++;
    TEST_CHECK_EQ(NIFFS_ftell(&fs, fd), offs);
  }
  for (i = 0; i < s.size; i++) {
    res = NIFFS_lseek(&fs, fd, -1, NIFFS_SEEK_CUR);
    TEST_CHECK_EQ(res, NIFFS_OK);
    offs--;
    TEST_CHECK_EQ(NIFFS_ftell(&fs, fd), offs);
  }

  (void)NIFFS_close(&fs, fd);

  return TEST_RES_OK;
}
TEST_END

TEST(sys_lseek_read_ftell) {
  int res;
  int fd;
  char *fname = "seekfile";
  int len = ((fs.sectors-1) * fs.pages_per_sector / 2) * _NIFFS_SPIX_2_PDATA_LEN(&fs, 1);
  int runs = 10000;

  fd = NIFFS_open(&fs, fname, NIFFS_O_TRUNC | NIFFS_O_CREAT | NIFFS_O_RDWR, 0);
  TEST_CHECK_GE(fd, 0);

  u8_t *refbuf = niffs_emul_create_data("seekfile", len);
  res = NIFFS_write(&fs, fd, refbuf, len);
  TEST_CHECK_EQ(res, len);

  int offs = 0;
  res = NIFFS_lseek(&fs, fd, 0, NIFFS_SEEK_SET);
  TEST_CHECK_EQ(res, NIFFS_OK);
  TEST_CHECK_EQ(NIFFS_ftell(&fs, fd), offs);

  while (runs--) {
    int i;
    u8_t buf[64];
    if (offs + 41 + sizeof(buf) >= len) {
      offs = (offs + 41 + sizeof(buf)) % len;
      res = NIFFS_lseek(&fs, fd, offs, NIFFS_SEEK_SET);
      TEST_CHECK_EQ(res, NIFFS_OK);
      TEST_CHECK_EQ(NIFFS_ftell(&fs, fd), offs);
    }
    res = NIFFS_lseek(&fs, fd, 41, NIFFS_SEEK_CUR);
    TEST_CHECK_EQ(res, NIFFS_OK);
    offs += 41;
    TEST_CHECK_EQ(NIFFS_ftell(&fs, fd), offs);
    res = NIFFS_read(&fs, fd, buf, sizeof(buf));
    TEST_CHECK_EQ(res, (u32_t)sizeof(buf));
    for (i = 0; i < sizeof(buf); i++) {
      if (buf[i] != refbuf[offs+i]) {
        printf("  mismatch at offs %i\n", offs);
      }
      TEST_CHECK_EQ(buf[i], refbuf[offs+i]);
    }
    offs += sizeof(buf);

    res = NIFFS_lseek(&fs, fd, -((u32_t)sizeof(buf)+11), NIFFS_SEEK_CUR);
    TEST_CHECK_EQ(res, NIFFS_OK);
    offs -= (sizeof(buf)+11);
    TEST_CHECK_EQ(NIFFS_ftell(&fs, fd), offs);
    res = NIFFS_read(&fs, fd, buf, sizeof(buf));
    TEST_CHECK_EQ(res, (u32_t)sizeof(buf));
    for (i = 0; i < sizeof(buf); i++) {
      if (buf[i] != refbuf[offs+i]) {
        printf("  mismatch at offs %i\n", offs);
      }
      TEST_CHECK_EQ(buf[i], refbuf[offs+i]);
    }
    offs += sizeof(buf);
  }

  (void)NIFFS_close(&fs, fd);

  return TEST_RES_OK;
}
TEST_END

#if NIFFS_LINEAR_AREA

TEST(sys_lin_open) {
  int res;
  int fd;

  u8_t *data = niffs_emul_create_data("linear", fs.lin_sectors * fs.sector_size);
  TEST_CHECK(data);

  u32_t mul;
  for (mul = 1; mul <= fs.lin_sectors; mul++) {
    const u32_t fsize = fs.sector_size * mul;
    fd = NIFFS_open(&fs, "linear", NIFFS_O_CREAT | NIFFS_O_TRUNC | NIFFS_O_LINEAR | NIFFS_O_RDWR, 0);
    TEST_CHECK_GE(fd,0);
    res = NIFFS_write(&fs, fd, data, fsize);
    TEST_CHECK_EQ(res, fsize);
    res = NIFFS_close(&fs, fd);
    TEST_CHECK_EQ(res, NIFFS_OK);
    fd = NIFFS_open(&fs, "linear", NIFFS_O_RDONLY, 0);
    TEST_CHECK_GE(fd,0);
    res = niffs_emul_verify_file(&fs, "linear");
    TEST_CHECK_EQ(res, NIFFS_OK);
    u8_t *ptr;
    u32_t len;
    res = NIFFS_read_ptr(&fs, fd, &ptr, &len);
    TEST_CHECK_EQ(res, fsize);
    TEST_CHECK_EQ(len, fsize);
    niffs_stat s;
    res = NIFFS_fstat(&fs, fd, &s);
    TEST_CHECK_EQ(res, NIFFS_OK);
    TEST_CHECK_EQ(s.size, fsize);
    TEST_CHECK_EQ(s.type, _NIFFS_FTYPE_LINFILE);
    res = NIFFS_close(&fs, fd);
    TEST_CHECK_EQ(res, NIFFS_OK);
  }
  return TEST_RES_OK;
}
TEST_END

#endif // NIFFS_LINEAR_AREA

SUITE_TESTS(niffs_sys_tests)
  ADD_TEST(sys_missing_file)
  ADD_TEST(sys_bad_fd)
  ADD_TEST(sys_closed_fd)
  ADD_TEST(sys_deleted_same_fd)
  ADD_TEST(sys_deleted_other_fd)
  ADD_TEST(sys_fd_overflow)
  ADD_TEST(sys_file_by_open)
  ADD_TEST(sys_file_by_open_flags)
  ADD_TEST(sys_file_by_creat)
  ADD_TEST(sys_list_dir)
  ADD_TEST(sys_write)
  ADD_TEST(sys_simultaneous_write)
  ADD_TEST(sys_simultaneous_write_append)
  ADD_TEST(sys_rename)
  ADD_TEST(sys_remove_single_by_path)
  ADD_TEST(sys_remove_single_by_fd)
  ADD_TEST(sys_read_beyond)
  ADD_TEST(sys_read_empty)
  ADD_TEST(sys_read_ptr_beyond)
  ADD_TEST(sys_read_ptr_empty)
  ADD_TEST(sys_write_flush)
  ADD_TEST(sys_file_uniqueness)
  ADD_TEST(sys_lseek_simple_modification)
  ADD_TEST(sys_lseek_modification_append)
  ADD_TEST(sys_lseek_modification_append_multi)
  ADD_TEST(sys_lseek)
  ADD_TEST(sys_lseek_read_ftell)
#if NIFFS_LINEAR_AREA
  ADD_TEST(sys_lin_open)
#endif // NIFFS_LINEAR_AREA
SUITE_END(niffs_sys_tests)
