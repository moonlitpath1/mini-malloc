## Clues:
This is the most intellectually satisfying phase. You're about to fix the opposite problem — **external fragmentation**: adjacent free blocks that could be one big block but aren't.

```
[meta|FREE 64][meta|FREE 128][meta|USED 32]
```

If someone asks for `malloc(100)` — your `find_free_block` skips both free blocks since neither is ≥ 100 alone. But together they're 192 bytes. Coalescing **merges adjacent free blocks** when freeing.​

The key insight to think about before writing any code:

When `free(ptr)` is called and you mark a block free — how do you find the block that comes **immediately after** it in memory (not in the linked list, but physically adjacent)? You know the block's address and its size. Where does the next block's header start?

---

**The Implication for Coalescing**

Since the heap is flat and you control all structure, you can **walk forward** from any block by doing `address + sizeof(meta) + size` and you'll land precisely on the next block's header — guaranteed.

The only edge case: what if you walk past the END of the heap? What value would `next_physical` have then, and how do you detect it?

---


![[../images/20260305_165504.jpg]]


## 📚 Flow:
In `free()` function:- 
1) use existing logic to find `*meta_curr`
2) check if it's the last block in **physical** memory 
		`(char*)(meta_curr+1) + meta_curr->size >= sbrk(0)`
3) y -> check if next_block in **physical memory** is free
4) y -> update `meta_curr->size = meta_curr->size + sizeof(*meta_curr) + next_block->size`
5) update meta_curr->is_free
6) update meta_curr->next = next_block->next


---

## Implementation and results: 

Added the above logic to code
```
void free(void *ptr)
{
	if(ptr == NULL) return;

	//ptr points to the user data
	//find the meta block and set is_free=1
	struct block_meta *curr_block = (struct block_meta*)ptr - 1;
	curr_block->is_free=1;
	
	//coalescing
 	//check if it's the last block in physical memory	
	if((void*)((char*)(curr_block+1)+curr_block->size) >= sbrk(0)) return;
	struct block_meta *next_block = (struct block_meta*)((char*)(curr_block+1)+curr_block->size);
	if(next_block->is_free == 0) return;
	
	curr_block->size = curr_block->size + sizeof(struct block_meta) + next_block->size;
	curr_block->next = next_block->next;
}


/*prints heap contents */
void heap_dump()
{
	struct block_meta *curr = head;
	int i=0;
	while(curr != NULL)
	{
		i++;
		printf("Block %d: | Free: %d | Size: %zu\n",i, curr->is_free, curr->size); 
		curr = curr->next;
	}

}

```




Tested if the code works for `ls`
```
anu@laptop:~/anu/programming/systems_progg/mini-malloc$ gcc -shared -fPIC -o malloc_v5.so malloc_v5.c && LD_PRELOAD=./malloc_v5.so ls
'\'                    images         malloc_v3.so   og_malloc.c
 dump                  malloc_v1.c    malloc_v4.c    out
 faulty_heap_spy.c     malloc_v1.so   malloc_v4.so   test_coalescing.c
 git_push.sh           malloc_v2.c    malloc_v5.c    test_fragmentation.c
 heap_spy.c            malloc_v2.so   malloc_v5.so   test_split.c
 heap_spy_modified.c   malloc_v3.c    notes
```



This was my C code `test_coalescing.c` used to test the allocator
```
#include<stdio.h>
#include<stdlib.h>
void heap_dump(void);
int main()
{
	void *a, *b, *c, *d;
	a = malloc(32);
	b = malloc(64);
	c = malloc(64);	

	heap_dump();

	free(b);	
	free(a);

	heap_dump();

	
	return 0;
}

```

Built and compiled the code:
```
anu@laptop:~/anu/programming/systems_progg/mini-malloc$ gcc test_coalescing.c ./malloc_v5.so -o out
```

Ran program
```
anu@laptop:~/anu/programming/systems_progg/mini-malloc$ LD_PRELOAD=./malloc_v5.so ./out
Block 1: | Free: 0 | Size: 32
Block 2: | Free: 0 | Size: 64
Block 3: | Free: 0 | Size: 64
Block 4: | Free: 0 | Size: 1024

Block 1: | Free: 1 | Size: 128
Block 2: | Free: 0 | Size: 64
Block 3: | Free: 0 | Size: 1024
```

As we can see, after freeing `a` and `b`, there are only 3 blocks in total. This means that Coalesing was successful.

----

## Issues
Currently, I have implemented just forward coalesing. Backward Coalescing isn't possible currently. In the next section, I shall implemet that. 

```
        void *a, *b, *c, *d;
        a = malloc(32);
        b = malloc(64);
        c = malloc(64);
        heap_dump();
        free(a);
        free(b);
        heap_dump();
        return 0;
```

```
anu@laptop:~/anu/programming/systems_progg/mini-malloc$ LD_PRELOAD=./malloc_v5.so ./out
Block 1: | Free: 0 | Size: 32
Block 2: | Free: 0 | Size: 64
Block 3: | Free: 0 | Size: 64
Block 4: | Free: 0 | Size: 1024
Block 1: | Free: 1 | Size: 32
Block 2: | Free: 1 | Size: 64
Block 3: | Free: 0 | Size: 64
Block 4: | Free: 0 | Size: 1024
```



