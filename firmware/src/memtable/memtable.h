#ifndef MEMTABLE_H_
#define MEMTABLE_H_

#define MAX_SKIPLIST_LEVEL 10
#define MAX_SKIPLIST_NODE (4096)
#define SKIPLIST_NIL 0

typedef struct _SKIPLIST_NODE {
    unsigned int allocated;
    // unsigned int dirty;
    // aux
    unsigned int key;
    unsigned int lba;
    // unsigned int offset;
    unsigned int length;
    struct _SKIPLIST_NODE* forward[MAX_SKIPLIST_LEVEL];
} SKIPLIST_NODE;

typedef struct _SKIPLIST_HEAD {
    unsigned int count;
    unsigned int st_lba;
    unsigned int total_value_size;
    SKIPLIST_NODE* forward[MAX_SKIPLIST_LEVEL];
} SKIPLIST_HEAD;

SKIPLIST_NODE* allocateSkiplistNode(SKIPLIST_HEAD* head, int level);

int getLevel();
void initSkipList(SKIPLIST_HEAD* head);
SKIPLIST_NODE* skiplist_insert(SKIPLIST_HEAD* head, unsigned int key, int lpn, int length);
SKIPLIST_NODE* skiplist_search(SKIPLIST_HEAD* head, unsigned int key);
SKIPLIST_NODE* skiplist_remove(SKIPLIST_HEAD* head, unsigned int key);

#endif /* MEMTABLE_H_ */
