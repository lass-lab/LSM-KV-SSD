#include "sstable.h"

unsigned int simple_hash(const unsigned int key, const unsigned int seed)
{
	const unsigned int m = 0xc6a4a793;
	unsigned int h = seed ^ (4*m);
	h += key;
	h *= m;
	h ^= (h >> 16);
	return h;
}
