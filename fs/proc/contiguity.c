#include<linux/fs.h>
#include<linux/proc_fs.h>
#include<linux/seq_file.h>
#include<linux/kernel.h>
#include<linux/init.h>
#include<linux/utsname.h>
#include<linux/contiguity.h>

atomic64_t total_contiguity_failure[NUM_CONTIG_FAILURES];
atomic64_t total_contiguity_success[NUM_CONTIG_SUCCESS];
unsigned long long total_alloc_hint;

atomic64_t contiguity_failure[MAX_ORDER][NUM_CONTIG_FAILURES];
atomic64_t contiguity_success[MAX_ORDER][NUM_CONTIG_SUCCESS];
unsigned long nr_pages_rmm_reserved;
int order[MAX_ORDER];

unsigned long total_contiguity_heap_failures[NUM_FAIL_STAT];
unsigned long total_contiguity_file_failures[NUM_FAIL_STAT];
unsigned long total_thp_cow;
unsigned long total_4k_cow;
unsigned long total_khugepaged_contig;
unsigned long total_khugepaged_non_contig;
unsigned long total_khugepaged_contig_alloc_success;

unsigned long contiguity_bucket[MAX_ORDER];
unsigned long contiguity_failed_bucket[MAX_ORDER];
unsigned long contiguity_fallback_max_order[MAX_ORDER];

static int order_contiguity_failure_show(struct seq_file *m,void *v)
{
 
  int order = *((int *) m->private);
  seq_printf(m, "INVALID_PFN:\t%ld\n",atomic64_read(&contiguity_failure[order][INVALID_PFN]));
  seq_printf(m, "EXCEED_MEMORY:\t%ld\n",atomic64_read(&contiguity_failure[order][EXCEED_MEMORY]));
  seq_printf(m, "GUARD_PAGE:\t%ld\n",atomic64_read(&contiguity_failure[order][GUARD_PAGE]));
  seq_printf(m, "OCCUPIED:\t%ld\n",atomic64_read(&contiguity_failure[order][OCCUPIED]));
  seq_printf(m, "BUDDY_OCCUPIED:\t%ld\n",atomic64_read(&contiguity_failure[order][BUDDY_OCCUPIED]));
  seq_printf(m, "WRONG_ALIGNMENT:\t%ld\n",atomic64_read(&contiguity_failure[order][WRONG_ALIGNMENT]));
  seq_printf(m, "EXCEED_ZONE:\t%ld\n",atomic64_read(&contiguity_failure[order][EXCEED_ZONE]));
  seq_printf(m, "NOTHING_FOUND:\t%ld\n",atomic64_read(&contiguity_failure[order][NOTHING_FOUND]));
  seq_printf(m, "BUDDY_GUARD:\t%ld\n",atomic64_read(&contiguity_failure[order][BUDDY_GUARD]));
  seq_printf(m, "SUBBLOCK:\t%ld\n",atomic64_read(&contiguity_failure[order][SUBBLOCK_F]));
  seq_printf(m, "PAGECACHE:\t%ld\n",atomic64_read(&contiguity_failure[order][PAGECACHE_F]));
  seq_printf(m, "NUMA:\t%ld\n",atomic64_read(&contiguity_failure[order][NUMA_F]));

  return 0;
}

static int order_contiguity_success_show(struct seq_file *m,void *v)
{
  
  int order = *((int *) m->private);
  seq_printf(m, "SUPERBLOCK:\t%ld\n",atomic64_read(&contiguity_success[order][SUPERBLOCK]));
  seq_printf(m, "SUBBLOCK:\t%ld\n",atomic64_read(&contiguity_success[order][SUBBLOCK_S]));
  seq_printf(m, "EXACT:\t%ld\n",atomic64_read(&contiguity_success[order][EXACT]));
  seq_printf(m, "RESERVED:\t%ld\n",atomic64_read(&contiguity_success[order][RMM_RESERVED]));
  seq_printf(m, "PAGECACHE:\t%ld\n",atomic64_read(&contiguity_success[order][PAGECACHE]));
  return 0;
}

static int contiguity_failure_show(struct seq_file *m,void *v)
{

  int order=0;
  for (order=0; order<MAX_ORDER; order++){  
    seq_printf(m, "ORDER[%ld]\n",order);
    seq_printf(m, "\t\tINVALID_PFN:\t%ld\n",atomic64_read(&contiguity_failure[order][INVALID_PFN]));
    seq_printf(m, "\t\tEXCEED_MEMORY:\t%ld\n",atomic64_read(&contiguity_failure[order][EXCEED_MEMORY]));
    seq_printf(m, "\t\tGUARD_PAGE:\t%ld\n",atomic64_read(&contiguity_failure[order][GUARD_PAGE]));
    seq_printf(m, "\t\tOCCUPIED:\t%ld\n",atomic64_read(&contiguity_failure[order][OCCUPIED]));
    seq_printf(m, "\t\tBUDDY_OCCUPIED:\t%ld\n",atomic64_read(&contiguity_failure[order][BUDDY_OCCUPIED]));
    seq_printf(m, "\t\tWRONG_ALIGNMENT:\t%ld\n",atomic64_read(&contiguity_failure[order][WRONG_ALIGNMENT]));
    seq_printf(m, "\t\tEXCEED_ZONE:\t%ld\n",atomic64_read(&contiguity_failure[order][EXCEED_ZONE]));
    seq_printf(m, "\t\tNOTHING_FOUND:\t%ld\n",atomic64_read(&contiguity_failure[order][NOTHING_FOUND]));
    seq_printf(m, "\t\tBUDDY_GUARD:\t%ld\n",atomic64_read(&contiguity_failure[order][BUDDY_GUARD]));
    seq_printf(m, "\t\tSUBBLOCK:\t%ld\n",atomic64_read(&contiguity_failure[order][SUBBLOCK_F]));
    seq_printf(m, "\t\tPAGECACHE:\t%ld\n",atomic64_read(&contiguity_failure[order][PAGECACHE_F]));
    seq_printf(m, "\t\tNUMA:\t%ld\n",atomic64_read(&contiguity_failure[order][NUMA_F]));
  }

  seq_printf(m, "TOTAL\n");
  seq_printf(m, "\t\tINVALID_PFN:\t%ld\n",atomic64_read(&total_contiguity_failure[INVALID_PFN]));
  seq_printf(m, "\t\tEXCEED_MEMORY:\t%ld\n",atomic64_read(&total_contiguity_failure[EXCEED_MEMORY]));
  seq_printf(m, "\t\tGUARD_PAGE:\t%ld\n",atomic64_read(&total_contiguity_failure[GUARD_PAGE]));
  seq_printf(m, "\t\tOCCUPIED:\t%ld\n",atomic64_read(&total_contiguity_failure[OCCUPIED]));
  seq_printf(m, "\t\tBUDDY_OCCUPIED:\t%ld\n",atomic64_read(&total_contiguity_failure[BUDDY_OCCUPIED]));
  seq_printf(m, "\t\tWRONG_ALIGNMENT:\t%ld\n",atomic64_read(&total_contiguity_failure[WRONG_ALIGNMENT]));
  seq_printf(m, "\t\tEXCEED_ZONE:\t%ld\n",atomic64_read(&total_contiguity_failure[EXCEED_ZONE]));
  seq_printf(m, "\t\tNOTHING_FOUND:\t%ld\n",atomic64_read(&total_contiguity_failure[NOTHING_FOUND]));
  seq_printf(m, "\t\tBUDDY_GUARD:\t%ld\n",atomic64_read(&total_contiguity_failure[BUDDY_GUARD]));
  seq_printf(m, "\t\tSUBBLOCK:\t%ld\n",atomic64_read(&total_contiguity_failure[SUBBLOCK_F]));
  seq_printf(m, "\t\tPAGECACHE:\t%ld\n",atomic64_read(&total_contiguity_failure[PAGECACHE_F]));
  seq_printf(m, "\t\tNUMA:\t%ld\n",atomic64_read(&total_contiguity_failure[NUMA_F]));


  //seq_printf(m, "\n\n\t\tHEAP OCCUPIED BY HEAP SAME PROCESS SAME VMA:\t%ld\n",total_contiguity_heap_failures[ANON_SAME_PROCESS_SAME_VMA]);
  //seq_printf(m, "\n\n\t\tHEAP OCCUPIED BY HEAP SAME PROCESS DIFF VMA:\t%ld\n",total_contiguity_heap_failures[ANON_SAME_PROCESS_DIFFERENT_VMA]);
  //seq_printf(m, "\t\tHEAP OCCUPIED BY HEAP OTHER PROCESS:\t%ld\n",total_contiguity_heap_failures[ANON_OTHER_PROCESS]);
  //seq_printf(m, "\t\tHEAP OCCUPIED BY FILE:\t%ld\n",total_contiguity_heap_failures[NOT_ANON]);
  //seq_printf(m, "\t\tFILE OCCUPIED BY HEAP SAME PROCESS:\t%ld\n",total_contiguity_file_failures[ANON_SAME_PROCESS_SAME_VMA]);
  //seq_printf(m, "\t\tFILE OCCUPIED BY HEAP OTHER PROCESS:\t%ld\n",total_contiguity_file_failures[ANON_OTHER_PROCESS]);
  //seq_printf(m, "\t\tFILE OCCUPIED BY FILE:\t%ld\n",total_contiguity_file_failures[NOT_ANON]);


  //seq_printf(m, "\n\n\t\tKHUGEPAGED collapsed contig:\t%ld\n",total_khugepaged_contig);
  //seq_printf(m, "\t\tKHUGEPAGED collapsed non contig:\t%ld\n",total_khugepaged_non_contig);
  //seq_printf(m, "\t\tKHUGEPAGED contig alloc success:\t%ld\n",total_khugepaged_contig_alloc_success);


  //seq_printf(m, "\n\n\t\t4K cow:\t%ld\n",total_4k_cow);
  //seq_printf(m, "\t\tTHP cow:\t%ld\n",total_thp_cow);
  return 0;
}

static int contiguity_success_show(struct seq_file *m,void *v)
{
  
  int order=0;
  for (order=0; order<MAX_ORDER; order++){  
    seq_printf(m, "ORDER[%ld]\n",order);
    seq_printf(m, "\t\tSUPERBLOCK:\t%ld\n",atomic64_read(&contiguity_success[order][SUPERBLOCK]));
    seq_printf(m, "\t\tSUBBLOCK:\t%ld\n",atomic64_read(&contiguity_success[order][SUBBLOCK_S]));
    seq_printf(m, "\t\tEXACT:\t%ld\n",atomic64_read(&contiguity_success[order][EXACT]));
    seq_printf(m, "\t\tRESERVED:\t%ld\n",atomic64_read(&contiguity_success[order][RMM_RESERVED]));
    seq_printf(m, "\t\tPAGECACHE:\t%ld\n\n\n",atomic64_read(&contiguity_success[order][PAGECACHE]));
   }  
    
  seq_printf(m, "TOTAL\n");
  seq_printf(m, "\t\tSUPERBLOCK:\t%ld\n",atomic64_read(&total_contiguity_success[SUPERBLOCK]));
  seq_printf(m, "\t\tSUBBLOCK:\t%ld\n",atomic64_read(&total_contiguity_success[SUBBLOCK_S]));
  seq_printf(m, "\t\tEXACT:\t%ld\n",atomic64_read(&total_contiguity_success[EXACT]));
  seq_printf(m, "\t\tRESERVED:\t%ld\n",atomic64_read(&total_contiguity_success[RMM_RESERVED]));
  seq_printf(m, "\t\tPAGECACHE:\t%ld\n",atomic64_read(&total_contiguity_success[PAGECACHE]));

  return 0;
}

static int total_contig_alloc_show(struct seq_file *m,void *v){

  seq_printf(m, "%lld\n",total_alloc_hint);
  return 0;
}

static int reserved_pool_show(struct seq_file *m,void *v){

  seq_printf(m, "%lld\n",nr_pages_rmm_reserved);
  return 0;
}
static int order_contiguity_failure_open(struct inode *inode,struct file *file)
{
    return single_open(file, order_contiguity_failure_show, proc_get_parent_data(inode));
}

static int order_contiguity_success_open(struct inode *inode,struct file *file)
{
    return single_open(file, order_contiguity_success_show, proc_get_parent_data(inode));
}

static int contiguity_failure_open(struct inode *inode,struct file *file)
{
    return single_open(file, contiguity_failure_show, NULL);
}

static int contiguity_success_open(struct inode *inode,struct file *file)
{
    return single_open(file, contiguity_success_show, NULL);
}

static int total_contig_alloc_open(struct inode *inode,struct file *file)
{
    return single_open(file, total_contig_alloc_show, NULL);
}

static int reserved_pool_open(struct inode *inode,struct file *file)
{
    return single_open(file, reserved_pool_show, NULL);
}

static const struct file_operations order_contiguity_failure_fops = {
    .open = order_contiguity_failure_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static const struct file_operations order_contiguity_success_fops = {
    .open = order_contiguity_success_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static const struct file_operations contiguity_failure_fops = {
    .open = contiguity_failure_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static const struct file_operations contiguity_success_fops = {
    .open = contiguity_success_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static const struct file_operations total_contig_alloc_fops = {
    .open = total_contig_alloc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static const struct file_operations reserved_pool_fops = {
    .open = reserved_pool_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

/*
SYSCALL_DEFINE0(reset_contiguity)
{ 
  int i,j;

  for(i=0; i< NUM_CONTIG_FAILURES; i++){
    for (j=0; j<MAX_ORDER; j++) 
       atomic64_set(&contiguity_failure[j][i],0);

    atomic64_set(&total_contiguity_failure[i],0);
  }

  for(i=0; i< NUM_CONTIG_SUCCESS; i++){
    for (j=0; j<MAX_ORDER; j++) 
      atomic64_set(&contiguity_success[j][i],0);
    
    atomic64_set(&total_contiguity_success[i],0);
  }
  
  for(i=0; i< NUM_FAIL_STAT; i++){
    total_contiguity_heap_failures[i]=0;
    total_contiguity_file_failures[i]=0;
  }
  
  for (j=0; j<MAX_ORDER; j++){ 
     contiguity_bucket[j]=0;
     contiguity_fallback_max_order[j]=0;
     contiguity_failed_bucket[j]=0;
  }
  
 total_thp_cow=0;
 total_4k_cow=0;
 total_khugepaged_contig_alloc_success=0;
 total_khugepaged_contig=0;
 total_khugepaged_non_contig=0;
 total_alloc_hint=0;
 return 0;
}
*/

static int __init contiguity_init(void)
{
 
    int i;
    struct proc_dir_entry *contiguity_dir, *subdir;
    char buf[64];
    
    contiguity_dir = proc_mkdir_data("capaging", 0, NULL, NULL);
    proc_create("failure", 0, contiguity_dir, &contiguity_failure_fops);
    proc_create("success", 0, contiguity_dir, &contiguity_success_fops);
    proc_create("total_alloc", 0, contiguity_dir, &total_contig_alloc_fops);

    for(i=0; i<MAX_ORDER; i++){
      order[i]=i;
      sprintf(buf,"%i", i);
      subdir = proc_mkdir_data(buf,0,contiguity_dir, order+i);
      proc_create("failure", 0, subdir, &order_contiguity_failure_fops);
      proc_create("success", 0, subdir, &order_contiguity_success_fops);
    }


    return 0;
}

fs_initcall(contiguity_init);
