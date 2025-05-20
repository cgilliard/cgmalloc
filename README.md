# cgmalloc
A simple memory allocator. Implements the 4 main memory allocation functions:
* malloc (cg_malloc)
* free (cg_free)
* realloc (cg_realloc)
* calloc (cg_calloc)

## Build
cgmalloc essentially has no dependencies other than standard system calls like (mmap, munmap, sysctl, write (to display a panic message on memory corruption), sched_yield (to yield CPU while spin-locking)) and a few c standard library calls (memcpy, bzero (through compiler intrinsic), exit, and error). For testing, the criterion framework is used. This is optional for it you want to run the tests. To install criterion on linux:

```# sudo apt-get install libcriterion-dev```

or on Mac OS:

```# brew install criterion```

You may have to ensure that the headers and shared objects are in your search path.

To build the shared object:

```# make clean all```

To build the test program:

```# make clean test```

To build the bench program:

```# make clean bench```
