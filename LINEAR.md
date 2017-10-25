# Linear Area feature

Below is a short description of how the linear feature works in niffs.

The linear area feature allows for having linear files in niffs, meaning files that are not chopped up in pages on medium. This can be handy for e.g. audio, video, code, bytecodes, streaming or FOTA images.

Features of the linear area:

*   Linear files are linear on flash.
*   The linear area is not wear-leveled, so beware doing frequent updates.
*   Each linear file claims at least one sector.
*   Linear files can be created, written, read, seeked, and deleted.
*   Linear files can only be appended when writing - written data cannot be changed.

## Build for linear area feature

Set `NIFFS_LINEAR_AREA` to `1` in your `niffs_config.h`

## Assigning a linear area

When configuring your file system with `NIFFS_config` you specify how many sectors the file system will have the address of the starting sector using arguments `sectors` and `phys_addr`. The argument `lin_sectors` determines how many sectors the linear area comprises. The file system will thus claim `sectors + lin_sectors` number of sectors, starting at `phys_addr`. The linear area is placed after the paged file system sectors.

For example:
```C
NIFFS_init(&niffs_fs, 
           0x00860000,                             /* address of starting sector */
           16,                                     /* number of paged sectors */
           4096,                                   /* logical sector size */
           128,                                    /* logical page size */
           work_buf,                               /* work ram */
           sizeof(work_buf),                       /* work ram length in bytes */
           desc_buf,                               /* file descriptor array */
           sizeof(desc_buf) / sizeof(desc_buf[0]), /* number of file descriptors */
           hal_erase_fn, hal_write_fn,             /* flash HAL functions */
           32);                                    /* number of linear sectors */
```
will look like this on your flash:

| address  | sector | type  |
| -------- | ------ | ----- |
| 0x860000 | x      | paged |
| 0x861000 | x+1    | paged |
|   ...    |   ...  |  ...  |
| 0x86f000 | x+15   | paged |
| 0x870000 | x+16   | linear |
| 0x871000 | x+17   | linear |
|   ...    |   ...  |  ...   |
| 0x88f000 | x+47   | linear |

## Understanding linear files

There are two ways to create linear files.

 * `NIFFS_open` including flags `NIFFS_O_CREAT | NIFFS_O_LINEAR`
 * `NIFFS_mknod_linear`
 
To understand the difference between these, the linear sector assignment must be described. When a linear file is created, 
the linear area is scanned for free sectors. The first found free sector is selected and the file is placed on that sector.
Assume we have a system with five linear sectors:

| sector0 | sector1 | sector2 | sector3 | sector4 |
| ------- | ------- | ------- | ------- | ------- |
| free    | free    | free    | free    | free    |

We create three files, "fileA", "fileB", "fileC". We don't write anything to them yet.
```C
   int fda = NIFFS_open(fs, "fileA", NIFFS_O_CREAT | NIFFS_O_TRUNC | NIFFS_O_RDWR | NIFFS_O_LINEAR, 0);
   int fdb = NIFFS_open(fs, "fileB", NIFFS_O_CREAT | NIFFS_O_TRUNC | NIFFS_O_RDWR | NIFFS_O_LINEAR, 0);
   int fdc = NIFFS_open(fs, "fileC", NIFFS_O_CREAT | NIFFS_O_TRUNC | NIFFS_O_RDWR | NIFFS_O_LINEAR, 0);
```

| sector0 | sector1 | sector2 | sector3 | sector4 |
| ------- | ------- | ------- | ------- | ------- |
| fileA   | fileB   | fileC   | free    | free    |

The meta data for all files resides on the paged part of the filesystem. This means the **linear sectors contain only data**.
Now we write to fileC, two sectors worth of data.

```C
   u8_t data[2 * sector_size];
   // fill data with something useful
   int res = NIFFS_write(fs, fdc, data, sizeof(data));
```

| sector0 | sector1 | sector2 | sector3 | sector4 |
| ------- | ------- | ------- | ------- | ------- |
| fileA   | fileB   | fileC   | fileC(2)| free    |

Now we want to write the same to fileA.

```C
   res = NIFFS_write(fs, fda, data, sizeof(data));
   // BAM: res = ERR_NIFFS_LINEAR_NO_SPACE
```
This will not work, fileA can at most be one sector long as the sector after is occupied.
In order to do this, we must delete fileB first.

```C
   res = NIFFS_fremove(fs, fdb);
   res = NIFFS_write(fs, fda, data, sizeof(data));
   // all dandy
```
| sector0 | sector1 | sector2 | sector3 | sector4 |
| ------- | ------- | ------- | ------- | ------- |
| fileA   | fileA(2)| fileC   | fileC(2)| free    |

But hey, what if I want all three files and I know that fileB only ever will be one sector long, and the other two must 
be at least two sectors long?
Then you use `NIFFS_mknod_linear`. We format our system, and instead of using ´SPIFFS_open` we create our files like this:
```C
   int fda = NIFFS_mknod_linear(fs, "fileA", sector_size*2);
   int fdb = NIFFS_mknod_linear(fs, "fileB", sector_size);
   int fdc = NIFFS_mknod_linear(fs, "fileC", sector_size*2);
```
Still nothing written, but the representation will be like this:

| sector0 | sector1 | sector2 | sector3 | sector4 |
| ------- | ------- | ------- | ------- | ------- |
| fileA   | (fileA) | fileB   | fileC   | (fileC) |

Meaning that fileA and fileC will have two sectors worth of data reserved and are guaranteed to be able to contain two sectors
worth of data. The reservation is a minimum length guarantee. Would the sectors after a file be free, it can of course be longer.

## Using linear files

Opening linear files works just like normal. You do not need to know it is a linear file.
```C
int fd = NIFFS_open(fs, "linearfile", NIFFS_O_RDONLY, 0); // NIFFS_O_LINEAR not needed
```
`NIFFS_O_LINEAR` is only necessary when creating files with `NIFFS_open`.

Also, opening a linear file for writing forcefully sets the `NIFFS_O_APPEND` even if it is not specified.

```C
int fd = NIFFS_open(fs, "linearfile", NIFFS_O_WRONLY, 0);
 // NIFFS_O_LINEAR not needed, and NIFFS_O_APPEND is set under the hood
```

Seeking, reading, writing, statting, and removing works just like any other file.

Again, with the exception that writing is only by append.

**This will not work as expected:**
```C
// THIS WILL NOT WORK AS EXPECTED
int fd = NIFFS_open(fs, "linearfile", NIFFS_O_WRONLY, 0);
// get file size
int size = NIFFS_lseek(fs, fd, 0, NIFFS_SEEK_END);
// position us in the middle of file
int res = NIFFS_lseek(fs, fd, size/2, NIFFS_SEEK_SET);
 // WILL BE WRITTEN TO END OF FILE
res = NIFFS_write(fs, fd, data, len);
```

## Knowing if a file is linear or not
By `NIFFS_stat`, `NIFFS_fstat`, and `NIFFS_readdir` you check the `type` member of these structs.

## When are linear sectors erased?
They are erased on a format, and when overwritten.

