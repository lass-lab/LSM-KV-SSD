#include "memtable.h"
#include "../memory_map.h"
#include "xil_printf.h"
#include <stdlib.h>

int getLevel() {
    int level = 1;
    while (((int)rand()%2) && level < MAX_SKIPLIST_LEVEL) {
        level++;
    }
    return (level - 1);
}

SKIPLIST_NODE* allocateSkiplistNode(SKIPLIST_HEAD* head, int level) {
    static SKIPLIST_NODE* allocatePtr = (SKIPLIST_NODE*)MEMTABLE_NODE_ADDR;

    SKIPLIST_NODE* allocated;
    while (allocatePtr->allocated == 1) {
        allocatePtr++;
        if((SKIPLIST_NODE*)MEMTABLE_NODE_END_ADDR <= allocatePtr) {
                allocatePtr = (SKIPLIST_NODE*)MEMTABLE_NODE_ADDR;
        }
    }
    allocated = allocatePtr;
    for(int lev = 0; lev<MAX_SKIPLIST_LEVEL;lev++){
        allocated->forward[lev] = SKIPLIST_NIL;
    }
    allocated->allocated = 1;
    // allocated->dirty = 0;

    return allocated;
}

// head = MEMTABLE_HEAD_ADDR
void initSkipList(SKIPLIST_HEAD* head) {

    head->count = 0;
    head->total_value_size = 0;
    int level;
    for (level = 0; level < MAX_SKIPLIST_LEVEL; level++) {
        head->forward[level] = SKIPLIST_NIL;
    }
    SKIPLIST_NODE* cur = (SKIPLIST_NODE*)MEMTABLE_NODE_ADDR;
    while(cur < (SKIPLIST_NODE*)MEMTABLE_NODE_END_ADDR) {
        cur->allocated = 0;
        for (level = 0; level < MAX_SKIPLIST_LEVEL; level++) {
            cur->forward[level] = SKIPLIST_NIL;
        }
        // cur->dirty = 0;
        cur ++;
    }
}

SKIPLIST_NODE* skiplist_insert(SKIPLIST_HEAD* head, unsigned int key, int lba, int length) {

	SKIPLIST_NODE** current_forward = head->forward;
    SKIPLIST_NODE** update[MAX_SKIPLIST_LEVEL];
    int current_level = MAX_SKIPLIST_LEVEL - 1;

        while(current_level >= 0) {
        while(current_forward[current_level] != SKIPLIST_NIL && current_forward[current_level]->key < key) {
            current_forward = current_forward[current_level]->forward;
        }
        update[current_level] = &current_forward[current_level];
        current_level--;
    }

    if (current_forward[0] == SKIPLIST_NIL || current_forward[0]->key != key) {
        int new_level = getLevel();
        SKIPLIST_NODE* new_node = allocateSkiplistNode(head, new_level);
        // aux
        new_node->key = key;
        new_node->lba = lba;
        new_node->length = length;

        while(new_level >= 0) {
            new_node->forward[new_level] = *update[new_level];
            *update[new_level] = new_node;
            new_level--;
        }

        // inserted cleanly
        head->count++;
        head->total_value_size += length;
        return (SKIPLIST_NODE*)SKIPLIST_NIL;
    } else {
        // if same key is already exists ...
        // handle by caller
        return current_forward[0];
    }
}

SKIPLIST_NODE* skiplist_search(SKIPLIST_HEAD* head, unsigned int key) {
    SKIPLIST_NODE** current_forward = head->forward;
    int current_level = MAX_SKIPLIST_LEVEL - 1;
    while(current_level >= 0) {
        while(current_forward[current_level] != (SKIPLIST_NODE*)SKIPLIST_NIL && current_forward[current_level]->key < key) {
            current_forward = current_forward[current_level]->forward;
        }
        if(current_forward[current_level]->key == key) {
            //hit
            return current_forward[current_level];
        }
        current_level--;
    }
    return (SKIPLIST_NODE*)SKIPLIST_NIL;
}

SKIPLIST_NODE* skiplist_remove(SKIPLIST_HEAD* head, unsigned int key) {
    SKIPLIST_NODE** current_forward = head->forward;
    SKIPLIST_NODE** update[MAX_SKIPLIST_LEVEL];
    int current_level = MAX_SKIPLIST_LEVEL - 1;
    while(current_level >= 0) {
        while(current_forward[current_level] != SKIPLIST_NIL && current_forward[current_level]->key < key) {
            current_forward = current_forward[current_level]->forward;
        }
        update[current_level] = &current_forward[current_level];
        current_level--;
    }
    if (current_forward[0] != SKIPLIST_NIL && current_forward[0]->key == key) {
        SKIPLIST_NODE* target = current_forward[0];
        current_level = MAX_SKIPLIST_LEVEL - 1;
        while(current_level >= 0) {
            if((*update[current_level]) == current_forward[0]) {
                *update[current_level] = current_forward[0]->forward[current_level];
            }
            current_level--;
        }
        target->allocated = 0;
        head->count--;
        return target;
    } else {
        return (SKIPLIST_NODE*)SKIPLIST_NIL;
    }
}
