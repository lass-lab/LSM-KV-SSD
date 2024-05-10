#ifndef SUPER_H_
#define SUPER_H_

#define VALUE_LOG_BASE_LBA 0x00300000

#define MAX_LEVEL 4
#define MAX_SSTABLE_LEVEL0 4
#define MAX_SSTABLE_LEVEL1 (16)
#define MAX_SSTABLE_LEVEL2 (128)
#define MAX_SSTABLE_LEVEL3 (65537)

static unsigned int value_log_lba = VALUE_LOG_BASE_LBA;
static unsigned int sst_log_lpn = 0;

typedef struct _SUPER_LEVEL_INFO {
	unsigned int level_count[MAX_LEVEL];
} SUPER_LEVEL_INFO;

typedef struct _SUPER_SSTABLE_LIST {
	unsigned int head;
	unsigned int tail;
} SUPER_SSTABLE_LIST;

typedef struct _SUPER_SSTABLE_INFO {
    unsigned int level;
    unsigned int bloomfilter_size;
    unsigned int index_size;
    unsigned int data_size;
    unsigned int total_entry;
    unsigned int min_key;
    unsigned int max_key;
    unsigned int head_lpn;
    unsigned int tail_lpn;
	unsigned int prev;
	unsigned int next;
	// unsigned int ref;
} SUPER_SSTABLE_INFO;

#endif