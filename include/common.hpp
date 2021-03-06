#ifndef COMMON_HEADER
#define COMMON_HEADER
#include <stdint.h>
#include <time.h>
#include <vector>

typedef int nrfs;
typedef char* nrfsFile;

#define MAX_MESSAGE_BLOCK_COUNT 10      /* Max count of block index in a message. */

typedef struct 
{
	uint16_t node_id;
	uint64_t offset;
	uint64_t size;
} file_pos_tuple;

struct file_pos_info
{
	uint32_t len;
	file_pos_tuple tuple[MAX_MESSAGE_BLOCK_COUNT];
};

/* getattr */
struct nrfsfileattr
{
	uint32_t mode;	/* 0 - file, 1 - directory */
	uint64_t size;
	uint32_t time;
};



/** Definitions. **/
#define MAX_PATH_LENGTH 255             /* Max length of path. */

/** Definitions. **/
#define MAX_FILE_EXTENT_COUNT 512        /* Max extent count in meta of a file. */
#define BLOCK_SIZE (16 * 1024 * 1024)    /* Current block size in bytes. */
#define MAX_FILE_NAME_LENGTH 50         /* Max file name length. */
#define MAX_DIRECTORY_COUNT 60         /* Max directory count. */

/** Classes and structures. **/
typedef uint64_t NodeHash;              /* Node hash. */

typedef struct 
{
    NodeHash hashNode; /* Node hash array of extent. */
    uint32_t indexExtentStartBlock; /* Index array of start block in an extent. */
    uint32_t countExtentBlock; /* Count array of blocks in an extent. */
    bool isDirty;
    uint64_t StorageAddress;
} FileMetaTuple;

typedef struct
{
    NodeHash hashNode;
    uint32_t indexExtentStartBlock;
    uint32_t BlockID;
} FileBlockTuple;


typedef struct {
    uint32_t BlockID;
    uint16_t nodeID;
    uint16_t tier;
    uint32_t indexCache;
    uint32_t indexMem;
    uint64_t StorageAddress;
    bool isDirty;
    bool present; //whether a block is presented in RDMA region;
} BlockInfo;


typedef struct                          /* File meta structure. */
{
    char name[MAX_FILE_NAME_LENGTH];
    time_t timeLastModified;        /* Last modified time. */
    uint64_t count;                 /* Count of extents. (not required and might have consistency problem with size) */
    uint64_t size;                  /* Size of extents. */
    bool isNewFile;                 /* Whether the file is newly created or dirty */
    uint32_t tier;                  /* The storage tier the file resides*/
    bool hasNextChunk;              /* Flag for large files since each FileMeta object contains MAX_FILE_EXTENT_COUNT blocks*/
    uint64_t indexOfNextChunk;      /* Index of Next chunk*/
    BlockInfo BlockList[MAX_FILE_EXTENT_COUNT];  /*Block metadata*/
} FileMeta;

typedef struct {
	char names[MAX_FILE_NAME_LENGTH];
	bool isDirectories;
} DirectoryMetaTuple;

typedef struct                          /* Directory meta structure. */
{
    uint64_t count;                 /* Count of names. */
    DirectoryMetaTuple tuple[MAX_DIRECTORY_COUNT];
} DirectoryMeta;

typedef DirectoryMeta nrfsfilelist;


static inline void NanosecondSleep(struct timespec *preTime, uint64_t diff) {
	struct timespec now;
	uint64_t temp;
	temp = 0;
	while (temp < diff) {
		clock_gettime(CLOCK_MONOTONIC, &now);
		temp = (now.tv_sec - preTime->tv_sec) * 1000000000 + now.tv_nsec - preTime->tv_nsec;
		temp = temp / 1000;
	}
}

#endif
