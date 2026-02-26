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
```
anu@laptop:~/anu/programming/systems_progg/mini-malloc$ cat malloc_v3.c
// in v2, we assigned metadata headers
//we will give free() some work to do here

#include<unistd.h>
#include<stddef.h>

/*
* struct block_meta - Metadata header for each allocated block 
* @size   : Size of user data reigion (in bytes)
* @is_free: Indicates whether the block is free(1) or not(0) 
* @next   : Pointer to the next metadata block in the list
 */
struct block_meta
{
	size_t size;
	int is_free;
	struct block_meta *next;
} __attribute__((aligned(16)));

struct block_meta *head = NULL;
struct block_meta *tail = NULL;

struct block_meta* find_free_block(size_t size)
{
        //search if free block of enough size is available to reuse -- first fit
        if( head == NULL) return NULL;
       
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


**Output:**

```
anu@laptop:~/anu/programming/systems_progg/mini-malloc$ gcc -shared -fPIC -o malloc_v3.so malloc_v3.c && LD_PRELOAD=./malloc_v3.so ls
'\'                  heap_spy.c            malloc_v1.so   malloc_v3.c    og_malloc.c
 faulty_heap_spy.c   heap_spy_modified.c   malloc_v2.c    malloc_v3.so   out
 git_push.sh         malloc_v1.c           malloc_v2.so   notes

```