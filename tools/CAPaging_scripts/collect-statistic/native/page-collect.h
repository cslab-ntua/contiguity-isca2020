
#ifndef __page_collect_h__
#define __page_collect_h__




// start - compile time options

//#define DEBUG 1
#define ENABLE_KPAGEFLAGS 1

// end - compile time options





#define FILENAMELEN         256
#define LINELEN             8192
#define PAGE_SIZE           4096
#define NR_PAGES_THP        512
#define HUGE_PAGE_SIZE      PAGE_SIZE*NR_PAGES_THP

#define PROC_DIR_NAME       "/proc"
#define MAPS_NAME           "maps"
#define PAGEMAP_NAME        "pagemap"
#define KPAGEFLAGS_NAME     "kpageflags"
#define CMDLINE_NAME        "cmdline"
#define STAT_NAME           "stat"
#define OUT_NAME            "./page-collect.dat"

#define FALSE               0
#define TRUE                (!FALSE)

#define DYNAMIC_ANCHOR_DISTANCE	TRUE
#define DEFAULT_ANCHOR_DISTANCE	14
#define MAX_ANCHOR_DISTANCE	22


#define PM_ENTRY_BYTES      sizeof(uint64_t)
#define PM_STATUS_BITS      3
#define PM_STATUS_OFFSET    (64 - PM_STATUS_BITS)
#define PM_STATUS_MASK      (((1LL << PM_STATUS_BITS) - 1) << PM_STATUS_OFFSET)
#define PM_STATUS(nr)       (((nr) << PM_STATUS_OFFSET) & PM_STATUS_MASK)
#define PM_PSHIFT_BITS      6
#define PM_PSHIFT_OFFSET    (PM_STATUS_OFFSET - PM_PSHIFT_BITS)
#define PM_PSHIFT_MASK      (((1LL << PM_PSHIFT_BITS) - 1) << PM_PSHIFT_OFFSET)
#define PM_PSHIFT(x)        (((u64) (x) << PM_PSHIFT_OFFSET) & PM_PSHIFT_MASK)
#define PM_PFRAME_MASK      ((1LL << PM_PSHIFT_OFFSET) - 1)
#define PM_PFRAME(x)        ((x) & PM_PFRAME_MASK)

#define PM_PRESENT          PM_STATUS(4LL)
#define PM_SWAP             PM_STATUS(2LL)

#define KPF_LOCKED          0
#define KPF_ERROR           1
#define KPF_REFERENCED		2
#define KPF_UPTODATE		3
#define KPF_DIRTY           4
#define KPF_LRU             5
#define KPF_ACTIVE          6
#define KPF_SLAB            7
#define KPF_WRITEBACK		8
#define KPF_RECLAIM         9
#define KPF_BUDDY           10

/* [11-20] new additions in 2.6.31 */
#define KPF_MMAP            11
#define KPF_ANON            12
#define KPF_SWAPCACHE		13
#define KPF_SWAPBACKED		14
#define KPF_COMPOUND_HEAD	15
#define KPF_COMPOUND_TAIL	16
#define KPF_HUGE            17
#define KPF_UNEVICTABLE		18
#define KPF_HWPOISON		19
#define KPF_NOPAGE          20
#define KPF_KSM             21
#define KPF_THP             22

#define BIT(name)		(1ULL << KPF_##name)
#define CMP_BIT(nr, bit) ((nr) & (1<<(bit)))


#endif //__page_collect_h__

