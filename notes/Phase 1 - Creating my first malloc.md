### Clues that I've got: 

Create `mymalloc.c`. You need **two** functions (not one):

```
#include <unistd.h>
#include <stddef.h>

void *malloc(size_t size) {
    // your 3-line bump allocator here
}

void free(void *ptr) {
    // intentionally empty for now — but must exist
}
```

**Why does `free` need to exist even empty?** When `ls` runs, it calls `free()` on memory it allocated. If you don't provide `free`, your `.so` has no `free` symbol, so the linker falls back to glibc's `free` — which will try to read metadata from a pointer your malloc never wrote, and **segfault.** A stub that does nothing is safer for now.

**Your 3 clues for `malloc`:**
1. `sbrk(0)` → returns current heap end → **this is what you return to the caller**
2. `sbrk(size)` → shoves the heap forward by `size` bytes
3. Do step 1 first, step 2 second — one line each

**Failure check clue:** `sbrk()` returns `(void *)-1` on failure. Real malloc returns `NULL` on failure. Handle it.

**Compile + inject:**
```bash
gcc -shared -fPIC -o mymalloc.so mymalloc.c
LD_PRELOAD=./mymalloc.so ls
```

**What success looks like:** `ls` prints your files. No segfault.

**What to watch for:** Run it with `strace -e trace=brk` too. You'll see something very different from glibc's single big-grab behavior — your allocator hits `brk` on every single call. That's the inefficiency you're building toward fixing in Phase 3.

---

**Mistake 1**
I created a main function and tried running a test code in it, while creating the malloc and free functions and leaving them empty. 

However, this lead to segfault. 

**Reason**: 
My code's `malloc()` was overriding the original malloc's and free's code. 
And `printf()` depends upon `malloc()` to create space since it uses the output buffer, etc. 
So, when it encountered the empty malloc() it ran into a frenzy and this lead to a segfault. 

```
#include<stdio.h>
#include<unistd.h>
#include<stddef.h>

void* malloc(size_t size)
{
    /* void *top = sbrk(0); */
    /* printf("Print %p\n", top); */
}

void free(void *ptr)
{
    /*
    intentionally empty for now
    */
}

int main()
{
    printf("%p\n", sbrk(0));
    return 0;
}
```

I learnt that the `main()` function is not needed here, and to never use `printf()`s inside an malloc. 

----

**My code:**
```
cat malloc_v1.c
//the most basic malloc ever
//aka bump allocator -- since the ptr can only move forward like a bump on a counter --- your heap can only grow. never shrink. 
//no memory reusability. 


#include<unistd.h>
#include<stddef.h>

void* malloc(size_t size)
{
	void *top = sbrk(0);
	if(sbrk(size) == (void *)-1) return NULL;
	return top;
}

void free(void *ptr)
{
	(void)ptr;
	//intentionally empty for now -- we are going to test it on `ls` 
	//but ls calls free() on memory, 
	//if we dont provide the function the .so file has no free symbol,
	//so the linkter falls back to glibc's free
	//which tries to read metadata from a pointer malloc never wrote
	//and segfault. 
}

```

---

**Testing my custom malloc() on `ls`**
```
anu@laptop:~/anu/programming/systems_progg/mini-malloc$ gcc -shared -fPIC -o mymalloc.so mymalloc.c

anu@laptop:~/anu/programming/systems_progg/mini-malloc$ LD_PRELOAD=./malloc_v1.so ls
faulty_heap_spy.c  heap_spy_modified.c  malloc_v1.so  og_malloc.c
heap_spy.c         malloc_v1.c          notes         out

```

IT WORKED!!! 
It displayed all the files in my directory!!

---

**strace of running `ls` using my malloc()**
51 malloc() calls. my bump allocator asked the OS for memory on *every* single `malloc()` call. every tiny filename, string, struct that `ls` allocates needed a call to the kernel. 

Every `brk()` call is a context switch. the program has to stop, go to the kernel, wait, then come back. 


```
anu@laptop:~/anu/programming/systems_progg/mini-malloc$ LD_PRELOAD=./malloc_v1.so strace -e trace=brk ls 2>&1 | grep brk
brk(NULL)                              = 0x555555579000
brk(NULL)                              = 0x555555579000
brk(0x5555555791d8)                    = 0x5555555791d8
brk(0x555555579250)                    = 0x555555579250
brk(0x555555579650)                    = 0x555555579650
brk(0x555555579655)                    = 0x555555579655
brk(0x5555555796cd)                    = 0x5555555796cd
brk(0x5555555796d9)                    = 0x5555555796d9
brk(0x5555555799f1)                    = 0x5555555799f1
brk(0x555555579a59)                    = 0x555555579a59
brk(0x555555579f89)                    = 0x555555579f89
brk(0x55555557a059)                    = 0x55555557a059
brk(0x55555557a201)                    = 0x55555557a201
brk(0x55555557a261)                    = 0x55555557a261
brk(0x55555557a2b1)                    = 0x55555557a2b1
brk(0x55555557a321)                    = 0x55555557a321
brk(0x55555557a3c1)                    = 0x55555557a3c1
brk(0x55555557a421)                    = 0x55555557a421
brk(0x55555557a469)                    = 0x55555557a469
brk(0x55555557a521)                    = 0x55555557a521
brk(0x55555557a52d)                    = 0x55555557a52d
brk(0x55555557a539)                    = 0x55555557a539
brk(0x55555557a545)                    = 0x55555557a545
brk(0x55555557a551)                    = 0x55555557a551  ← 🚫 12b
brk(0x55555557a55d)                    = 0x55555557a55d  ← 🚫 12b
brk(0x55555557a569)                    = 0x55555557a569
brk(0x55555557a575)                    = 0x55555557a575
brk(0x55555557a581)                    = 0x55555557a581
brk(0x55555557a58d)                    = 0x55555557a58d
brk(0x55555557a599)                    = 0x55555557a599
brk(0x55555557a5a5)                    = 0x55555557a5a5
brk(0x55555557a5b1)                    = 0x55555557a5b1
brk(0x55555557a5bd)                    = 0x55555557a5bd
brk(0x55555557a5df)                    = 0x55555557a5df
brk(0x55555557a5e9)                    = 0x55555557a5e9
brk(0x55555557a621)                    = 0x55555557a621
brk(0x55555557a659)                    = 0x55555557a659
brk(0x55555557a6d9)                    = 0x55555557a6d9
brk(0x55555559b6d9)                    = 0x55555559b6d9  ← 💡 132kb
brk(0x55555559c000)                    = 0x55555559c000
brk(0x55555559c020)                    = 0x55555559c020
brk(0x55555559c022)                    = 0x55555559c022
brk(0x5555555a4052)                    = 0x5555555a4052
brk(0x5555555a405e)                    = 0x5555555a405e
brk(0x5555555a4062)                    = 0x5555555a4062
brk(0x5555555a4068)                    = 0x5555555a4068
brk(0x5555555a407c)                    = 0x5555555a407c
brk(0x5555555a4087)                    = 0x5555555a4087
brk(0x5555555a4093)                    = 0x5555555a4093
brk(0x5555555a40a0)                    = 0x5555555a40a0
brk(0x5555555a40b2)                    = 0x5555555a40b2
brk(0x5555555a50b2)                    = 0x5555555a50b2
```


**strace of running `ls` using regular malloc**
just 3 brk calls. 
glibc had asked for a huge chunk of memory at once, and then it handled all those 51 allocations by itself, not disturbing the kernel. 

```
anu@laptop:~/anu/programming/systems_progg/mini-malloc$ strace -e trace=brk ls 2>&1 | grep brk
brk(NULL)                               = 0x555555579000
brk(NULL)                               = 0x555555579000
brk(0x55555559a000)                     = 0x55555559a000
```

This is the biggest limitation of the bump allocator. 
No memory reusability. 


