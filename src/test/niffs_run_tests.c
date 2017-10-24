/*
 * niffs_run_tests.c
 *
 *  Created on: Feb 24, 2015
 *      Author: petera
 */

#include "testrunner.h"
#include "niffs_test_emul.h"

#define NIFFS_DBG_TEST(...) //printf(__VA_ARGS__)

static u32_t trand(void) {
  static u32_t seed = 0x12312112;
  srand(seed);
  u32_t r = rand();
  seed = r;
  return r;
}

SUITE(niffs_run_tests)

static void setup(test *t) {
  (void)niffs_emul_init();
  NIFFS_format(&fs);
  NIFFS_mount(&fs);
}

static void teardown(test *t) {
  if (t->test_result != TEST_RES_OK) {
    NIFFS_dump(&fs);
  }
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
TEST_END

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
      char *dname;
      while ((dname = niffs_emul_get_next_data_name())) {
        TEST_CHECK_EQ(niffs_emul_verify_file(&fs, dname), NIFFS_OK);
      }

      niffs_emul_reset_data_cursor();
      int ix = 0;
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
TEST_END

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
      char *dname;
      while ((dname = niffs_emul_get_next_data_name())) {
        TEST_CHECK_EQ(niffs_emul_verify_file(&fs, dname), NIFFS_OK);
      }

      niffs_emul_reset_data_cursor();
      int ix = 0;
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
TEST_END

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
      char *dname;
      while ((dname = niffs_emul_get_next_data_name())) {
        TEST_CHECK_EQ(niffs_emul_verify_file(&fs, dname), NIFFS_OK);
      }

      niffs_emul_reset_data_cursor();
      int ix = 0;
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
TEST_END

// loop:
//  create random sized files until full
//  when full, check current and remove some
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
      char *dname;
      while ((dname = niffs_emul_get_next_data_name())) {
        TEST_CHECK_EQ(niffs_emul_verify_file(&fs, dname), NIFFS_OK);
      }

      niffs_emul_reset_data_cursor();
      int ix = 0;
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
TEST_END

// create constant file, never touched
// loop:
//  create random sized files until full
//  when full, check current and remove some
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
      char *dname;
      while ((dname = niffs_emul_get_next_data_name())) {
        TEST_CHECK_EQ(niffs_emul_verify_file(&fs, dname), NIFFS_OK);
      }

      niffs_emul_reset_data_cursor();
      int ix = 0;
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
TEST_END


// aborted create, gc, rm test
//
// create constant file which is never removed
// loop:
//  create random sized files until full
//  when full, check current and remove some
//  abort sporadically
//  when aborted, run check
//  if check gives overflow, remove all but constant
TEST(run_create_many_garbled_one_constant_aborted)
{
  int files = 500 * (fs.sectors - 1) * fs.pages_per_sector;
  int fileno = 0;
  int res;
  u32_t mlen = _NIFFS_SPIX_2_PDATA_LEN(&fs, 1) * fs.pages_per_sector * fs.sectors / 4;

  TEST_CHECK_EQ(niffs_emul_create_file(&fs, "constant", mlen/2), NIFFS_OK);

  while (fileno < files) {
    u8_t data_already_freed = 0;
    char name[NIFFS_NAME_LEN];
    sprintf(name, "file%i", fileno);
    u32_t len = 1 + (trand() % mlen);
    u8_t *data = niffs_emul_create_data(name, len);

    NIFFS_DBG_TEST("create %s %i bytes\n", name, len);

    // random power loss simulation
    int abort = (trand() % 100) < 10;
    if (abort) {
      u32_t abort_len = 1 + (trand() % len );
      niffs_emul_set_write_byte_limit(abort_len);
      NIFFS_DBG_TEST("abort after %i bytes (of %i)\n", abort_len, len);
    } else {
      niffs_emul_set_write_byte_limit(0);
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
      // full, verify all and remove some
      niffs_emul_destroy_data(name);
      data_already_freed = 1;
      NIFFS_DBG_TEST("full on %s\n", name);

      NIFFS_DBG_TEST("verify when full\n");
      niffs_emul_reset_data_cursor();
      char *dname;
      while ((dname = niffs_emul_get_next_data_name())) {
        NIFFS_DBG_TEST("  .. check %s\n", dname);
        res = niffs_emul_verify_file(&fs, dname);
        if (res != NIFFS_OK) {
          NIFFS_dump(&fs);
        }
        TEST_CHECK_EQ(res, NIFFS_OK);
      }

      int deleted = 0;
      int ix = 0;
      int runs = 0;
      while (deleted == 0) {
        niffs_emul_reset_data_cursor();
        while (res == NIFFS_OK && (dname = niffs_emul_get_next_data_name())) {
          ix++;
          if (strcmp("constant", dname) == 0) continue;
          if (((ix + fileno) % MAX(1, 3-runs)) == 0) {
            NIFFS_DBG_TEST("remove %s\n", dname);
            res = NIFFS_remove(&fs, dname);
            if (res != ERR_NIFFS_TEST_ABORTED_WRITE) {
              TEST_CHECK_EQ(res, NIFFS_OK);
            }
            niffs_emul_destroy_data(dname);
            deleted++;
          }
        }
        runs++;
        if (runs == 100) {
          // too many remove runs, major failure
          niffs_DIR d;
          struct niffs_dirent e;
          struct niffs_dirent *pe = &e;

          NIFFS_DBG_TEST("niffs contents\n");
          NIFFS_opendir(&fs, "/", &d);
          while ((pe = NIFFS_readdir(&d, pe))) {
            NIFFS_DBG_TEST(" * %s %i  oid:%04x  pix:%04x\n", pe->name, pe->size, pe->obj_id, pe->pix);
            if (pe->size > mlen || !(strncmp("file", pe->name, 4) == 0 || strncmp("const", pe->name, 5)== 0)) {
              NIFFS_DBG_TEST("found crap file pix %04x  length %i  objid %04x  name %s\n", pe->pix, pe->size, pe->obj_id, pe->name);
              niffs_emul_dump_pix(&fs, pe->pix);
              TEST_CHECK(0);
            }
          }
          NIFFS_closedir(&d);

          NIFFS_DBG_TEST("ram contents\n");
          niffs_emul_reset_data_cursor();
          while (res == NIFFS_OK && (dname = niffs_emul_get_next_data_name())) {
            NIFFS_DBG_TEST(" * %s\n", dname);
          }

          NIFFS_dump(&fs);
          TEST_CHECK(0);
        }
      }
    }

    if (res == ERR_NIFFS_TEST_ABORTED_WRITE) {
      // aborted, check consistency
      NIFFS_DBG_TEST("aborted on %s\n", name);
      if (!data_already_freed) {
        niffs_emul_destroy_data(name);
      }
      TEST_CHECK_EQ(NIFFS_unmount(&fs), NIFFS_OK);
      res = NIFFS_chk(&fs);
      if (res == ERR_NIFFS_OVERFLOW) {
        // crammed, remove all but constant
        TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);
        NIFFS_DBG_TEST("niffs overflow %s\n", name);
        niffs_emul_set_write_byte_limit(0);
        niffs_emul_reset_data_cursor();
        int ix = 0;
        char *dname;
        while ((dname = niffs_emul_get_next_data_name())) {
          ix++;
          if (strcmp("constant", dname) == 0) continue;
          NIFFS_DBG_TEST("remove %s\n", dname);
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
      NIFFS_DBG_TEST("verify after check\n");

      while ((dname = niffs_emul_get_next_data_name())) {
        NIFFS_DBG_TEST("  .. check %s\n", dname);
        res = niffs_emul_verify_file(&fs, dname);
        if (res != NIFFS_OK) {
          NIFFS_dump(&fs);
        }
        TEST_CHECK_EQ(res, NIFFS_OK);
      }
    }

    // check for garbage files
    NIFFS_DBG_TEST("find crap\n");
    niffs_DIR d;
    struct niffs_dirent e;
    struct niffs_dirent *pe = &e;

    NIFFS_opendir(&fs, "/", &d);
    while ((pe = NIFFS_readdir(&d, pe))) {
      if (pe->size > mlen || !(strncmp("file", pe->name, 4) == 0 || strncmp("const", pe->name, 5)== 0)) {
        NIFFS_DBG_TEST("found crap file pix %04x  length %i  objid %04x  name %s\n", pe->pix, pe->size, pe->obj_id, pe->name);
        niffs_emul_dump_pix(&fs, pe->pix);
        TEST_CHECK(0);
      }
    }
    NIFFS_closedir(&d);

    if (res != NIFFS_OK) {
      NIFFS_dump(&fs);
    }

    TEST_CHECK_EQ(res, NIFFS_OK);

    // check erase span
    u32_t era_min, era_max;
    niffs_emul_get_sector_erase_count_info(&fs, &era_min, &era_max);
    if (era_max > 0) {
      NIFFS_DBG_TEST("ERA INF min:%i max:%i span:%i\n", era_min, era_max, ((era_max - era_min)*100)/era_max);
      if (era_max > 100) TEST_CHECK_LT(((era_max - era_min)*100)/era_max, 50);
    }
  } // main loop

  NIFFS_DBG_TEST("verify final\n");
  niffs_emul_reset_data_cursor();
  char *dname;
  while ((dname = niffs_emul_get_next_data_name())) {
    NIFFS_DBG_TEST("  .. check %s\n", dname);
    TEST_CHECK_EQ(niffs_emul_verify_file(&fs, dname), NIFFS_OK);
  }

  printf("\n");

  return TEST_RES_OK;
}
TEST_END

// aborted create, gc, rm, append test
//
// create constant file which is never removed
// loop:
//  if less than 5 files, create files, random sized
//  if more or equal, modify/append files until full
//  sometimes, modify constant file, one page only
//  when full, remove one
//  abort sporadically
//  when aborted, run check
//  if check gives overflow, remove all but constant
TEST(run_create_modify_append_some_garbled_one_constant_aborted)
{
  int run = 0;
  int runs = 500 * (fs.sectors - 1) * fs.pages_per_sector;
  int fileno = 0;
  int active_files = 0;
  int res;
  u32_t mlen = _NIFFS_SPIX_2_PDATA_LEN(&fs, 1) * fs.pages_per_sector * fs.sectors / 8;

  u32_t const_len =  _NIFFS_SPIX_2_PDATA_LEN(&fs, 1) * 3;
  TEST_CHECK_EQ(niffs_emul_create_file(&fs, "constant", const_len), NIFFS_OK);
  char name[NIFFS_NAME_LEN];

  while (run < runs) {
    NIFFS_DBG_TEST("run#%i\n", run);
    //TODO if (run == 11660) __dbg = 1;
    //TODO if (run >= 11660) NIFFS_dump(&fs);
    u8_t data_already_freed = 0;
    u8_t create_else_mod = 0;

    if (active_files < 5) {

      // create file

      create_else_mod = 1;
      sprintf(name, "file%i", fileno);
      u32_t len = 1 + (trand() % mlen);
      u8_t *data = niffs_emul_create_data(name, len);
      NIFFS_DBG_TEST("create %s %i bytes\n", name, len);

      // random power loss simulation
      int abort = (trand() % 100) < 10;
      if (abort) {
        u32_t abort_len = 1 + (trand() % len );
        niffs_emul_set_write_byte_limit(abort_len);
        NIFFS_DBG_TEST("abort after %i bytes (of %i)\n", abort_len, len);
      } else {
        niffs_emul_set_write_byte_limit(0);
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
        active_files++;
      }

    } else {

      // find file and modify/append

      create_else_mod = 0;
      u32_t len;
      u32_t offs;
      u32_t flen;
      int fd;
      if (active_files == 0 || trand() % 100 < 20) {
        // select constant file
        strcpy(name, "constant");
        NIFFS_DBG_TEST("select %s\n", name);

        fd = NIFFS_open(&fs, name, NIFFS_O_RDWR, 0);
        if (fd < 0) res = fd;
        TEST_CHECK_EQ(res, NIFFS_OK);
        niffs_stat s;
        res = NIFFS_fstat(&fs, fd, &s);
        TEST_CHECK_EQ(res, NIFFS_OK);
        TEST_CHECK_EQ(s.size, const_len);
        len = 1 + (trand() % (1 + s.size/3));
        offs = trand() % (s.size - len);
        NIFFS_ASSERT(len+offs < s.size);
        flen = s.size;
      } else {
        // select arbitrary other file
        char *dname;
        u32_t selected = 0;
        u32_t ix = 0;
        u32_t sel_ix = trand() % active_files;
        while (selected == 0) {
          niffs_emul_reset_data_cursor();
          while (res == NIFFS_OK && (dname = niffs_emul_get_next_data_name())) {
            if (strcmp("constant", dname) == 0) continue;
            if (ix >= sel_ix) {
              selected = 1;
              break;
            }
            ix++;
          }
        }

        strcpy(name, dname);
        NIFFS_DBG_TEST("select %s\n", name);

        fd = NIFFS_open(&fs, name, NIFFS_O_RDWR, 0);
        if (fd < 0) res = fd;
        TEST_CHECK_EQ(res, NIFFS_OK);
        niffs_stat s;
        res = NIFFS_fstat(&fs, fd, &s);
        TEST_CHECK_EQ(res, NIFFS_OK);
        len = 1 + (trand() % (1+s.size/3));
        offs = s.size / 2 + trand() % (1+s.size / 2);
        flen = s.size;
      }

      NIFFS_DBG_TEST("modify %s %i bytes offs %i / %i\n", name, len, offs, flen);

      res = NIFFS_lseek(&fs, fd, offs, NIFFS_SEEK_SET);
      TEST_CHECK_EQ(res, offs);

      u8_t *mdata;
      if (strcmp("constant", name) == 0) {
        u32_t dummy;
        mdata = niffs_emul_get_data("constant", &dummy);
        mdata += offs;
      } else {
        mdata = malloc(len);
      }
      NIFFS_ASSERT(mdata);
      memrand(mdata, len, run*0xead1c9f1);
      res = NIFFS_write(&fs, fd, mdata, len);
      if (strcmp("constant", name)) {
        free(mdata);
      }

      (void)NIFFS_close(&fs, fd);
      if (res != ERR_NIFFS_FULL && res != ERR_NIFFS_TEST_ABORTED_WRITE) {
        TEST_CHECK_EQ(res, len);
        res = NIFFS_OK;
      }
    }

    // handle problems

    if (res == ERR_NIFFS_FULL) {
      res = NIFFS_OK;
      data_already_freed = 1;
      NIFFS_DBG_TEST("full on %s, remove\n", name);
      if (create_else_mod) niffs_emul_destroy_data(name);

      int deleted = 0;
      int ix = 0;
      int remove_runs = 0;
      char *dname;
      while (deleted == 0) {
        niffs_emul_reset_data_cursor();
        while (res == NIFFS_OK && deleted == 0 && (dname = niffs_emul_get_next_data_name())) {
          ix++;
          if (strcmp("constant", dname) == 0) continue;
          if (((ix + fileno) % MAX(1, 3-remove_runs)) == 0) {
            NIFFS_DBG_TEST("remove %s\n", dname);
            res = NIFFS_remove(&fs, dname);
            if (res != ERR_NIFFS_TEST_ABORTED_WRITE) {
              TEST_CHECK_EQ(res, NIFFS_OK);
            }
            niffs_emul_destroy_data(dname);
            deleted++;
            active_files--;
          }
        }
        remove_runs++;
        if (remove_runs == 100) {
          // too many remove runs, major failure
          niffs_DIR d;
          struct niffs_dirent e;
          struct niffs_dirent *pe = &e;

          NIFFS_DBG_TEST("niffs contents\n");
          NIFFS_opendir(&fs, "/", &d);
          while ((pe = NIFFS_readdir(&d, pe))) {
            NIFFS_DBG_TEST(" * %s %i  oid:%04x  pix:%04x\n", pe->name, pe->size, pe->obj_id, pe->pix);
            if (pe->size > mlen || !(strncmp("file", pe->name, 4) == 0 || strncmp("const", pe->name, 5)== 0)) {
              NIFFS_DBG_TEST("found crap file pix %04x  length %i  objid %04x  name %s\n", pe->pix, pe->size, pe->obj_id, pe->name);
              niffs_emul_dump_pix(&fs, pe->pix);
              TEST_CHECK(0);
            }
          }
          NIFFS_closedir(&d);

          NIFFS_DBG_TEST("ram contents\n");
          niffs_emul_reset_data_cursor();
          while (res == NIFFS_OK && (dname = niffs_emul_get_next_data_name())) {
            NIFFS_DBG_TEST(" * %s\n", dname);
          }

          NIFFS_dump(&fs);
          TEST_CHECK(0);
        }
      }
    }

    if (res == ERR_NIFFS_TEST_ABORTED_WRITE) {
      // aborted, check consistency
      NIFFS_DBG_TEST("aborted on %s\n", name);
      if (!data_already_freed && create_else_mod) {
        niffs_emul_destroy_data(name);
      }
      TEST_CHECK_EQ(NIFFS_unmount(&fs), NIFFS_OK);
      res = NIFFS_chk(&fs);
      if (res == ERR_NIFFS_OVERFLOW) {
        // crammed, remove all but constant
        TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);
        NIFFS_DBG_TEST("niffs overflow %s\n", name);
        niffs_emul_set_write_byte_limit(0);
        niffs_emul_reset_data_cursor();
        int ix = 0;
        char *dname;
        while ((dname = niffs_emul_get_next_data_name())) {
          ix++;
          if (strcmp("constant", dname) == 0) continue;
          NIFFS_DBG_TEST("remove %s\n", dname);
          res = NIFFS_remove(&fs, dname);
          TEST_CHECK_EQ(res, NIFFS_OK);
          niffs_emul_destroy_data(dname);
        }
        active_files = 0;
        TEST_CHECK_EQ(NIFFS_unmount(&fs), NIFFS_OK);
        res = NIFFS_chk(&fs);
      }
      if (res != NIFFS_OK) {
        NIFFS_dump(&fs);
      }
      TEST_CHECK_EQ(res, NIFFS_OK);

      TEST_CHECK_EQ(NIFFS_mount(&fs), NIFFS_OK);
      TEST_CHECK_EQ(niffs_emul_remove_all_zerosized_files(&fs), NIFFS_OK);
    }

    // check "constant" length
    niffs_stat s;
    TEST_CHECK_EQ(NIFFS_stat(&fs, "constant", &s), NIFFS_OK);
    TEST_CHECK_EQ(s.size, const_len);

    // check for garbage files
    niffs_DIR d;
    struct niffs_dirent e;
    struct niffs_dirent *pe = &e;

    NIFFS_opendir(&fs, "/", &d);
    while ((pe = NIFFS_readdir(&d, pe))) {
      if (!(strncmp("file", pe->name, 4) == 0 || strncmp("const", pe->name, 5)== 0)) {
        NIFFS_DBG_TEST("found crap file pix %04x  length %i  objid %04x  name %s\n", pe->pix, pe->size, pe->obj_id, pe->name);
        niffs_emul_dump_pix(&fs, pe->pix);
        TEST_CHECK(0);
      }
    }
    NIFFS_closedir(&d);

    if (res != NIFFS_OK) {
      NIFFS_dump(&fs);
    }

    TEST_CHECK_EQ(res, NIFFS_OK);

    // check erase span
    u32_t era_min, era_max;
    niffs_emul_get_sector_erase_count_info(&fs, &era_min, &era_max);
    if (era_max > 0) {
      NIFFS_DBG_TEST("ERA INF min:%i max:%i span:%i\n", era_min, era_max, ((era_max - era_min)*100)/era_max);
      if (era_max > 100) TEST_CHECK_LT(((era_max - era_min)*100)/era_max, 50);
    }
    run++;
    if ((run % (runs/50))==0) {
      printf(".");
      fflush(stdout);
    }
  } // main loop

  TEST_CHECK_EQ(niffs_emul_verify_file(&fs, "constant"), NIFFS_OK);

  printf("\n");

  return TEST_RES_OK;
}
TEST_END


// full system
TEST(run_full)
{
  int res;
  u32_t of_len = fs.sectors * fs.sector_size;
  u8_t buf[EMUL_PAGE_SIZE];
  memrand(buf, sizeof(buf), 0x20070515);
  int fd = NIFFS_open(&fs, "overflow", NIFFS_O_CREAT | NIFFS_O_APPEND | NIFFS_O_RDWR, 0);
  TEST_CHECK_GE(fd, 0);

  printf("  fill fs but one page\n");
  res = NIFFS_OK;
  while (res >= NIFFS_OK) {
    res = NIFFS_write(&fs, fd, buf, sizeof(buf));
  }
  TEST_CHECK_EQ(res, ERR_NIFFS_FULL);

  niffs_stat s;
  TEST_CHECK_EQ(NIFFS_fstat(&fs, fd, &s), NIFFS_OK);
  printf("    %s\t%i bytes\n", s.name, s.size);

  NIFFS_close(&fs, fd);

  printf("  create a file in last page\n");
  fd = NIFFS_open(&fs, "full", NIFFS_O_CREAT | NIFFS_O_APPEND | NIFFS_O_RDWR, 0);
  TEST_CHECK_GE(fd, 0);
  res = NIFFS_write(&fs, fd, buf, _NIFFS_SPIX_2_PDATA_LEN(&fs, 0));
  TEST_CHECK_GE(res, NIFFS_OK);
  TEST_CHECK_EQ(NIFFS_fstat(&fs, fd, &s), NIFFS_OK);
  printf("    %s\t%i bytes\n", s.name, s.size);
  TEST_CHECK_EQ(s.size, _NIFFS_SPIX_2_PDATA_LEN(&fs, 0));
  printf("  remove file in last page\n");
  TEST_CHECK_EQ(NIFFS_remove(&fs, "full"), NIFFS_OK);

  printf("  create a file in last page again\n");
  fd = NIFFS_open(&fs, "fullagain", NIFFS_O_CREAT | NIFFS_O_APPEND | NIFFS_O_RDWR, 0);
  TEST_CHECK_GE(fd, 0);
  res = NIFFS_write(&fs, fd, buf, _NIFFS_SPIX_2_PDATA_LEN(&fs, 0));
  TEST_CHECK_GE(res, NIFFS_OK);
  TEST_CHECK_EQ(NIFFS_fstat(&fs, fd, &s), NIFFS_OK);
  printf("    %s\t%i bytes\n", s.name, s.size);
  TEST_CHECK_EQ(s.size, _NIFFS_SPIX_2_PDATA_LEN(&fs, 0));
  printf("  remove file in last page\n");
  TEST_CHECK_EQ(NIFFS_remove(&fs, "fullagain"), NIFFS_OK);

  printf("  create a file in last page again, now with too big a size\n");
  fd = NIFFS_open(&fs, "fullof", NIFFS_O_CREAT | NIFFS_O_APPEND | NIFFS_O_RDWR, 0);
  TEST_CHECK_GE(fd, 0);
  res = NIFFS_write(&fs, fd, buf, _NIFFS_SPIX_2_PDATA_LEN(&fs, 0) + 1);
  TEST_CHECK_EQ(res, ERR_NIFFS_FULL);
  TEST_CHECK_EQ(NIFFS_fstat(&fs, fd, &s), NIFFS_OK);
  printf("    %s\t%i bytes\n", s.name, s.size);
  TEST_CHECK_EQ(s.size, 0);

  printf("  remove huge file\n");
  TEST_CHECK_EQ(NIFFS_remove(&fs, "overflow"), NIFFS_OK);

  printf("  write to file in last page again, with the big size\n");
  res = NIFFS_write(&fs, fd, buf, _NIFFS_SPIX_2_PDATA_LEN(&fs, 0) + 1);
  TEST_CHECK_EQ(res, _NIFFS_SPIX_2_PDATA_LEN(&fs, 0) + 1);
  TEST_CHECK_EQ(NIFFS_fstat(&fs, fd, &s), NIFFS_OK);
  printf("    %s\t%i bytes\n", s.name, s.size);
  TEST_CHECK_EQ(s.size, _NIFFS_SPIX_2_PDATA_LEN(&fs, 0) + 1);

  return TEST_RES_OK;
}
TEST_END

TEST(run_full_single_byte)
{
  int res;
  u32_t of_len = fs.sectors * fs.sector_size;
  int fd = NIFFS_open(&fs, "overflow", NIFFS_O_CREAT | NIFFS_O_APPEND | NIFFS_O_RDWR, 0);
  TEST_CHECK_GE(fd, 0);

  printf("  fill fs, byte wise\n");
  res = NIFFS_OK;
  while (res >= NIFFS_OK) {
    u8_t data = 'z';
    res = NIFFS_write(&fs, fd, &data, 1);
  }
  TEST_CHECK_EQ(res, ERR_NIFFS_FULL);

  niffs_stat s;
  TEST_CHECK_EQ(NIFFS_fstat(&fs, fd, &s), NIFFS_OK);
  printf("    %s\t%i bytes\n", s.name, s.size);

  NIFFS_close(&fs, fd);

  printf("  create a file when full\n");
  fd = NIFFS_open(&fs, "full", NIFFS_O_CREAT | NIFFS_O_APPEND | NIFFS_O_RDWR, 0);
  TEST_CHECK_EQ(fd, ERR_NIFFS_FULL);
  TEST_CHECK_EQ(NIFFS_fstat(&fs, fd, &s), ERR_NIFFS_FILEDESC_BAD);
  TEST_CHECK_EQ(NIFFS_stat(&fs, "full", &s), ERR_NIFFS_FILE_NOT_FOUND);

  printf("  remove huge file\n");
  TEST_CHECK_EQ(NIFFS_remove(&fs, "overflow"), NIFFS_OK);

  printf("  create a file again\n");
  fd = NIFFS_open(&fs, "notfull", NIFFS_O_CREAT | NIFFS_O_APPEND | NIFFS_O_RDWR, 0);
  TEST_CHECK_GE(fd, NIFFS_OK);
  int i;
  res = NIFFS_OK;
  for (i = 0; i < 1000 && res >= NIFFS_OK; i++) {
    u8_t data = 'x';
    res = NIFFS_write(&fs, fd, &data, 1);
  }
  TEST_CHECK_GE(res, NIFFS_OK);

  TEST_CHECK_EQ(NIFFS_fstat(&fs, fd, &s), NIFFS_OK);
  printf("    %s\t%i bytes\n", s.name, s.size);
  TEST_CHECK_EQ(s.size, 1000);

  return TEST_RES_OK;
}
TEST_END

SUITE_TESTS(niffs_run_tests)
  ADD_TEST(run_create_many_small)
  ADD_TEST(run_create_many_medium)
  ADD_TEST(run_create_many_large)
  ADD_TEST(run_create_huge)
  ADD_TEST(run_create_many_garbled)
  ADD_TEST(run_create_many_garbled_one_constant)
  ADD_TEST(run_create_many_garbled_one_constant_aborted)
  ADD_TEST(run_create_modify_append_some_garbled_one_constant_aborted)
  ADD_TEST(run_full)
  ADD_TEST(run_full_single_byte)
SUITE_END(niffs_run_tests)
