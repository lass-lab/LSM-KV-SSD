#ifndef SSTABLE_H_
#define SSTABLE_H_

typedef struct _SSTABLE_INDEX_NODE {
	unsigned int key;
//	unsigned int lba;
//	unsigned int length;
} SSTABLE_INDEX_NODE;

typedef struct _SSTABLE_DATA_NODE {
//	unsigned int key;
	unsigned int lba;
	unsigned int length;
} SSTABLE_DATA_NODE;

unsigned int simple_hash(const unsigned int key, const unsigned int seed);

#endif

