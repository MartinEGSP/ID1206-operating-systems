Build:
    make

Run (huge pages):
    perf stat -e page-faults ./page_faults 8192


#Edit the mmap flags to exclude MAP_HUGETLB, rebuild, then run:

Run (normal pages):
    perf stat -e page-faults ./page_faults 8192
