#ifndef __APRIORI_PAGING_ALLOC_H__
#define __APRIORI_PAGING_ALLOC_H__

#define MAX_PROC_NAME_LEN 16

enum {
  INVALID_PFN,
  EXCEED_MEMORY,
  GUARD_PAGE,
  OCCUPIED,
  BUDDY_OCCUPIED,
  WRONG_ALIGNMENT,
  EXCEED_ZONE,
  NOTHING_FOUND,
  BUDDY_GUARD,
  SUBBLOCK_F,
  PAGECACHE_F,
  NUMA_F,
  NUM_CONTIG_FAILURES
}; 

enum {
  SUPERBLOCK,
  SUBBLOCK_S,
  EXACT,
  RMM_RESERVED,
  PAGECACHE,
  NUM_CONTIG_SUCCESS
};

enum {
  ANON_SAME_PROCESS_SAME_VMA,
  ANON_SAME_PROCESS_DIFFERENT_VMA,
  ANON_OTHER_PROCESS,
  NOT_ANON,
  NUM_FAIL_STAT
};

extern atomic64_t contiguity_failure[MAX_ORDER][NUM_CONTIG_FAILURES];
extern atomic64_t contiguity_success[MAX_ORDER][NUM_CONTIG_SUCCESS];
extern unsigned long contiguity_bucket[MAX_ORDER];
extern unsigned long contiguity_failed_bucket[MAX_ORDER];
extern unsigned long contiguity_fallback_max_order[MAX_ORDER];

extern atomic64_t total_contiguity_failure[NUM_CONTIG_FAILURES];
extern atomic64_t total_contiguity_success[NUM_CONTIG_SUCCESS];
extern unsigned long total_contiguity_heap_failures[NUM_FAIL_STAT];
extern unsigned long total_contiguity_file_failures[NUM_FAIL_STAT];
extern unsigned long total_thp_cow;
extern unsigned long total_4k_cow;
extern unsigned long total_khugepaged_contig_alloc_success;
extern unsigned long total_khugepaged_contig;
extern unsigned long total_khugepaged_non_contig;

extern unsigned long long total_alloc_hint;
#endif
