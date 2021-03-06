/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by The HDF Group.                                               *
 * Copyright by the Board of Trustees of the University of Illinois.         *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of HDF5.  The full HDF5 copyright notice, including     *
 * terms governing use, modification, and redistribution, is contained in    *
 * the files COPYING and Copyright.html.  COPYING can be found at the root   *
 * of the source code distribution tree; Copyright.html can be found at the  *
 * root level of an installed copy of the electronic HDF5 document set and   *
 * is linked from the top-level documents page.  It can also be found at     *
 * http://hdfgroup.org/HDF5/doc/Copyright.html.  If you do not have          *
 * access to either file, you may request a copy from help@hdfgroup.org.     *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* Programmer:  Peng Cheng <peng.cheng@nscc-gz.cn>
 *              Wednesday, October 30, 2019
 *
 * Purpose: The C MEMFS virtual file driver which only uses calls from nrfs.h.
 *
 */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "memfsWrapper.h"
#include "hdf5.h"


#ifdef H5_HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef H5_HAVE_WIN32_API
/* The following two defines must be before any windows headers are included */
#define WIN32_LEAN_AND_MEAN    /* Exclude rarely-used stuff from Windows headers */
#define NOGDI                  /* Exclude Graphic Display Interface macros */

#include <windows.h>
#include <io.h>

/* This is not defined in the Windows header files */
#ifndef F_OK
#define F_OK 00
#endif

#endif

#ifdef MAX
#undef MAX
#endif /* MAX */
#define MAX(X,Y)  ((X)>(Y)?(X):(Y))

/* The driver identification number, initialized at runtime */
static hid_t H5FD_MEMFS_g = 0;

/* The maximum number of bytes which can be written in a single I/O operation */
static size_t H5_MEMFS_MAX_IO_BYTES_g = (size_t)-1;

nrfs fs;

/* File operations */
typedef enum {
    H5FD_MEMFS_OP_UNKNOWN=0,
    H5FD_MEMFS_OP_READ=1,
    H5FD_MEMFS_OP_WRITE=2,
    H5FD_MEMFS_OP_SEEK=3
} H5FD_memfs_file_op;

/* The description of a file belonging to this driver. The 'eoa' and 'eof'
 * determine the amount of hdf5 address space in use and the high-water mark
 * of the file (the current size of the underlying Unix file). The 'pos'
 * value is used to eliminate file position updates when they would be a
 * no-op. Unfortunately we've found systems that use separate file position
 * indicators for reading and writing so the lseek can only be eliminated if
 * the current operation is the same as the previous operation.  When opening
 * a file the 'eof' will be set to the current file size, 'eoa' will be set
 * to zero, 'pos' will be set to H5F_ADDR_UNDEF (as it is when an error
 * occurs), and 'op' will be set to H5F_OP_UNKNOWN.
 */
typedef struct H5FD_memfs_t {
    H5FD_t      pub;            /* public stuff, must be first      */
    //FILE        *fp;            /* the file handle                */
    nrfsFile    filePath;           /*The file path                 */
    long        offset;          /*offset of read opration          */
    //int         fd;             /* file descriptor (for truncate)   */
    haddr_t     eoa;            /* end of allocated region          */
    haddr_t     eof;            /* end of file; current file size   */
    haddr_t     pos;            /* current file I/O position        */
    unsigned    write_access;   /* Flag to indicate the file was opened with write access */
    H5FD_memfs_file_op op;  /* last operation */
#ifndef H5_HAVE_WIN32_API
    /* On most systems the combination of device and i-node number uniquely
     * identify a file.  Note that Cygwin, MinGW and other Windows POSIX
     * environments have the stat function (which fakes inodes)
     * and will use the 'device + inodes' scheme as opposed to the
     * Windows code further below.
     */
    dev_t           device;     /* file device number   */
    ino_t           inode;      /* file i-node number   */
#else
    /* Files in windows are uniquely identified by the volume serial
     * number and the file index (both low and high parts).
     *
     * There are caveats where these numbers can change, especially
     * on FAT file systems.  On NTFS, however, a file should keep
     * those numbers the same until renamed or deleted (though you
     * can use ReplaceFile() on NTFS to keep the numbers the same
     * while renaming).
     *
     * See the MSDN "BY_HANDLE_FILE_INFORMATION Structure" entry for
     * more information.
     *
     * http://msdn.microsoft.com/en-us/library/aa363788(v=VS.85).aspx
     */
    DWORD           nFileIndexLow;
    DWORD           nFileIndexHigh;
    DWORD           dwVolumeSerialNumber;
    
    HANDLE          hFile;      /* Native windows file handle */
#endif  /* H5_HAVE_WIN32_API */
} H5FD_memfs_t;

/* Use similar structure as in H5private.h by defining Windows stuff first. */
#ifdef H5_HAVE_WIN32_API
#ifndef H5_HAVE_MINGW
    #define file_fseek      _fseeki64
    #define file_offset_t   __int64
    #define file_ftruncate  _chsize_s   /* Supported in VS 2005 or newer */
    #define file_ftell      _ftelli64
#endif /* H5_HAVE_MINGW */
#endif /* H5_HAVE_WIN32_API */

/* If these functions weren't re-defined for Windows, give them
 * more platform-independent names.
 */
#ifndef file_fseek
    #define file_fseek      fseeko
    #define file_offset_t   off_t
    #define file_ftruncate  ftruncate
    #define file_ftell      ftello
#endif /* file_fseek */

/* These macros check for overflow of various quantities.  These macros
 * assume that file_offset_t is signed and haddr_t and size_t are unsigned.
 *
 * ADDR_OVERFLOW:  Checks whether a file address of type `haddr_t'
 *      is too large to be represented by the second argument
 *      of the file seek function.
 *
 * SIZE_OVERFLOW:  Checks whether a buffer size of type `hsize_t' is too
 *      large to be represented by the `size_t' type.
 *
 * REGION_OVERFLOW:  Checks whether an address and size pair describe data
 *      which can be addressed entirely by the second
 *      argument of the file seek function.
 */
/* adding for windows NT filesystem support. */
#define MAXADDR (((haddr_t)1<<(8*sizeof(file_offset_t)-1))-1)
#define ADDR_OVERFLOW(A)  (HADDR_UNDEF==(A) || ((A) & ~(haddr_t)MAXADDR))
#define SIZE_OVERFLOW(Z)  ((Z) & ~(hsize_t)MAXADDR)
#define REGION_OVERFLOW(A,Z)  (ADDR_OVERFLOW(A) || SIZE_OVERFLOW(Z) || \
    HADDR_UNDEF==(A)+(Z) || (file_offset_t)((A)+(Z))<(file_offset_t)(A))

/* Prototypes */
static H5FD_t *H5FD_memfs_open(const char *name, unsigned flags,
                 hid_t fapl_id, haddr_t maxaddr);
static herr_t H5FD_memfs_close(H5FD_t *lf);
static int H5FD_memfs_cmp(const H5FD_t *_f1, const H5FD_t *_f2);
static herr_t H5FD_memfs_query(const H5FD_t *_f1, unsigned long *flags);
static haddr_t H5FD_memfs_alloc(H5FD_t *_file, H5FD_mem_t type, hid_t dxpl_id, hsize_t size);
static haddr_t H5FD_memfs_get_eoa(const H5FD_t *_file, H5FD_mem_t type);
static herr_t H5FD_memfs_set_eoa(H5FD_t *_file, H5FD_mem_t type, haddr_t addr);
static haddr_t H5FD_memfs_get_eof(const H5FD_t *_file);
static herr_t  H5FD_memfs_get_handle(H5FD_t *_file, hid_t fapl, void** file_handle);
static herr_t H5FD_memfs_read(H5FD_t *lf, H5FD_mem_t type, hid_t fapl_id, haddr_t addr,
                size_t size, void *buf);
static herr_t H5FD_memfs_write(H5FD_t *lf, H5FD_mem_t type, hid_t fapl_id, haddr_t addr,
                size_t size, const void *buf);
static herr_t H5FD_memfs_flush(H5FD_t *_file, hid_t dxpl_id, unsigned closing);
static herr_t H5FD_memfs_truncate(H5FD_t *_file, hid_t dxpl_id, hbool_t closing);

static const H5FD_class_t H5FD_memfs_g = {
    "memfs",                    /* name         */
    MAXADDR,                    /* maxaddr      */
    H5F_CLOSE_WEAK,             /* fc_degree    */
    NULL,                       /* sb_size      */
    NULL,                       /* sb_encode    */
    NULL,                       /* sb_decode    */
    0,                          /* fapl_size    */
    NULL,                       /* fapl_get     */
    NULL,                       /* fapl_copy    */
    NULL,                       /* fapl_free    */
    0,                          /* dxpl_size    */
    NULL,                       /* dxpl_copy    */
    NULL,                       /* dxpl_free    */
    H5FD_memfs_open,            /* open         */
    H5FD_memfs_close,           /* close        */
    H5FD_memfs_cmp,             /* cmp          */
    H5FD_memfs_query,           /* query        */
    NULL,                       /* get_type_map */
    H5FD_memfs_alloc,           /* alloc        */
    NULL,                       /* free         */
    H5FD_memfs_get_eoa,         /* get_eoa      */
    H5FD_memfs_set_eoa,         /* set_eoa      */
    H5FD_memfs_get_eof,         /* get_eof      */
    H5FD_memfs_get_handle,      /* get_handle   */
    H5FD_memfs_read,            /* read         */
    H5FD_memfs_write,           /* write        */
    H5FD_memfs_flush,           /* flush        */
    H5FD_memfs_truncate,        /* truncate     */
    NULL,                       /* lock         */
    NULL,                       /* unlock       */
    H5FD_FLMAP_DICHOTOMY	/* fl_map       */
};


/*-------------------------------------------------------------------------
 * Function:  H5FD_memfs_init
 *
 * Purpose:  Initialize this driver by registering the driver with the
 *    library.
 *
 * Return:  Success:  The driver ID for the memfs driver.
 *
 *    Failure:  Negative.
 *
 * Programmer:  Peng Cheng
 *
 *-------------------------------------------------------------------------
 */
hid_t
H5FD_memfs_init(void)
{
    printf("Debug, H5FD_memfs_init\n");

    /* Clear the error stack */
    H5Eclear2(H5E_DEFAULT);

    if (H5I_VFL!=H5Iget_type(H5FD_MEMFS_g))
        H5FD_MEMFS_g = H5FDregister(&H5FD_memfs_g);

    fs = nrfsConnect_Wrapper("default", 0, 0);
    return H5FD_MEMFS_g;
} /* end H5FD_memfs_init() */


/*---------------------------------------------------------------------------
 * Function:  H5FD_memfs_term
 *
 * Purpose:  Shut down the VFD
 *
 * Returns:     None
 *
 * Programmer:  Peng Cheng
 *
 *---------------------------------------------------------------------------
 */
void
H5FD_memfs_term(void)
{
    printf("Debug, H5FD_memfs_term\n");

    /* Reset VFL ID */
    H5FD_MEMFS_g = 0;

    nrfsDisconnect_Wrapper(fs);

    return;
} /* end H5FD_memfs_term() */


/*-------------------------------------------------------------------------
 * Function:  H5Pset_fapl_memfs
 *
 * Purpose:  Modify the file access property list to use the H5FD_MEMFS
 *    driver defined in this source file.  There are no driver
 *    specific properties.
 *
 * Return:  Non-negative on success/Negative on failure
 *
 * Programmer:  Robb Matzke
 *    Thursday, February 19, 1998
 *
 *-------------------------------------------------------------------------
 */
herr_t
H5Pset_fapl_memfs(hid_t fapl_id)
{
    printf("Debug, H5Pset_fapl_memfs\n");

    static const char *func = "H5FDset_fapl_memfs";  /*for error reporting*/

    /*NO TRACE*/

    /* Clear the error stack */
    H5Eclear2(H5E_DEFAULT);

    if(0 == H5Pisa_class(fapl_id, H5P_FILE_ACCESS))
        H5Epush_ret(func, H5E_ERR_CLS, H5E_PLIST, H5E_BADTYPE, "not a file access property list", -1)

    printf("Debug, H5Pset_fapl_memfs: done\n");

    return H5Pset_driver(fapl_id, H5FD_MEMFS, NULL);

} /* end H5Pset_fapl_memfs() */


/*-------------------------------------------------------------------------
 * Function:  H5FD_memfs_open
 *
 * Purpose:  Create and/or opens a Standard C file as an HDF5 file.
 *
 * Errors:
 *  IO  CANTOPENFILE    File doesn't exist and CREAT wasn't
 *                      specified.
 *  IO  CANTOPENFILE    fopen() failed.
 *  IO  FILEEXISTS      File exists but CREAT and EXCL were
 *                      specified.
 *
 * Return:
 *      Success:    A pointer to a new file data structure. The
 *                  public fields will be initialized by the
 *                  caller, which is always H5FD_open().
 *
 *      Failure:    NULL
 *
 * Programmer:  Peng Cheng
 *
 *-------------------------------------------------------------------------
 */
static H5FD_t *
H5FD_memfs_open( const char *name, unsigned flags, hid_t fapl_id,
    haddr_t maxaddr)
{
    printf("Debug, H5FD_memfs_open\n");
    //FILE                *f = NULL;
    nrfsFile            f = (char *)malloc(sizeof(name));
    unsigned            write_access = 0;           /* File opened with write access? */
    H5FD_memfs_t        *file = NULL;
    static const char   *func = "H5FD_memfs_open";  /* Function Name for error reporting */
#ifdef H5_HAVE_WIN32_API
    struct _BY_HANDLE_FILE_INFORMATION fileinfo;
#else /* H5_HAVE_WIN32_API */
    struct stat         sb;
#endif  /* H5_HAVE_WIN32_API */

    /* Sanity check on file offsets */
    assert(sizeof(file_offset_t) >= sizeof(size_t));

    /* Quiet compiler */
    fapl_id = fapl_id;

    /* Clear the error stack */
    H5Eclear2(H5E_DEFAULT);

    /* Check arguments */
    if (!name || !*name)
        H5Epush_ret(func, H5E_ERR_CLS, H5E_ARGS, H5E_BADVALUE, "invalid file name", NULL)
    if (0 == maxaddr || HADDR_UNDEF == maxaddr)
        H5Epush_ret(func, H5E_ERR_CLS, H5E_ARGS, H5E_BADRANGE, "bogus maxaddr", NULL)
    if (ADDR_OVERFLOW(maxaddr))
        H5Epush_ret(func, H5E_ERR_CLS, H5E_ARGS, H5E_OVERFLOW, "maxaddr too large", NULL)

    /* Tentatively open file in read-only mode, to check for existence */
    //f = nrfsOpenFile_Wrapper(fs, name, O_CREAT);
    //write_access = 1;
  
    if(flags & H5F_ACC_RDWR)
        f = nrfsOpenFile_Wrapper(fs, name, O_RDWR);
    else
        f = nrfsOpenFile_Wrapper(fs, name, O_RDONLY);

    printf("Debug, H5FD_memfs_open: open file once\n");

    if(f == NULL) {
        // File doesn't exist 
        if(flags & H5F_ACC_CREAT) {
            assert(flags & H5F_ACC_RDWR);
            f = nrfsOpenFile_Wrapper(fs, name, O_CREAT);
            write_access = 1;     // Note the write access 
        }
        else
            H5Epush_ret(func, H5E_ERR_CLS, H5E_IO, H5E_CANTOPENFILE, "file doesn't exist and CREAT wasn't specified", NULL)
    } else if(flags & H5F_ACC_EXCL) {
        // File exists, but EXCL is passed.  Fail.
        assert(flags & H5F_ACC_CREAT);
        nrfsCloseFile_Wrapper(fs, f);
        H5Epush_ret(func, H5E_ERR_CLS, H5E_IO, H5E_FILEEXISTS, "file exists but CREAT and EXCL were specified", NULL)
    } else if(flags & H5F_ACC_RDWR) {
        if(flags & H5F_ACC_TRUNC)
            f = nrfsOpenFile_Wrapper(fs, name, O_CREAT);//f = freopen(name, "wb+", f);
        write_access = 1;     // Note the write access 
    } // end if 
    // Note there is no need to reopen if neither TRUNC nor EXCL are specified,
    //  as the tentative open will work
    

    if(!f)
        H5Epush_ret(func, H5E_ERR_CLS, H5E_IO, H5E_CANTOPENFILE, "fopen failed", NULL)

    /* Build the return value */
    if(NULL == (file = (H5FD_memfs_t *)calloc((size_t)1, sizeof(H5FD_memfs_t)))) {
        nrfsCloseFile_Wrapper(fs, f);
        H5Epush_ret(func, H5E_ERR_CLS, H5E_RESOURCE, H5E_NOSPACE, "memory allocation failed", NULL)
    } /* end if */

    file->filePath = f;
    file->op = H5FD_MEMFS_OP_SEEK;
    file->pos = HADDR_UNDEF;
    file->write_access = write_access;    /* Note the write_access for later */
    
    file_offset_t x = (file_offset_t) nrfsGetFileSize_Wrapper(fs, file->filePath);
    if (x == -1) {
      file->op = H5FD_MEMFS_OP_UNKNOWN;
    }
    assert (x >= 0);
    file->eof = (haddr_t)x;

    printf("Debug, H5FD_memfs_open:  init file structure. file_offset_t is %ld\n", (long)x);
#ifdef H5_HAVE_WIN32_API
    H5Epush_ret(func, H5E_ERR_CLS, H5E_FILE, H5E_CANTOPENFILE, "unable to get Windows file handle", NULL);
    /*
    file->hFile = (HANDLE)_get_osfhandle(file->fd);
    if(INVALID_HANDLE_VALUE == file->hFile) {
        free(file);
        fclose(f);
        H5Epush_ret(func, H5E_ERR_CLS, H5E_FILE, H5E_CANTOPENFILE, "unable to get Windows file handle", NULL);
    } // end if 

    if(!GetFileInformationByHandle((HANDLE)file->hFile, &fileinfo)) {
        free(file);
        fclose(f);
        H5Epush_ret(func, H5E_ERR_CLS, H5E_FILE, H5E_CANTOPENFILE, "unable to get Windows file descriptor information", NULL);
    } // end if 

    file->nFileIndexHigh = fileinfo.nFileIndexHigh;
    file->nFileIndexLow = fileinfo.nFileIndexLow;
    file->dwVolumeSerialNumber = fileinfo.dwVolumeSerialNumber;
    */
#else /* H5_HAVE_WIN32_API */
    if(nrfsAccess_Wrapper(fs, file->filePath) != 1) {
        free(file);
        nrfsCloseFile_Wrapper(fs, f);
        H5Epush_ret(func, H5E_ERR_CLS, H5E_FILE, H5E_BADFILE, "unable to fstat file", NULL)
    } /* end if */
    file->device = sb.st_dev;
    file->inode = sb.st_ino;
#endif /* H5_HAVE_WIN32_API */
    printf("Debug, H5FD_memfs_open:  open success\n");
    return (H5FD_t*)file;
} /* end H5FD_memfs_open() */


/*-------------------------------------------------------------------------
 * Function:  H5F_memfs_close
 *
 * Purpose:  Closes a file.
 *
 * Errors:
 *    IO    CLOSEERROR  Fclose failed.
 *
 * Return:  Non-negative on success/Negative on failure
 *
 * Programmer:  Peng Chnrg
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5FD_memfs_close(H5FD_t *_file)
{
    printf("Debug, H5FD_memfs_close\n");

    H5FD_memfs_t  *file = (H5FD_memfs_t*)_file;
    static const char *func = "H5FD_memfs_close";  /* Function Name for error reporting */

    /* Clear the error stack */
    H5Eclear2(H5E_DEFAULT);

    if (nrfsCloseFile_Wrapper(fs, file->filePath) < 0)
        H5Epush_ret(func, H5E_ERR_CLS, H5E_IO, H5E_CLOSEERROR, "fclose failed", -1)

    free(file);

    printf("Debug, H5FD_memfs_close:  done\n");

    return 0;
} /* end H5FD_memfs_close() */


/*-------------------------------------------------------------------------
 * Function:  H5FD_memfs_cmp
 *
 * Purpose:  Compares two files belonging to this driver using an
 *    arbitrary (but consistent) ordering.
 *
 * Return:
 *      Success:    A value like strcmp()
 *
 *      Failure:    never fails (arguments were checked by the caller).
 *
 * Programmer:  Peng Cheng
 *
 *-------------------------------------------------------------------------
 */
static int
H5FD_memfs_cmp(const H5FD_t *_f1, const H5FD_t *_f2)
{
    printf("Debug, H5FD_memfs_cmp\n");

    const H5FD_memfs_t  *f1 = (const H5FD_memfs_t*)_f1;
    const H5FD_memfs_t  *f2 = (const H5FD_memfs_t*)_f2;

    /* Clear the error stack */
    H5Eclear2(H5E_DEFAULT);

    printf("Debug, H5FD_memfs_cmp: done\n");

    return -1;
} /* H5FD_memfs_cmp() */


/*-------------------------------------------------------------------------
 * Function:  H5FD_memfs_query
 *
 * Purpose:  Set the flags that this VFL driver is capable of supporting.
 *              (listed in H5FDpublic.h)
 *
 * Return:  Success:  non-negative
 *
 *    Failure:  negative
 *
 * Programmer:  Peng Cheng
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5FD_memfs_query(const H5FD_t *_f, unsigned long *flags /* out */)
{

    printf("Debug, H5FD_memfs_query \n");
    /* Quiet the compiler */
    _f=_f;

    /* Set the VFL feature flags that this driver supports */
    if(flags) {
        *flags = 0;
        *flags|=H5FD_FEAT_AGGREGATE_METADATA; /* OK to aggregate metadata allocations */
        *flags|=H5FD_FEAT_ACCUMULATE_METADATA; /* OK to accumulate metadata for faster writes */
        *flags|=H5FD_FEAT_DATA_SIEVE;       /* OK to perform data sieving for faster raw data reads & writes */
        *flags|=H5FD_FEAT_AGGREGATE_SMALLDATA; /* OK to aggregate "small" raw data allocations */
    }

    printf("Debug, H5FD_memfs_query: done\n");

    return 0;
} /* end H5FD_memfs_query() */


/*-------------------------------------------------------------------------
 * Function:  H5FD_memfs_alloc
 *
 * Purpose:     Allocates file memory. If fseeko isn't available, makes
 *              sure the file size isn't bigger than 2GB because the
 *              parameter OFFSET of fseek is of the type LONG INT, limiting
 *              the file size to 2GB.
 *
 * Return:
 *      Success:    Address of new memory
 *
 *      Failure:    HADDR_UNDEF
 *
 * Programmer:  Peng Cheng
 *
 *-------------------------------------------------------------------------
 */
static haddr_t
H5FD_memfs_alloc(H5FD_t *_file, H5FD_mem_t /*H5_ATTR_UNUSED*/ type, hid_t /*H5_ATTR_UNUSED*/ dxpl_id, hsize_t size)
{
    printf("Debug, H5FD_memfs_alloc\n");

    H5FD_memfs_t    *file = (H5FD_memfs_t*)_file;
    haddr_t         addr;

    /* Quiet compiler */
    type = type;
    dxpl_id = dxpl_id;

    /* Clear the error stack */
    H5Eclear2(H5E_DEFAULT);

    /* Compute the address for the block to allocate */
    addr = file->eoa;

    /* Check if we need to align this block */
    if(size >= file->pub.threshold) {
        /* Check for an already aligned block */
        if((addr % file->pub.alignment) != 0)
            addr = ((addr / file->pub.alignment) + 1) * file->pub.alignment;
    } /* end if */

    file->eoa = addr + size;

    printf("Debug, H5FD_memfs_alloc: done\n");

    return addr;
} /* end H5FD_memfs_alloc() */


/*-------------------------------------------------------------------------
 * Function:  H5FD_memfs_get_eoa
 *
 * Purpose:  Gets the end-of-address marker for the file. The EOA marker
 *           is the first address past the last byte allocated in the
 *           format address space.
 *
 * Return:  Success:  The end-of-address marker.
 *
 *    Failure:  HADDR_UNDEF
 *
 * Programmer:  Robb Matzke
 *              Monday, August  2, 1999
 *
 *-------------------------------------------------------------------------
 */
static haddr_t
H5FD_memfs_get_eoa(const H5FD_t *_file, H5FD_mem_t /*H5_ATTR_UNUSED*/ type)
{
    printf("Debug, H5FD_memfs_get_eoa\n");

    const H5FD_memfs_t *file = (const H5FD_memfs_t *)_file;

    /* Clear the error stack */
    H5Eclear2(H5E_DEFAULT);

    /* Quiet compiler */
    type = type;

    printf("Debug, H5FD_memfs_get_eoa: done\n");

    return file->eoa;
} /* end H5FD_memfs_get_eoa() */


/*-------------------------------------------------------------------------
 * Function:  H5FD_memfs_set_eoa
 *
 * Purpose:  Set the end-of-address marker for the file. This function is
 *    called shortly after an existing HDF5 file is opened in order
 *    to tell the driver where the end of the HDF5 data is located.
 *
 * Return:  Success:  0
 *
 *    Failure:  Does not fail
 *
 * Programmer:  Robb Matzke
 *              Thursday, July 29, 1999
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5FD_memfs_set_eoa(H5FD_t *_file, H5FD_mem_t /*H5_ATTR_UNUSED*/ type, haddr_t addr)
{
    printf("Debug, H5FD_memfs_set_eoa\n");

    H5FD_memfs_t  *file = (H5FD_memfs_t*)_file;

    /* Clear the error stack */
    H5Eclear2(H5E_DEFAULT);

    /* Quiet the compiler */
    type = type;

    file->eoa = addr;

    printf("Debug, H5FD_memfs_set_eoa: done\n");

    return 0;
}


/*-------------------------------------------------------------------------
 * Function:  H5FD_memfs_get_eof
 *
 * Purpose:  Returns the end-of-file marker, which is the greater of
 *    either the Unix end-of-file or the HDF5 end-of-address
 *    markers.
 *
 * Return:  Success:  End of file address, the first address past
 *        the end of the "file", either the Unix file
 *        or the HDF5 file.
 *
 *    Failure:  HADDR_UNDEF
 *
 * Programmer:  Robb Matzke
 *              Thursday, July 29, 1999
 *
 *-------------------------------------------------------------------------
 */
static haddr_t
H5FD_memfs_get_eof(const H5FD_t *_file)
{
    printf("Debug, H5FD_memfs_get_eof\n");

    const H5FD_memfs_t  *file = (const H5FD_memfs_t *)_file;

    /* Clear the error stack */
    H5Eclear2(H5E_DEFAULT);

    printf("Debug, H5FD_memfs_get_eof: done\n");

    //FUNC_LEAVE_NOAPI(MAX(file->eof, file->eoa))

    return MAX(file->eof, file->eoa);
} /* end H5FD_memfs_get_eof() */


/*-------------------------------------------------------------------------
 * Function:       H5FD_memfs_get_handle
 *
 * Purpose:        Returns the file handle of memfs file driver.
 *
 * Returns:        Non-negative if succeed or negative if fails.
 *
 * Programmer:     Raymond Lu
 *                 Sept. 16, 2002
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5FD_memfs_get_handle(H5FD_t *_file, hid_t fapl, void** file_handle)
{
    printf("Debug, H5FD_memfs_get_handle\n");

    H5FD_memfs_t       *file = (H5FD_memfs_t *)_file;
    static const char  *func = "H5FD_memfs_get_handle";  /* Function Name for error reporting */

    /* Quiet the compiler */
    fapl = fapl;

    /* Clear the error stack */
    H5Eclear2(H5E_DEFAULT);

    *file_handle = &(file->filePath);
    if(*file_handle == NULL)
        H5Epush_ret(func, H5E_ERR_CLS, H5E_IO, H5E_WRITEERROR, "get handle failed", -1)

    printf("Debug, H5FD_memfs_get_handle: done\n");

    return 0;
} /* end H5FD_memfs_get_handle() */


/*-------------------------------------------------------------------------
 * Function:  H5FD_memfs_read
 *
 * Purpose:  Reads SIZE bytes beginning at address ADDR in file LF and
 *    places them in buffer BUF.  Reading past the logical or
 *    physical end of file returns zeros instead of failing.
 *
 * Errors:
 *    IO    READERROR  fread failed.
 *    IO    SEEKERROR  fseek failed.
 *
 * Return:  Non-negative on success/Negative on failure
 *
 * Programmer:  Peng Cheng
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5FD_memfs_read(H5FD_t *_file, H5FD_mem_t type, hid_t dxpl_id, haddr_t addr, size_t size,
    void *buf/*out*/)
{
    printf("Debug, H5FD_memfs_read\n");

    H5FD_memfs_t    *file = (H5FD_memfs_t*)_file;
    static const char *func = "H5FD_memfs_read";  /* Function Name for error reporting */

    /* Quiet the compiler */
    type = type;
    dxpl_id = dxpl_id;

    /* Clear the error stack */
    H5Eclear2(H5E_DEFAULT);

    /* Check for overflow */
    if (HADDR_UNDEF==addr)
        H5Epush_ret (func, H5E_ERR_CLS, H5E_IO, H5E_OVERFLOW, "file address overflowed", -1)
    if (REGION_OVERFLOW(addr, size))
        H5Epush_ret (func, H5E_ERR_CLS, H5E_IO, H5E_OVERFLOW, "file address overflowed", -1)

    /* Check easy cases */
    if (0 == size)
        return 0;
    if ((haddr_t)addr >= file->eof) {
        memset(buf, 0, size);
        return 0;
    }

    /* Seek to the correct file position. */
    if (!(file->op == H5FD_MEMFS_OP_READ || file->op == H5FD_MEMFS_OP_SEEK) ||
            file->pos != addr) {
        /*
        if (file_fseek(file->fp, (file_offset_t)addr, SEEK_SET) < 0) {
            file->op = H5FD_MEMFS_OP_UNKNOWN;
            file->pos = HADDR_UNDEF;
            H5Epush_ret(func, H5E_ERR_CLS, H5E_IO, H5E_SEEKERROR, "fseek failed", -1)
        }*/
        file->pos = addr;
    }

    /* Read zeros past the logical end of file (physical is handled below) */
    if (addr + size > file->eof) {
        size_t nbytes = (size_t) (addr + size - file->eof);
        memset((unsigned char *)buf + size - nbytes, 0, nbytes);
        size -= nbytes;
    }

    /* Read the data.  Since we're reading single-byte values, a partial read
     * will advance the file position by N.  If N is zero or an error
     * occurs then the file position is undefined.
     */
    while(size > 0) {

        size_t bytes_in        = 0;    /* # of bytes to read       */
        size_t bytes_read      = 0;    /* # of bytes actually read */
        size_t item_size       = 1;    /* size of items in bytes */

        if(size > 16 * 1024 * 1024)    /*Block size of memfs*/
            bytes_in = 16 * 1024 * 1024;
        else
            bytes_in = size;

        bytes_read = nrfsRead_Wrapper(fs, file->filePath, buf, bytes_in, file->pos);
        file->pos += bytes_read;
        if(bytes_in != bytes_read || bytes_read == -1 ) { /* error */
            file->op = H5FD_MEMFS_OP_UNKNOWN;
            file->pos = HADDR_UNDEF;
            H5Epush_ret(func, H5E_ERR_CLS, H5E_IO, H5E_READERROR, "fread failed", -1)
        } /* end if */
        
        if(0 == bytes_read && (file->pos >= file->eof)) {
            /* end of file but not end of format address space */
            memset((unsigned char *)buf, 0, size);
            break;
        } /* end if */
        
        size -= bytes_read;
        addr += (haddr_t)bytes_read;
        buf = (char *)buf + bytes_read;
    } /* end while */

    /* Update the file position data. */
    file->op = H5FD_MEMFS_OP_READ;
    file->pos = addr;

    printf("Debug, H5FD_memfs_read: done\n");

    return 0;
}


/*-------------------------------------------------------------------------
 * Function:  H5FD_memfs_write
 *
 * Purpose:  Writes SIZE bytes from the beginning of BUF into file LF at
 *    file address ADDR.
 *
 * Errors:
 *    IO    SEEKERROR   fseek failed.
 *    IO    WRITEERROR  fwrite failed.
 *
 * Return:  Non-negative on success/Negative on failure
 *
 * Programmer:  Peng Cheng
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5FD_memfs_write(H5FD_t *_file, H5FD_mem_t type, hid_t dxpl_id, haddr_t addr,
    size_t size, const void *buf)
{
    printf("Debug, H5FD_memfs_write\n");

    H5FD_memfs_t    *file = (H5FD_memfs_t*)_file;
    static const char *func = "H5FD_memfs_write";  /* Function Name for error reporting */

    /* Quiet the compiler */
    dxpl_id = dxpl_id;
    type = type;

    /* Clear the error stack */
    H5Eclear2(H5E_DEFAULT);

    /* Check for overflow conditions */
    if (HADDR_UNDEF == addr)
        H5Epush_ret (func, H5E_ERR_CLS, H5E_IO, H5E_OVERFLOW, "file address overflowed", -1)
    if (REGION_OVERFLOW(addr, size))
        H5Epush_ret (func, H5E_ERR_CLS, H5E_IO, H5E_OVERFLOW, "file address overflowed", -1)

    /* Seek to the correct file position. */
    if ((file->op != H5FD_MEMFS_OP_WRITE && file->op != H5FD_MEMFS_OP_SEEK) ||
                file->pos != addr) {
        file->pos = addr;
    }

    /* Write the buffer.  On successful return, the file position will be
     * advanced by the number of bytes read.  On failure, the file position is
     * undefined.
     */
    while(size > 0) {

        size_t bytes_in        = 0;    /* # of bytes to write  */
        size_t bytes_wrote     = 0;    /* # of bytes written   */
        size_t item_size       = 1;    /* size of items in bytes */

        if(size > 16 * 1024 * 1024)    /*Size of block in memfs*/
            bytes_in = 16 * 1024 * 1024;
        else
            bytes_in = size;

        //bytes_wrote = fwrite(buf, item_size, bytes_in, file->fp);
        bytes_wrote = nrfsWrite_Wrapper(fs, file->filePath, buf, bytes_in, file->pos);
        file->pos += bytes_wrote;
        if(bytes_wrote != bytes_in || (-1 == bytes_wrote)) { /* error */
            file->op = H5FD_MEMFS_OP_UNKNOWN;
            file->pos = HADDR_UNDEF;
            H5Epush_ret(func, H5E_ERR_CLS, H5E_IO, H5E_WRITEERROR, "fwrite failed", -1)
        } /* end if */
        
        assert(bytes_wrote > 0);
        assert((size_t)bytes_wrote <= size);

        size -= bytes_wrote;
        addr += (haddr_t)bytes_wrote;
        buf = (const char *)buf + bytes_wrote;
    }

    /* Update seek optimizing data. */
    file->op = H5FD_MEMFS_OP_WRITE;
    file->pos = addr;

    /* Update EOF if necessary */
    if (file->pos > file->eof)
        file->eof = file->pos;

    printf("Debug, H5FD_memfs_write: done\n");

    return 0;
}


/*-------------------------------------------------------------------------
 * Function:  H5FD_memfs_flush
 *
 * Purpose:  Makes sure that all data is on disk.
 *
 * Errors:
 *    IO    SEEKERROR     fseek failed.
 *    IO    WRITEERROR    fflush or fwrite failed.
 *
 * Return:  Non-negative on success/Negative on failure
 *
 * Programmer:  Robb Matzke, Peng Cheng
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5FD_memfs_flush(H5FD_t *_file, hid_t dxpl_id, unsigned closing)
{
    printf("Debug, H5FD_memfs_flush\n");

    H5FD_memfs_t  *file = (H5FD_memfs_t*)_file;
    static const char *func = "H5FD_memfs_flush";  /* Function Name for error reporting */

    /* Quiet the compiler */
    dxpl_id = dxpl_id;

    /* Clear the error stack */
    H5Eclear2(H5E_DEFAULT);

    /* Only try to flush the file if we have write access */
    if(file->write_access) {
        if(!closing) {
            /* Reset last file I/O information */
            file->pos = HADDR_UNDEF;
            file->op = H5FD_MEMFS_OP_UNKNOWN;
        } /* end if */
    } /* end if */

    printf("Debug, H5FD_memfs_flush: done\n");

    return 0;
} /* end H5FD_memfs_flush() */


/*-------------------------------------------------------------------------
 * Function:  H5FD_memfs_truncate
 *
 * Purpose:  Makes sure that the true file size is the same (or larger)
 *    than the end-of-address.
 *
 * Errors:
 *    IO    SEEKERROR     fseek failed.
 *    IO    WRITEERROR    fflush or fwrite failed.
 *
 * Return:  Non-negative on success/Negative on failure
 *
 * Programmer:  Quincey Koziol
 *    Thursday, January 31, 2008
 *
 *-------------------------------------------------------------------------
 */
static herr_t
H5FD_memfs_truncate(H5FD_t *_file, hid_t dxpl_id, hbool_t closing)
{
    printf("Debug, H5FD_memfs_truncate\n");

    H5FD_memfs_t  *file = (H5FD_memfs_t*)_file;
    static const char *func = "H5FD_memfs_truncate";  /* Function Name for error reporting */

    /* Quiet the compiler */
    dxpl_id = dxpl_id;
    closing = closing;

    /* Clear the error stack */
    H5Eclear2(H5E_DEFAULT);

    if(file->write_access) {
      if(file->eoa != file->eof) {

        printf("Debug, H5FD_memfs_truncate: file->eoa equals file->eof\n");

        file->pos = 0;

        /* Update the eof value */
        file->eof = file->eoa;

        /* Reset last file I/O information */
        file->pos = HADDR_UNDEF;
        file->op = H5FD_MEMFS_OP_UNKNOWN;

        //H5Epush_ret(func, H5E_ERR_CLS, H5E_FILE, H5E_FILEOPEN, "unable to set file pointer", -1)
      } else {
        printf("Debug, H5FD_memfs_truncate: file->eoa equals file->eof\n");
      }
    }

    printf("Debug, H5FD_memfs_truncate: done\n");
    return 0;
} /* end H5FD_memfs_truncate() */


#ifdef _H5private_H
/*
 * This is not related to the functionality of the driver code.
 * It is added here to trigger warning if HDF5 private definitions are included
 * by mistake.  The code should use only HDF5 public API and definitions.
 */
#error "Do not use HDF5 private definitions"
#endif

