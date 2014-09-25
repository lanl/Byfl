/*********************************************
 * Convert Byfl binary data to NetCDF format *
 * By Scott Pakin <pakin@lanl.gov>           *
 *********************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netcdf.h>
#include <stdarg.h>
#include "binarytagdefs.h"

/* Define a type that represents an open and memory-mapped file. */
typedef struct {
  int fd;           /* File descriptor */
  void *ptr;        /* Pointer to memory-mapped file contents */
  size_t length;    /* Number of bytes mapped */
} mapped_file_t;

/* Store the name of the current executable. */
const char *progname;

/* Complain about a system call failing and abort the program. */
void abort_on_bad_syscall (const char *callname)
{
  perror(callname);
  exit(1);
}

/* Complain about a NetCDF call failing and abort the program. */
void abort_on_bad_netcdf (const char *callname, int errcode)
{
  fprintf(stderr, "%s: %s\n", callname, nc_strerror(errcode));
  exit(1);
}

/* Complain about anything else being wrong and abort the program. */
void abort_on_bad_other (const char *format, ...)
{
  va_list ap;

  fprintf(stderr, "%s: ", progname);
  va_start(ap, format);
  vfprintf(stderr, format, ap);
  va_end(ap);
  fputc('\n', stderr);
  exit(1);
}

/* Map a file into memory.  The caller must free() the resulting structure. */
mapped_file_t *map_file_into_memory (const char *filename)
{
  mapped_file_t *mapping;
  struct stat fileinfo;

  mapping = malloc(sizeof(mapped_file_t));
  if (!mapping)
    abort_on_bad_syscall("malloc");
  mapping->fd = open(filename, O_RDONLY);
  if (mapping->fd == -1)
    abort_on_bad_syscall("open");
  if (fstat(mapping->fd, &fileinfo) == -1)
    abort_on_bad_syscall("fstat");
  mapping->length = fileinfo.st_size;
  mapping->ptr = mmap(NULL, mapping->length, PROT_READ, MAP_SHARED, mapping->fd, 0);
  if (mapping->ptr == MAP_FAILED)
    abort_on_bad_syscall("mmap");
  return mapping;
}

/* Unmap a file from memory. */
void unmap_file_from_memory (const mapped_file_t *mapping)
{
  if (munmap(mapping->ptr, mapping->length) == -1)
    abort_on_bad_syscall("munmap");
  if (close(mapping->fd) == -1)
    abort_on_bad_syscall("close");
}

/* Replace the extension in a filename.  The caller must free() the result. */
char *replace_extension (const char *oldfilename, const char *newext)
{
  char *dotptr;       /* Pointer to the last "." in oldfilename */
  char *newfilename;  /* New filename to return */

  newfilename = malloc(strlen(oldfilename) + strlen(newext) + 1);
  if (newfilename == NULL)
    abort_on_bad_syscall("malloc");
  strcpy(newfilename, oldfilename);
  dotptr = strrchr(newfilename, '.');
  if (dotptr == NULL)
    strcat(newfilename, newext);
  else
    strcpy(dotptr, newext);
  return newfilename;
}

/* Read a string from a Byfl file.  The caller should free() the result. */
char *read_byfl_string (const uint8_t **bptr)
{
  char *str;       /* String read from the file */
  uint16_t len;    /* Length of the above (excluding the NULL byte) */

  /* Read a 16-bit big-endian string length. */
  len = ((*bptr)[0] << 8) | (*bptr)[1];
  (*bptr) += 2;

  /* Read the string itself. */
  str = malloc(len + 1);
  if (str == NULL)
    abort_on_bad_syscall("malloc");
  memcpy(str, *bptr, len);
  (*bptr) += len;
  str[len] = '\0';
  return str;
}

/* Read a big-endian unsigned 64-bit integer from a Byfl file. */
uint64_t read_byfl_uint64 (const uint8_t **orig_bptr)
{
  uint64_t value = 0;    /* Value to return */
  int shift = 56;        /* Shift for current byte */
  int i;                 /* Byte index */
  const uint8_t *bptr = *orig_bptr;   /* Pointer to next byte to read */

  for (i = 0; i < 8; i++, shift -= 8)
    value |= (*bptr++) << shift;
  *orig_bptr = bptr;
  return value;
}

/* Create a NetCDF column of data -- more specifically, a 1-D variable
 * with unlimited extent and with both chunking and deflating
 * specified. */
void create_netcdf_column (int groupid, const char *colname, int datatype,
                           size_t chunksize, int *varid_p)
{
  int dimid;                /* NetCDF ID of the current dimension */
  int ncretval;             /* Return value from a NetCDF call */

  if ((ncretval = nc_def_dim(groupid, colname, NC_UNLIMITED, &dimid)))
    abort_on_bad_netcdf("nc_def_dim", ncretval);
  if ((ncretval = nc_def_var(groupid, colname, datatype, 1, &dimid, varid_p)))
    abort_on_bad_netcdf("nc_def_var", ncretval);
  if ((ncretval = nc_def_var_chunking(groupid, *varid_p, NC_CHUNKED, &chunksize)))
    abort_on_bad_netcdf("nc_def_var_chunking", ncretval);
  if ((ncretval = nc_def_var_deflate(groupid, *varid_p, 1, 1, 9)))
    abort_on_bad_netcdf("nc_def_var_deflate", ncretval);
}

/* Convert a basic Byfl table to NetCDF format. */
void convert_basic_table (const uint8_t **orig_bptr, int ncid)
{
  const uint8_t *bptr = *orig_bptr;       /* Pointer into the Byfl file */
  const char *tablename;                  /* Name of the table */
  static BINOUT_COL_T *coltypes = NULL;   /* Byfl type of each column */
  static int *varids = NULL;              /* NetCDF variable IDs, one per column */
  static size_t cols_alloced = 0;         /* Entries allocated for coltypes */
  size_t ncols = 0;                       /* Valid entries in coltypes */
  int groupid;                            /* ID of a NetCDF group */
  size_t row;                             /* Current row number */
  size_t chunksize;                       /* Size of each chunk to write */
  long pagesize;                          /* Size of an OS page */
  int ncretval;                           /* Return value from a NetCDF call */

  /* Determine the OS page size. */
  pagesize = sysconf(_SC_PAGESIZE);
  if (pagesize == -1)
    abort_on_bad_syscall("sysconf");

  /* Read the table name and use it as the name of a NetCDF group. */
  tablename = read_byfl_string(&bptr);
  if ((ncretval = nc_def_grp(ncid, tablename, &groupid)))
    abort_on_bad_netcdf("nc_def_grp", ncretval);

  /* Read a list of columns and convert each one to a NetCDF 1-D variable. */
  while (*bptr != BINOUT_COL_NONE) {
    const char *colname;      /* Name of the current column */
    int dimid;                /* NetCDF ID of the current dimension */

    /* Allocate more memory for column types if necessary. */
    if (ncols == cols_alloced) {
      cols_alloced = cols_alloced == 0 ? 1 : cols_alloced*2;
      coltypes = (BINOUT_COL_T *) realloc(coltypes, cols_alloced*sizeof(BINOUT_COL_T));
      if (coltypes == NULL)
        abort_on_bad_syscall("realloc");
      varids = (int *) realloc(varids, cols_alloced*sizeof(int));
      if (varids == NULL)
        abort_on_bad_syscall("realloc");
    }

    /* Store the current Byfl type, and create a NetCDF 1-D variable
     * of unlimited extent. */
    coltypes[ncols++] = *bptr++;
    colname = read_byfl_string(&bptr);
    switch (coltypes[ncols - 1]) {
      case BINOUT_COL_UINT64:
        create_netcdf_column(groupid, colname, NC_UINT64,
                             pagesize/sizeof(unsigned long long), &varids[ncols - 1]);
        break;

      case BINOUT_COL_STRING:
        /* We arbitrarily choose 32 as a typical string length. */
        create_netcdf_column(groupid, colname, NC_STRING, pagesize/32, &varids[ncols - 1]);
        break;

      default:
        abort_on_bad_other("Unexpected column type %d encountered", coltypes[ncols - 1]);
        break;
    }
    free((void *)colname);
  }

  /* Read a list of rows and write each value to its associated NetCDF
   * variable. */
  for (row = 0, bptr++; *bptr++ == BINOUT_ROW_DATA; row++) {
    size_t col;          /* Current column number */
    unsigned long long u64val;     /* A 64-bit unsigned integer read from the Byfl file */
    const char *strval;            /* A string read from the Byfl file */

    for (col = 0; col < ncols; col++) {
      switch (coltypes[col]) {
        case BINOUT_COL_UINT64:
          u64val = (unsigned long long) read_byfl_uint64(&bptr);
          if ((ncretval = nc_put_var1_ulonglong(groupid, varids[col], &row, &u64val)))
            abort_on_bad_netcdf("nc_put_var1_ulonglong", ncretval);
          break;

        case BINOUT_COL_STRING:
          strval = read_byfl_string(&bptr);
          if ((ncretval = nc_put_var1_string(groupid, varids[col], &row, &strval)))
            abort_on_bad_netcdf("nc_put_var1_string", ncretval);

          break;

        default:
          abort_on_bad_other("Internal error in %s, line %d", __FILE__, __LINE__);
          break;
      }
    }
  }

  /* Return the updated file pointer. */
  free((void *)tablename);
  *orig_bptr = bptr;
}

/* Convert a Byfl binary output file to a NetCDF file.  This function
 * aborts on error. */
void convert_byfl_to_netcdf (const char *byflfilename, const char *ncfilename)
{
  mapped_file_t *byfl;     /* Mapped Byfl file */
  const uint8_t *bptr;     /* Pointer into memory-mapped Byfl data */
  int ncretval;            /* NetCDF function return value */
  int ncid;                /* ID representing the entire NetCDF file */

  /* Open both the input and the output file. */
  byfl = map_file_into_memory(byflfilename);
  if ((ncretval = nc_create(ncfilename, NC_NETCDF4, &ncid)))
    abort_on_bad_netcdf("nc_create", ncretval);
  bptr = (const uint8_t *)byfl->ptr;
  if (memcmp(bptr, "BYFLBIN", 7) != 0)
    abort_on_bad_other("File %s does not appear to contain Byfl binary output", byflfilename);
  bptr += 7;

  /* Process in turn each table we encounter. */
  while (*bptr != BINOUT_TABLE_NONE)
    switch (*bptr++) {
      case BINOUT_TABLE_BASIC:
        convert_basic_table(&bptr, ncid);
        break;

      default:
        abort_on_bad_other("Unexpected table type %d found in %s", *(bptr-1), byflfilename);
        break;
    }

  /* Close the input and output files and return. */
  if ((ncretval = nc_close(ncid)))
    abort_on_bad_netcdf("nc_close", ncretval);
  unmap_file_from_memory(byfl);
}

int main (int argc, const char *argv[])
{
  const char *byflfilename;      /* Name of input file */
  char *ncfilename;              /* Name of output file */

  /* Parse the command line. */
  progname = argv[0];
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "Usage: %s <input.byfl> [<output.nc>]\n", progname);
    exit(1);
  }
  byflfilename = argv[1];
  if (argc > 2)
    ncfilename = strdup(argv[2]);
  else
    ncfilename = replace_extension(byflfilename, ".nc");

  /* Convert the file format. */
  convert_byfl_to_netcdf(byflfilename, ncfilename);

  /* Clean up and return. */
  free(ncfilename);
  return 0;
}
