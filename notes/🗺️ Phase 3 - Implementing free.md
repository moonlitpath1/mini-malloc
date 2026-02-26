## **Clues**

**3.1 — Make `free()` Actually Do Something**

Right now `free()` ignores its argument completely. It needs to do one thing: **find the meta block for that pointer and set `is_free = 1`.**

You already know how to go from user pointer → meta block. You did it in reverse in Phase 2.

```
malloc() returned this:   [ meta | user data ]
                                   ^
                          caller holds this ptr

free() receives that ptr → go back to meta → set is_free = 1
```

**Clue:** If `ptr` is typed as `struct block_meta *`, then `ptr - 1` gives you the meta block. But `ptr` comes in as `void *` — what do you cast it to first?

Implement this and move on.

---

**3.2 — Make `malloc()` Search Before Asking OS**

Right now your `malloc()` always calls `sbrk`. It needs a new first step: **walk the list looking for a free block big enough to reuse.**

This is called **first-fit** — grab the first free block where `is_free == 1 AND size >= requested_size`.

```
malloc(80) is called →
  walk list from head:
    block 1: is_free=0 → skip
    block 2: is_free=1, size=32 → too small, skip
    block 3: is_free=1, size=200 → fits! mark is_free=0, return it
  if nothing found → sbrk as before
```

**Three clues:**

1. Write a helper `find_free_block(size_t size)` that returns a `struct block_meta*` or `NULL`
2. Inside it, loop from `head` while `curr != NULL`, checking both conditions
3. Back in `malloc()`: if the helper returns non-NULL, mark that block `is_free = 0` and return its user pointer — NO sbrk needed

---

## First-fit vs Best-fit — Pick One

||First-fit|Best-fit|
|---|---|---|
|Strategy|First block big enough|Smallest block that fits|
|Speed|Fast — stops at first match|Slow — must scan entire list|
|Fragmentation|Leaves large blocks intact|Leaves many tiny unusable scraps|

**Use first-fit.** It's simpler and actually performs better in practice. glibc's unsorted bin is essentially first-fit too.

---

Build both halves, then inject into `ls` again. If it still works, your `free()` is now real. Come back and we'll run Experiment 3.1 to _watch_ fragmentation happen live. 🔨

---

## My Code
I implemented the free() code, adding feature to reuse memory that has been freed (`is_free = 1` ).




```
// in v2, we assigned metadata headers
//we will give free() some work to do here

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

struct block_meta *head = NULL;
struct block_meta *tail = NULL;

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
struct block_meta* find_free_block(size_t size)
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
	//search if free block of enough size is available to reuse -- first fit 
	struct block_meta *free_block = find_free_block(size);
	if(free_block)
	{
		free_block->is_free=0;	
		return (void*)(free_block+1); 	
	}


	//getting top of the heap
	void *top = sbrk(0);
	
	//allocting space for user data and meta data
	if( sbrk(size + sizeof(struct block_meta) ) == (void *)-1 ) return NULL;

	//meta data struct would start at the heap top
	struct block_meta *meta_data = (struct block_meta*)top;
	meta_data->size = size;
	meta_data->is_free=0;
	meta_data->next=NULL;

	//adding to metadata linkedlist	
	if(tail == NULL)
	{
		head = meta_data;
		tail=meta_data;
	}
	else
	{
		tail->next = meta_data;
		tail=meta_data;
	}

	//userdata memory starts after metadata
	top = (char*)top+sizeof(*meta_data);
	return top;
}

void free(void *ptr)
{
	if(ptr == NULL) return;

	//ptr points to the user data
	//find the meta block and set is_free=1
	struct block_meta *meta = (struct block_meta*)ptr - 1;
	meta->is_free=1;	
}

```


----

**Output:**
And the code works! 

```
anu@laptop:~/anu/programming/systems_progg/mini-malloc$ gcc -shared -fPIC -o malloc_v3.so malloc_v3.c && LD_PRELOAD=./malloc_v3.so ls
'\'                  heap_spy.c            malloc_v1.so   malloc_v3.c    og_malloc.c
 faulty_heap_spy.c   heap_spy_modified.c   malloc_v2.c    malloc_v3.so   out
 git_push.sh         malloc_v1.c           malloc_v2.so   notes
```


----


## Issue: Fragmentation. 
Memory blocks in free space that are too small can't be reused.

To experience this, I wrote a simple code:
```
#include <stdio.h>
#include <stdlib.h>

int main() {
    void *ptrs[10];

    // allocating 10 blocks of 32 bytes
    for(int i = 0; i < 10; i++)
        ptrs[i] = malloc(32);

    // free only the even-indexed ones
    for(int i = 0; i < 10; i += 2)
        free(ptrs[i]);

	//allocate 64 bytes... will it create new block or reuse??
    void *big = malloc(64);

    return 0;
}

```

I ran this code using my malloc and traced it with strace

```
anu@laptop:~/anu/programming/systems_progg/mini-malloc$ gcc test_fragmentation.c -o out LD_PRELOAD=./malloc_v3.so strace -e trace=brk ./out 2>&1 | grep brk

brk(NULL)                               = 0x555555559000    
brk(NULL)                               = 0x555555559000    
brk(0x555555559040)                     = 0x555555559040  ← malloc(32)
brk(0x555555559080)                     = 0x555555559080  ← malloc(32)
brk(0x5555555590c0)                     = 0x5555555590c0  ← malloc(32)
brk(0x555555559100)                     = 0x555555559100  ← malloc(32)
brk(0x555555559140)                     = 0x555555559140  ← malloc(32)
brk(0x555555559180)                     = 0x555555559180  ← malloc(32)
brk(0x5555555591c0)                     = 0x5555555591c0  ← malloc(32)
brk(0x555555559200)                     = 0x555555559200  ← malloc(32)
brk(0x555555559240)                     = 0x555555559240  ← malloc(32)
brk(0x555555559280)                     = 0x555555559280  ← malloc(32)

brk(0x5555555592e0)                     = 0x5555555592e0  ← malloc(64)⚠️
```

As expected, for allocating 64 bytes, malloc() could not reuse the memory of 32 bytes, since they were all fragmented, and had to create a new block instead

```
+----+----+----+----+----+----+----+----+----+----+--------+
| ✅ | A  | ✅ | A  | ✅ | A  | ✅ | A  | ✅ | A  |  64B   |
|32B |32B |32B |32B |32B |32B |32B |32B |32B |32B |  NEW   |
+----+----+----+----+----+----+----+----+----+----+--------+
```

