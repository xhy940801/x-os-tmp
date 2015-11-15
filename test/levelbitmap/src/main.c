#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>

int main()
{
	srand(time(NULL));
	int size = 100000;
	//printf("Please input test count\n");
	//scanf("%d", &size);
	struct level_bitmap_t lb;
	int rs[10000];
	for(int i = 0; i < 10000; ++i)
		rs[i] = 0;
	lb.bitmaps = malloc(sizeof(uint32_t) * 1000000);
	memset(lb.bitmaps, 0, sizeof(uint32_t) * 1000000);
	lb.max_level = 3;

	size_t dddd = 0;
	for(int i = 0; i < size; ++i)
	{
		switch(rand() % 7)
		{
			case 0:
			{
				if(size < 100)
					printf("bittest\n");
				int pos = rand() % 10000;
				int trs = level_bitmap_bit_test(&lb, pos);
				assert((rs[pos] != 0 && trs != 0) || (rs[pos] == 0 && trs == 0));
				break;
			}
			case 1:
			{
				if(size < 100)
					printf("bitset\n");
				int pos = rand() % 10000;
				int trs = level_bitmap_bit_set(&lb, pos);
				assert((rs[pos] != 0 && trs != 0) || (rs[pos] == 0 && trs == 0));
				rs[pos] = 1;
				break;
			}
			case 2:
			{
				if(size < 100)
					printf("bitclear\n");
				int pos = rand() % 10000;
				int trs = level_bitmap_bit_clear(&lb, pos);
				assert((rs[pos] != 0 && trs != 0) || (rs[pos] == 0 && trs == 0));
				rs[pos] = 0;
				break;
			}
			case 3:
			{
				if(size < 100)
					printf("getmin\n");
				int trs = level_bitmap_get_min(&lb);
				if(trs < 0)
				{
					for(size_t j = 0; j < 10000; ++j)
						assert(rs[j] == 0);
				}
				else
				{
					for(int j = 0; j < trs; ++j)
						assert(rs[j] == 0);
					assert(rs[trs] != 0);
				}
				break;
			}
			case 4:
			{
				if(size < 100)
					printf("getmax\n");
				int trs = level_bitmap_get_max(&lb);
				if(trs < 0)
				{
					for(size_t j = 0; j < 10000; ++j)
						assert(rs[j] == 0);
				}
				else
				{
					for(int j = trs + 1; j < 10000; ++j)
						assert(rs[j] == 0);
					assert(rs[trs] != 0);
				}
				break;
			}
			case 5:
			{
				if(dddd++ % 10 != 0)
					break;
				if(size < 100)
					printf("cpy\n");
				struct level_bitmap_t lbt;
				lbt.bitmaps = malloc(sizeof(uint32_t) * 1000000);
				memset(lbt.bitmaps, 0, sizeof(uint32_t) * 1000000);
				lbt.max_level = 3 + rand() % 3;
				level_bitmap_cpy(&lbt, &lb, 10000);
				free(lb.bitmaps);
				lb = lbt;
				break;
			}
			default:
			{
				--i;
				break;
			}
		}
	}

	for(size_t i = 0; i < 10000; ++i)
	{
		int trs = level_bitmap_bit_test(&lb, i);
		assert((rs[i] != 0 && trs != 0) || (rs[i] == 0 && trs == 0));
	}
    
    
    printf("succ\n");
    return 0;
}
