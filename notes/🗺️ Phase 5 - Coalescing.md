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



Backward Coaleascing involves, going back and checking if the previous block is free, and then merging the two blocks using similar logic as before. 

Checking the previous block's availability would require the size of the previous block to reach to the meta data of the prev block. 

---

## Reading glibc's `malloc.c`
glibc's `malloc` contains a clever trick to identify whether the previous block is free or not. 
All blocks are aligned, so each block's size is always a multiple of 8 or 16. This means that in binary the last 3 digits are always 0.
	`	Eg. size = 96  → binary: 01100000`

So, since the last bit is always 0, glibc decides to use it to store something useful in it. 
Thus, the last 0 of the current chunk's `size` field stores whether the previous block is free (0) or in use (1).

What does this mean? 

This is how glibc's memory allocation looks like:
```
chunk A:
[ prev_size (8 bytes) | size+flags (8 bytes) | user data... ]

chunk B (immediately after A):
[ prev_size (8 bytes) | size+flags (8 bytes) | user data... ]
```
there is no footer here that stores the current chunk's size. instead, the prev chunk's size is stored in the next chunk as a variable (`prev_size`).
And this is the variable whose last bit we manipulate for storing whether the previous chunk is free or not. 

This is really clever as it not only stores a lot of space, it also increases simplicity. 

glibc maintains a macro `PREV_INUSE`
```
#define PREV_INUSE 0x1
```


When glibc wants to check if the previous chunk is in use:
```
if (chunk->size & PREV_INUSE)   // AND operation
```

When it wants to set it:
```
chunk->size |= PREV_INUSE;      // OR operation
```

When it wants to clear it:
```
chunk->size &= ~PREV_INUSE;     // AND + NOT operation
```

In this manner, glibc cleverly maintains whether the previous block is free or not, using bit manipulation.


---

For my program, I am going to implement a simple footer which will maintain the current block's size. 
So, if I want to go to the previous block, I can calculate the prev block's footer location, see it's size and calculate location of the previous block's meta header. 

```
[-A header | A user data | A footer=32-] [-B header | B user data | B footer=64-]
```

current position: B user data
to go to: A header

---


## Version 1 of backwards coalesce
```
void backward_coalesce(struct block_meta *curr_block)
{
        if((struct block_meta*)curr_block == head) return;

        size_t *prev_footer = (size_t*)((char*)curr_block - sizeof(size_t));
        struct block_meta *prev_block = (struct block_meta*)((char*)prev_footer - *prev_footer - sizeof(struct block_meta));
        if(!prev_block->is_free) return;

        prev_block->size = prev_block->size + sizeof(struct block_meta) + curr_block->size;
        prev_block->next = curr_block->next;
}

```

There is segmentation fault here..

---
**Clues I got:**
**Clue 1:** You maintain two global pointers about the list. Your function updates one of them in the merge. What about the other one — what happens if `curr_block` happens to be at the very end of the list?

**Clue 2:** After merging, future `backward_coalesce` calls on blocks that come AFTER `curr_block` will need to read something to find their previous block. Is that something still accurate after your merge? 🔨

---

After fixing those bugs, the code still wont compile. I realized that I had not considered the space taken by footer (the new addition) in many other places throughout the code, such as block splitting and new block creation. 

Upon fixing the issues, the code was successfully compiled for both cases! 
Forward as well as backward

Same test_coalescing.c code
`b` freed before `a`
```
anu@laptop:~/anu/programming/systems_progg/mini-malloc$ # forward coaslescing
LD_PRELOAD=./malloc_v5.so ./out
Block 1: | Free: 0 | Size: 32
Block 2: | Free: 0 | Size: 64
Block 3: | Free: 0 | Size: 64
Block 4: | Free: 0 | Size: 1024
Block 1: | Free: 1 | Size: 136
Block 2: | Free: 0 | Size: 64
Block 3: | Free: 0 | Size: 1024

```

test_coalescing.c code where `a` is freed before `b`
	
```

anu@laptop:~/anu/programming/systems_progg/mini-malloc$ # backward coaslescing
LD_PRELOAD=./malloc_v5.so ./out
Block 1: | Free: 0 | Size: 32
Block 2: | Free: 0 | Size: 64
Block 3: | Free: 0 | Size: 64
Block 4: | Free: 0 | Size: 1024
Block 1: | Free: 1 | Size: 136
Block 2: | Free: 0 | Size: 64
Block 3: | Free: 0 | Size: 1024
```


----
## Full Code:
```
//coalescing
#include<stdio.h>
#include<unistd.h>
#include<stddef.h>

/**
* struct block_meta - metadata header for each allocated block 
* @size: size of user data reigion in bytes
* @is_free: nonzero if free, 0 if allocated 
* @next: pointer to the next metadata block in the list
 */
struct block_meta
{
	size_t size;
	int is_free;
	struct block_meta *next;
} __attribute__((aligned(16)));

static struct block_meta *head = NULL;
static struct block_meta *tail = NULL;


/**
 * find_free_block - finds a free block large enough for allocation 
 * @size: size of user data
 * 
 * Performs a first-fit search over the block list starting at @head and 
 * returns the first free block whose size is greater than or equal to @size
 *
 * Return:
 * Pointer to a suitable free block on success, or NULL if no such block exists
*/
static struct block_meta* find_free_block(size_t size)
{
        //search if free block of enough size is available to reuse -- first fit
        struct block_meta *curr = head;
	while(curr != NULL)
        {
 		if(curr->is_free==1 && curr->size >= size)
                {
                       return curr;
                }
                curr = curr->next;
        }
	return NULL;
}


/**
 * split_block - splits a block into required size 
 * @block - pointer to free block header
 * @size - required size
 */

static void split_block(struct block_meta *block, size_t size)
{
	//block splitting
        // |      free     |   ---->  |   required_size   |   remainder   |

        struct block_meta *remainder = (struct block_meta*)((char*)(block + 1) + size + sizeof(size_t));
        // assigning meta data to new block
        remainder->size = block->size - size - sizeof(struct block_meta) - sizeof(size_t);
        remainder->is_free = 1;
        remainder->next = block->next;

	//editing footer
	size_t *footer = (size_t*)((char*)(remainder+1) + remainder->size);
	*footer = remainder->size;
        //editing free block's meta data
        block->size = size;
        block->next = remainder;
	footer = (size_t*)((char*)(block+1) + block->size);
	*footer = block->size;

	if(block == tail) 
		tail = remainder;
}



//foward coalescing
void forward_coalesce(struct block_meta *curr_block)
{
        //check if it's the last block in physical memory
        if((void*)((char*)(curr_block+1)+curr_block->size) >= sbrk(0)) return;
        
	struct block_meta *next_block = (struct block_meta*)((char*)(curr_block+1)+curr_block->size + sizeof(size_t));
        if(!next_block->is_free) return; 

        curr_block->size = curr_block->size + sizeof(size_t) + sizeof(struct block_meta) + next_block->size;
        curr_block->next = next_block->next;

	size_t *footer = (size_t*)((char*)(curr_block+1) + curr_block->size);
	*footer = curr_block->size;
}




//backward coalescing
void backward_coalesce(struct block_meta *curr_block)
{
	if(curr_block == head) return;

	size_t *footer = (size_t*)((char*)curr_block - sizeof(size_t));
	struct block_meta *prev_block = (struct block_meta*)((char*)footer - *footer - sizeof(struct block_meta));
	if(!prev_block->is_free) return;

	prev_block->size = prev_block->size + sizeof(size_t) + sizeof(struct block_meta) + curr_block->size;
	prev_block->next = curr_block->next;

	if(curr_block == tail) tail = prev_block;

	footer = (size_t*)((char*)(prev_block+1) + prev_block->size);
	*footer = prev_block->size;
	
}
/**
 * malloc - allocates heap memory 
 * @size: memory in bytes to be allocated
 *
 * Allocates memory block of at least @size bytes. If free block is available,
 * then the allocator reuses that, else the heap is extended using sbrk() and a new 
 * block is created.
 *
 * Returns: 
 * Pointer to the start of the memory block 
*/

void* malloc(size_t size)
{
	struct block_meta *block;
	if(block = find_free_block(size))
	{
		block->is_free = 0;
		if(block->size > sizeof(struct block_meta) + 1)
			split_block(block, size);
	}
	else
	{
		block = sbrk(0);
		//allocting space for user data, meta data and footer
        	if( sbrk(size + sizeof(struct block_meta) + sizeof(size_t)) == (void *)-1 ) return NULL;

        	//meta data struct would start at the heap top
        	block->size = size;
        	block->is_free=0;
        	block->next=NULL;


        	//adding to metadata linkedlist 
        	if(tail == NULL)
        	{
                	head = block;
                	tail = block;
        	}
       		 else
    		{
       			tail->next = block;
	                tail = block;
	        }
		
       
		size_t *footer = (size_t*)((char*)(block+1) + size);
		*footer = size;
	
	}
       
	return (void*)(block+1);

}




/**
 *free
 *@ptr: pointer to memory location to be freed
 *returns: void
*/
void free(void *ptr)
{  
	if(ptr == NULL) return;

	//ptr points to the user data
	//find the meta block and set is_free=1
	struct block_meta *curr_block = (struct block_meta*)ptr - 1;
	curr_block->is_free=1;
	
	//coalescing
	backward_coalesce(curr_block);
	forward_coalesce(curr_block);

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


---

This marks the completion of phase 5!