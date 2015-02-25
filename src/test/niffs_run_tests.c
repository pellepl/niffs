/*
 * niffs_run_tests.c
 *
 *  Created on: Feb 24, 2015
 *      Author: petera
 */

#include "testrunner.h"
#include "niffs_test_emul.h"

static u32_t trand(void) {
  static u32_t seed = 0x12312112;
  srand(seed);
  u32_t r = rand();
  seed = r;
  return r;
}

SUITE(niffs_run_tests)

void setup(test *t) {
  (void)niffs_emul_init();
  NIFFS_format(&fs);
  NIFFS_mount(&fs);
}

void teardown(test *t) {
  niffs_emul_destroy_all_data();
}

TEST(run_create_many_small)
{
  int files = 100 * (fs.sectors - 1) * fs.pages_per_sector;
  int fileno = 0;
  int res;
  u32_t mlen = _NIFFS_SPIX_2_PDATA_LEN(&fs, 0);

  while (fileno < files) {
    char name[NIFFS_NAME_LEN];
    sprintf(name, "file%i", fileno);
    u32_t len = mlen - (fileno % (mlen/2));
    u8_t *data = niffs_emul_create_data(name, len);
    int fd = NIFFS_open(&fs, name, NIFFS_O_CREAT | NIFFS_O_EXCL | NIFFS_O_RDWR, 0);
    if (fd >= 0) {
      res = NIFFS_write(&fs, fd, data, len);
      if (res == ERR_NIFFS_FULL) {
        int res2 = NIFFS_fremove(&fs, fd);
        TEST_CHECK_EQ(res2, NIFFS_OK);
      } else {
        TEST_CHECK_EQ(res, len);
        (void)NIFFS_close(&fs, fd);
        res = NIFFS_OK;
      }
    } else {
      res = fd;
    }
    if (res == NIFFS_OK) {
      fileno++;
      if ((fileno % (files/50))==0) {
        printf(".");
        fflush(stdout);
      }
    } else if (res == ERR_NIFFS_FULL) {
      niffs_emul_destroy_data(name);
      niffs_emul_reset_data_cursor();
      int ix = 0;
      char *dname;
      while ((dname = niffs_emul_get_next_data_name())) {
        ix++;
        if (((ix + fileno) % 5) == 0) {
          res = NIFFS_remove(&fs, dname);
          TEST_CHECK_EQ(res, NIFFS_OK);
          niffs_emul_destroy_data(dname);
        }
      }
    } else {
      TEST_CHECK_EQ(res, NIFFS_OK);
    }
  }

  niffs_emul_reset_data_cursor();
  char *dname;
  while ((dname = niffs_emul_get_next_data_name())) {
    TEST_CHECK_EQ(niffs_emul_verify_file(&fs, dname), NIFFS_OK);
  }

  printf("\n");
  return TEST_RES_OK;
}
TEST_END(run_create_many_small)

TEST(run_create_many_medium)
{
  int files = 100 * (fs.sectors - 1);
  int fileno = 0;
  int res;
  u32_t mlen = _NIFFS_SPIX_2_PDATA_LEN(&fs, 1) * fs.pages_per_sector;

  while (fileno < files) {
    char name[NIFFS_NAME_LEN];
    sprintf(name, "file%i", fileno);
    u32_t len = mlen - (fileno % (mlen/2));
    u8_t *data = niffs_emul_create_data(name, len);
    int fd = NIFFS_open(&fs, name, NIFFS_O_CREAT | NIFFS_O_EXCL | NIFFS_O_RDWR, 0);

    if (fd >= 0) {
      res = NIFFS_write(&fs, fd, data, len);
      if (res == ERR_NIFFS_FULL) {
        int res2 = NIFFS_fremove(&fs, fd);
        TEST_CHECK_EQ(res2, NIFFS_OK);
      } else {
        TEST_CHECK_EQ(res, len);
        (void)NIFFS_close(&fs, fd);
        res = NIFFS_OK;
      }
    } else {
      res = fd;
    }
    if (res == NIFFS_OK) {
      fileno++;
      if ((fileno % (files/50))==0) {
        printf(".");
        fflush(stdout);
      }
    } else if (res == ERR_NIFFS_FULL) {
      niffs_emul_destroy_data(name);
      niffs_emul_reset_data_cursor();
      int ix = 0;
      char *dname;
      while ((dname = niffs_emul_get_next_data_name())) {
        ix++;
        if (((ix + fileno) % 5) == 0) {
          res = NIFFS_remove(&fs, dname);
          TEST_CHECK_EQ(res, NIFFS_OK);
          niffs_emul_destroy_data(dname);
        }
      }
    } else {
      TEST_CHECK_EQ(res, NIFFS_OK);
    }
  }

  niffs_emul_reset_data_cursor();
  char *dname;
  while ((dname = niffs_emul_get_next_data_name())) {
    TEST_CHECK_EQ(niffs_emul_verify_file(&fs, dname), NIFFS_OK);
  }

  printf("\n");
  return TEST_RES_OK;
}
TEST_END(run_create_many_medium)

TEST(run_create_many_large)
{
  int files = 100 * (fs.sectors - 1);
  int fileno = 0;
  int res;
  u32_t mlen = _NIFFS_SPIX_2_PDATA_LEN(&fs, 1) * fs.pages_per_sector * (fs.sectors / 3);

  while (fileno < files) {
    char name[NIFFS_NAME_LEN];
    sprintf(name, "file%i", fileno);
    u32_t len = mlen - (fileno % (mlen/2));
    u8_t *data = niffs_emul_create_data(name, len);
    int fd = NIFFS_open(&fs, name, NIFFS_O_CREAT | NIFFS_O_EXCL | NIFFS_O_RDWR, 0);

    if (fd >= 0) {
      res = NIFFS_write(&fs, fd, data, len);
      if (res == ERR_NIFFS_FULL) {
        int res2 = NIFFS_fremove(&fs, fd);
        TEST_CHECK_EQ(res2, NIFFS_OK);
      } else {
        TEST_CHECK_EQ(res, len);
        (void)NIFFS_close(&fs, fd);
        res = NIFFS_OK;
      }
    } else {
      res = fd;
    }
    if (res == NIFFS_OK) {
      fileno++;
      if ((fileno % (files/50))==0) {
        printf(".");
        fflush(stdout);
      }
    } else if (res == ERR_NIFFS_FULL) {
      niffs_emul_destroy_data(name);
      niffs_emul_reset_data_cursor();
      int ix = 0;
      char *dname;
      while ((dname = niffs_emul_get_next_data_name())) {
        ix++;
        if (((ix + fileno) % 2) == 0) {
          res = NIFFS_remove(&fs, dname);
          TEST_CHECK_EQ(res, NIFFS_OK);
          niffs_emul_destroy_data(dname);
        }
      }
    } else {
      TEST_CHECK_EQ(res, NIFFS_OK);
    }
  }

  niffs_emul_reset_data_cursor();
  char *dname;
  while ((dname = niffs_emul_get_next_data_name())) {
    TEST_CHECK_EQ(niffs_emul_verify_file(&fs, dname), NIFFS_OK);
  }

  printf("\n");
  return TEST_RES_OK;
}
TEST_END(run_create_many_large)

TEST(run_create_huge)
{
  int files = 100;
  int fileno = 0;
  int res;
  u32_t mlen = _NIFFS_SPIX_2_PDATA_LEN(&fs, 1) * fs.pages_per_sector * (fs.sectors - 2);

  while (fileno < files) {
    char name[NIFFS_NAME_LEN];
    sprintf(name, "file%i", fileno);
    u32_t len = mlen - (fileno % (mlen/2));
    u8_t *data = niffs_emul_create_data(name, len);
    int fd = NIFFS_open(&fs, name, NIFFS_O_CREAT | NIFFS_O_EXCL | NIFFS_O_RDWR, 0);
    if (fd >= 0) {
      res = NIFFS_write(&fs, fd, data, len);
      if (res == ERR_NIFFS_FULL) {
        int res2 = NIFFS_fremove(&fs, fd);
        TEST_CHECK_EQ(res2, NIFFS_OK);
      } else {
        TEST_CHECK_EQ(res, len);
        (void)NIFFS_close(&fs, fd);
        res = NIFFS_OK;
      }
    } else {
      res = fd;
    }
    if (res == NIFFS_OK) {
      fileno++;
      if ((fileno % (files/50))==0) {
        printf(".");
        fflush(stdout);
      }
    } else if (res == ERR_NIFFS_FULL) {
      niffs_emul_destroy_data(name);
      niffs_emul_reset_data_cursor();
      int ix = 0;
      char *dname;
      while ((dname = niffs_emul_get_next_data_name())) {
        res = NIFFS_remove(&fs, dname);
        TEST_CHECK_EQ(res, NIFFS_OK);
        niffs_emul_destroy_data(dname);
      }
    } else {
      TEST_CHECK_EQ(res, NIFFS_OK);
    }
  }

  niffs_emul_reset_data_cursor();
  char *dname;
  while ((dname = niffs_emul_get_next_data_name())) {
    TEST_CHECK_EQ(niffs_emul_verify_file(&fs, dname), NIFFS_OK);
  }

  printf("\n");
  return TEST_RES_OK;
}
TEST_END(run_create_huge)

TEST(run_create_many_garbled)
{
  int files = 100 * (fs.sectors - 1) * fs.pages_per_sector;
  int fileno = 0;
  int res;
  u32_t mlen = _NIFFS_SPIX_2_PDATA_LEN(&fs, 1) * fs.pages_per_sector * fs.sectors / 4;

  while (fileno < files) {
    char name[NIFFS_NAME_LEN];
    sprintf(name, "file%i", fileno);
    u32_t len = 1 + (trand() % mlen);
    u8_t *data = niffs_emul_create_data(name, len);
    int fd = NIFFS_open(&fs, name, NIFFS_O_CREAT | NIFFS_O_EXCL | NIFFS_O_RDWR, 0);
    if (fd >= 0) {
      res = NIFFS_write(&fs, fd, data, len);
      if (res == ERR_NIFFS_FULL) {
        int res2 = NIFFS_fremove(&fs, fd);
        TEST_CHECK_EQ(res2, NIFFS_OK);
      } else {
        TEST_CHECK_EQ(res, len);
        (void)NIFFS_close(&fs, fd);
        res = NIFFS_OK;
      }
    } else {
      res = fd;
    }
    if (res == NIFFS_OK) {
      fileno++;
      if ((fileno % (files/50))==0) {
        printf(".");
        fflush(stdout);
      }
    } else if (res == ERR_NIFFS_FULL) {
      niffs_emul_destroy_data(name);
      niffs_emul_reset_data_cursor();
      int ix = 0;
      char *dname;
      while ((dname = niffs_emul_get_next_data_name())) {
        ix++;
        if (((ix + fileno) % 5) == 0) {
          res = NIFFS_remove(&fs, dname);
          TEST_CHECK_EQ(res, NIFFS_OK);
          niffs_emul_destroy_data(dname);
        }
      }
    } else {
      TEST_CHECK_EQ(res, NIFFS_OK);
    }
  }

  niffs_emul_reset_data_cursor();
  char *dname;
  while ((dname = niffs_emul_get_next_data_name())) {
    TEST_CHECK_EQ(niffs_emul_verify_file(&fs, dname), NIFFS_OK);
  }

  printf("\n");
  return TEST_RES_OK;
}
TEST_END(run_create_many_garbled)

TEST(run_create_many_garbled_one_constant)
{
  int files = 100 * (fs.sectors - 1) * fs.pages_per_sector;
  int fileno = 0;
  int res;
  u32_t mlen = _NIFFS_SPIX_2_PDATA_LEN(&fs, 1) * fs.pages_per_sector * fs.sectors / 4;
  u32_t seed = 0x12312312;

  TEST_CHECK_EQ(niffs_emul_create_file(&fs, "constant", mlen/2), NIFFS_OK);

  while (fileno < files) {
    char name[NIFFS_NAME_LEN];
    sprintf(name, "file%i", fileno);
    u32_t len = 1 + (trand() % mlen);
    u8_t *data = niffs_emul_create_data(name, len);
    int fd = NIFFS_open(&fs, name, NIFFS_O_CREAT | NIFFS_O_EXCL | NIFFS_O_RDWR, 0);
    if (fd >= 0) {
      res = NIFFS_write(&fs, fd, data, len);
      if (res == ERR_NIFFS_FULL) {
        int res2 = NIFFS_fremove(&fs, fd);
        TEST_CHECK_EQ(res2, NIFFS_OK);
      } else {
        TEST_CHECK_EQ(res, len);
        (void)NIFFS_close(&fs, fd);
        res = NIFFS_OK;
      }
    } else {
      res = fd;
    }
    if (res == NIFFS_OK) {
      fileno++;
      if ((fileno % (files/50))==0) {
        printf(".");
        fflush(stdout);
      }
    } else if (res == ERR_NIFFS_FULL) {
      niffs_emul_destroy_data(name);
      niffs_emul_reset_data_cursor();
      int ix = 0;
      char *dname;
      while ((dname = niffs_emul_get_next_data_name())) {
        ix++;
        if (strcmp("constant", dname) == 0) continue;
        if (((ix + fileno) % 3) == 0) {
          res = NIFFS_remove(&fs, dname);
          TEST_CHECK_EQ(res, NIFFS_OK);
          niffs_emul_destroy_data(dname);
        }
      }
    } else {
      TEST_CHECK_EQ(res, NIFFS_OK);
    }
  }

  niffs_emul_reset_data_cursor();
  char *dname;
  while ((dname = niffs_emul_get_next_data_name())) {
    TEST_CHECK_EQ(niffs_emul_verify_file(&fs, dname), NIFFS_OK);
  }

  printf("\n");
  return TEST_RES_OK;
}
TEST_END(run_create_many_garbled_one_constant)

TEST(run_create_many_garbled_one_constant_aborted)
{
  int files = 500 * (fs.sectors - 1) * fs.pages_per_sector;
  int fileno = 0;
  int res;
  u32_t mlen = _NIFFS_SPIX_2_PDATA_LEN(&fs, 1) * fs.pages_per_sector * fs.sectors / 4;

  TEST_CHECK_EQ(niffs_emul_create_file(&fs, "constant", mlen/2), NIFFS_OK);

  while (fileno < files) {
    u8_t data_freed = 0;
    char name[NIFFS_NAME_LEN];
    sprintf(name, "file%i", fileno);
    u32_t len = 1 + (trand() % mlen);
    u8_t *data = niffs_emul_create_data(name, len);

    printf("create %s %i bytes\n", name, len);
    int abort = (trand() % 100) < 10;
    if (abort) {
      u32_t abort_len = 1 + (trand() % len );
      niffs_emul_set_write_byte_limit(abort_len);
      printf("abort after %i bytes (of %i)\n", abort_len, len);
    } else {
      niffs_emul_set_write_byte_limit(0);
    }

    if (fileno == 14192) {
      // TODO
      //__dbg = 1;
    }

    // try creating
    int fd = NIFFS_open(&fs, name, NIFFS_O_CREAT | NIFFS_O_EXCL | NIFFS_O_RDWR, 0);
    if (fd >= 0) {
      res = NIFFS_write(&fs, fd, data, len);
      if (res == ERR_NIFFS_FULL) {
        int res2 = NIFFS_fremove(&fs, fd);
        if (res2 == ERR_NIFFS_TEST_ABORTED_WRITE) {
          res = res2;
          (void)NIFFS_close(&fs, fd);
        } else {
          TEST_CHECK_EQ(res2, NIFFS_OK);
        }
      } else if (res == ERR_NIFFS_TEST_ABORTED_WRITE) {
        (void)NIFFS_close(&fs, fd);
      } else {
        TEST_CHECK_EQ(res, len);
        (void)NIFFS_close(&fs, fd);
        res = NIFFS_OK;
      }
    } else {
      res = fd;
    }
    if (res == NIFFS_OK) {
      fileno++;
      if ((fileno % (files/50))==0) {
        printf(".");
        fflush(stdout);
      }
    } else if (res == ERR_NIFFS_FULL) {
      // full, verify all and remove
      niffs_emul_destroy_data(name);
      data_freed = 1;
      printf("full on %s\n", name);

      printf("verify when full\n");
      niffs_emul_reset_data_cursor();
      char *dname;
      while ((dname = niffs_emul_get_next_data_name())) {
        printf("  .. check %s\n", dname);
        res = niffs_emul_verify_file(&fs, dname);
        if (res != NIFFS_OK) {
          NIFFS_dump(&fs);
        }
        TEST_CHECK_EQ(res, NIFFS_OK);
      }

      niffs_emul_reset_data_cursor();
      int ix = 0;
      while (res == NIFFS_OK && (dname = niffs_emul_get_next_data_name())) {
        ix++;
        if (strcmp("constant", dname) == 0) continue;
        if (((ix + fileno) % 3) == 0) {
          printf("remove %s\n", dname);
          res = NIFFS_remove(&fs, dname);
          if (res != ERR_NIFFS_TEST_ABORTED_WRITE) {
            TEST_CHECK_EQ(res, NIFFS_OK);
          }
          niffs_emul_destroy_data(dname);
        }
      }
    }

    if (res == ERR_NIFFS_TEST_ABORTED_WRITE) {
      // aborted, check consistency
      printf("aborted on %s\n", name);
      if (!data_freed) {
        niffs_emul_destroy_data(name);
      }
      TEST_CHECK_EQ(NIFFS_unmount(&fs), NIFFS_OK);
      res = NIFFS_chk(&fs);
      if (res == ERR_NIFFS_OVERFLOW) {
        // crammed, remove all but constant
        TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);
        printf("niffs overflow %s\n", name);
        niffs_emul_set_write_byte_limit(0);
        niffs_emul_reset_data_cursor();
        int ix = 0;
        char *dname;
        while ((dname = niffs_emul_get_next_data_name())) {
          ix++;
          if (strcmp("constant", dname) == 0) continue;
          printf("remove %s\n", dname);
          res = NIFFS_remove(&fs, dname);
          TEST_CHECK_EQ(res, NIFFS_OK);
          niffs_emul_destroy_data(dname);
        }
        TEST_CHECK_EQ(NIFFS_unmount(&fs), NIFFS_OK);
        res = NIFFS_chk(&fs);
      }
      if (res != NIFFS_OK) {
        NIFFS_dump(&fs);
      }
      TEST_CHECK_EQ(res, NIFFS_OK);

      TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);
      TEST_CHECK_EQ(niffs_emul_remove_all_zerosized_files(&fs), NIFFS_OK);

      // check all files after cleanup
      niffs_emul_reset_data_cursor();
      char *dname;
      printf("verify after check\n");

      while ((dname = niffs_emul_get_next_data_name())) {
        printf("  .. check %s\n", dname);
        res = niffs_emul_verify_file(&fs, dname);
        if (res != NIFFS_OK) {
          NIFFS_dump(&fs);
        }
        TEST_CHECK_EQ(res, NIFFS_OK);
      }
    }

    // check for garbage files
    printf("find crap\n");
    niffs_DIR d;
    struct niffs_dirent e;
    struct niffs_dirent *pe = &e;

    NIFFS_opendir(&fs, "/", &d);
    while ((pe = NIFFS_readdir(&d, pe))) {
      if (pe->size > mlen || !(strncmp("file", pe->name, 4) == 0 || strncmp("const", pe->name, 5)== 0)) {
        printf("found crap file pix %04x  length %i  objid %04x  name %s\n", pe->pix, pe->size, pe->obj_id, pe->name);
        niffs_emul_dump_pix(&fs, pe->pix);
        TEST_CHECK(0);
      }
    }
    NIFFS_closedir(&d);


    if (res != NIFFS_OK) {
      NIFFS_dump(&fs);
    }

    TEST_CHECK_EQ(res, NIFFS_OK);

    u32_t era_min, era_max;
    niffs_emul_get_sector_erase_count_info(&fs, &era_min, &era_max);
    if (era_max > 0) {
      printf("ERA INF min:%i max:%i span:%i\n", era_min, era_max, ((era_max - era_min)*100)/era_max);
      if (era_max > 100) TEST_CHECK(((era_max - era_min)*100)/era_max < 50);
    }
  }

  printf("verify final\n");
  niffs_emul_reset_data_cursor();
  char *dname;
  while ((dname = niffs_emul_get_next_data_name())) {
    printf("  .. check %s\n", dname);
    TEST_CHECK_EQ(niffs_emul_verify_file(&fs, dname), NIFFS_OK);
  }

  printf("\n");
  return TEST_RES_OK;
}
TEST_END(run_create_many_garbled_one_constant_aborted)

SUITE_END(niffs_run_tests)
