Build:
    make

Run:
    /usr/bin/time --verbose ./my_allocator 1024 0
    /usr/bin/time --verbose ./my_allocator 1024 1

Arguments:
    N_pages  Number of pages to allocate
    init     0 = allocate only (do not touch memory)
             1 = allocate and initialize memory (touch pages)

