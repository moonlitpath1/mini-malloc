First, I'll build a mental model of what malloc does at the systems level by observing it's behavior.


## 0.1 : Using strace to see how kernel manages memory for `ls`

Command to run 
```bash
anu@laptop:~$ strace -e trace=brk,mmap,munmap ls 2>&1 | head -40
```

[[dump/strace mmap, brk, munmap|Actual code output]]

- there are 2 ways to give memory to a program
	- brk: extending the heap
	- mmap: maps all the shared library files `.so` (that ls depends on) 

```
── PHASE 1: Library Loading ──────────────────────────────────
brk(NULL)  = 0x555555579000          ← "where does my heap end?" (just a question, heap doesn't grow yet)
mmap(NULL, 8192,  ..., -1, 0)        ← anonymous memory for the loader itself
mmap(NULL, 81463, ...,  3, 0)        ← loading a .so file (fd=3 means a library file)
mmap(NULL, 181960,...,  3, 0)        ← loading another .so file
mmap(...,  118784,...,  3, 0x6000)   ← loading another .so file
... (10 more mmap lines, all fd=3) ...

── PHASE 2: Heap Actually Grows ──────────────────────────────
brk(NULL)            = 0x555555579000   ← asking again: "where is my heap?"
brk(0x55555559a000)  = 0x55555559a000   ← NOW the heap actually moves forward ✅

── PHASE 3: Last Big mmap (locale/directory data) ────────────
mmap(NULL, 5719296, PROT_READ, MAP_PRIVATE, 3, 0) = 0x7ffff7600000

── OUTPUT ────────────────────────────────────────────────────
anu
backup.log
Desktop
...

```


---


## 0.2 Watching the heap using a C program

**Theory:**
`brk()` is a kernel syscall. The kernel maintains a pointer called program break - the official boundary marking the end of the process' heap segment in the virtual memory

```
Virtual Memory Layout (kernel's view):
┌─────────────┐
│    stack    │  ← grows downward
│      ↓      │
│   (gap)     │
│      ↑      │
│    heap     │  ← grows upward
│─────────────│ ← PROGRAM BREAK (brk moves this line)
│ data/bss    │
│    text     │
└─────────────┘
```

`brk()` moves that line up or down. the kernel just manages this boundary. 

`sbrk(0)` is a C library wrapper that asks the kernel where the progam break is rn. 

|                        | What it is                                |
| ---------------------- | ----------------------------------------- |
| `brk` / program break  | End of the heap segment in virtual memory |
| Top chunk end          | End of glibc's usable unallocated space   |
| `sbrk(0)` return value | Just reads the program break              |
These all point to the same address, but for different reasons.

The heap that we refer to here is both the Cs heap and the kernel's heap. According to the kernel, we are accessing the data segment, that's a range of  addresses owned by the processor
glibc calls it heap and imposes structures on it - chunks, bins, top chunk

The kernel has no knowledge of the internal structures made by glibc. It's just a collection of bytes for it. 


This was my C program:
```C
#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>

int main()
{
        printf("Heap Pointer locataion:\nBefore:%p", sbrk(0));
        malloc(1000);

        printf("\nAfter:%p", sbrk(0));
        return 0;

}

```

and this is my what my compiled file when traced with strace looks like 

```
strace -e trace=brk ./heap_spy
```

```bash
brk(NULL)                               = 0x555555559000
brk(NULL)                               = 0x555555559000
brk(0x55555557a000)                     = 0x55555557a000

Heap Pointer locataion:
Before:0x555555559000
After:0x55555557a000+++ exited with 0 +++

```


I asked for 1000 bytes using malloc however, the difference: 

`0x55555557a000 - 0x555555559000 = 0x21000` = **135,168 bytes ≈ 132 KB**

I got wayyy more than what I asked for! 
this is because glibc is greedy. system calls are expensive so it stocks up some more memory for future use. 

*aka glibc buys memory in wholesale, and stores it in its private stock.* 

**Why were there 2 brk calls?**
```
brk(NULL)  = 0x555555559000   ← not from your code
brk(NULL)  = 0x555555559000   ← your sbrk(0) call
brk(...)   = 0x55555557a000   ← your malloc(1000)
```

The 1st brk happened before my code ran. it a part of glibc's startup routine to check where the current heap pointer is.
//notice glibc is doing stuff before main


---


## Experiment 0.3 - Breaking the pattern

```C
#include<stdio.h>
#include<unistd.h>
#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>

int main()
{
	printf("Heap Pointer locataion:\nBefore:%p", sbrk(0));
	
	printf("\n1st malloc\nMalloc Pointer Address: %p\n", malloc(1000));
	printf("Heap Pointer location:%p", sbrk(0));

	printf("\n2nd malloc\nMalloc Pointer Address: %p\n", malloc(1000));
	printf("Heap Pointer location:%p", sbrk(0));

	printf("\n3rd malloc\nMalloc Pointer Address: %p\n", malloc(1000));
	printf("Heap Pointer location:%p", sbrk(0));


	char cmd[64]; //creates a character buffer
	
	//proc -- linux virtual file system -- you are getting memory mappings of a process that containt the word "heap" using current pid
	
	sprintf(cmd, "cat /proc/%d/maps | grep heap", getpid()); 
	system(cmd); //executes the command using /bin/sh

        return 0;

}


```

OUTPUT traced with strace:
```bash
strace -e trace=brk ./out
brk(NULL)                               = 0x555555559000
brk(NULL)                               = 0x555555559000
brk(0x55555557a000)                     = 0x55555557a000
Heap Pointer locataion:
Before:0x555555559000

--- 1st malloc
Malloc Pointer Address: 0x5555555596b0
Heap Pointer location:0x55555557a000

--- 2nd malloc
Malloc Pointer Address: 0x555555559aa0
Heap Pointer location:0x55555557a000

--- 3rd malloc
Malloc Pointer Address: 0x555555559e90

555555559000-55555557a000 rw-p 00000000 00:00 0                          [heap]
--- SIGCHLD {si_signo=SIGCHLD, si_code=CLD_EXITED, si_pid=11441, si_uid=1000, si_status=0, si_utime=0, si_stime=0} ---
Heap Pointer location:0x55555557a000+++ exited with 0 +++

```


We notice that all 3 malloc pointer addresses are within the initial 132kb reigion. 

The 132KB call had set up the **arena**, wherein my data was packed neatly. the memory malloc got was from the arena's **top chunk**

brk() was called just once to set up the arena, and after that glibc gave the memory as needed to malloc. 


-----

**Extra:** 
Previously, my code looked like this:
```
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main()
{
    printf("Heap Pointer location:\nBefore:%p", sbrk(0));
    printf("\n1st malloc\nMalloc Pointer Address: %p\nHeap Pointer location:%p", malloc(1000), sbrk(0));
    printf("\n2nd malloc\nMalloc Pointer Address: %p\nHeap Pointer location:%p", malloc(1000), sbrk(0));
    printf("\n3rd malloc\nMalloc Pointer Address: %p\nHeap Pointer location:%p", malloc(1000), sbrk(0));

    return 0;
}
```

and for some reason, the strace looked like this when I ran it, after rebooting however, looked like the previous strace, no `brk()` calls after `malloc()`. I have absolutely no clue why as of now, but I'll explore it in the future. 

```
Heap Pointer locataion:
Before:0x555555559000
1st malloc
Malloc Pointer Address: 0x5555555596b0
Heap Pointer location:0x55555557a000
2nd malloc
Malloc Pointer Address: 0x555555559aa0
Heap Pointer location:0x55555557a3e8
3rd malloc
Malloc Pointer Address: 0x555555559e90
Heap Pointer location:0x55555557a7d0
Press ENTER or type command to continue
brk(NULL)                               = 0x555555559000
brk(NULL)                               = 0x555555559000
brk(0x55555557a000)                     = 0x55555557a000
Heap Pointer locataion:
brk(0x55555557a3e8)                     = 0x55555557a3e8
Before:0x555555559000
1st malloc
Malloc Pointer Address: 0x5555555596b0
brk(0x55555557a7d0)                     = 0x55555557a7d0
Heap Pointer location:0x55555557a000
2nd malloc
Malloc Pointer Address: 0x555555559aa0
brk(0x55555557abb8)                     = 0x55555557abb8
Heap Pointer location:0x55555557a3e8
3rd malloc
Malloc Pointer Address: 0x555555559e90
Heap Pointer location:0x55555557a7d0+++ exited with 0 +++
Heap Pointer locataion:
Before:0x555555559000
1st malloc
Malloc Pointer Address: 0x5555555596b0
Heap Pointer location:0x55555557a000
2nd malloc
Malloc Pointer Address: 0x555555559aa0
Heap Pointer location:0x55555557a3e8
3rd malloc
Malloc Pointer Address: 0x555555559e90
Heap Pointer location:0x55555557a7d0
```


