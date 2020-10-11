#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/hugetlb.h>
#include <linux/mman.h>
#include <linux/mmzone.h>
#include <linux/proc_fs.h>
#include <linux/quicklist.h>
#include <linux/seq_file.h>
#include <linux/swap.h>
#include <linux/vmstat.h>
#include <linux/atomic.h>
#include <linux/vmalloc.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include "internal.h"
#include <linux/highmem.h>
#include <linux/kvm_host.h>


static struct proc_dir_entry *proc_dir_VMI;

//list of qemu process/VMs monitored
struct list_head *list_of_monitored_qp;

//struct for qemu process/VM
struct monitored_qp{
  pid_t tgid;
  struct proc_dir_entry *pde; // proc directory
  struct list_head *list_of_monitored_tasks; //list of guest tasks monitored
  struct list_head list; // to be added to list of VM monitored
};

//struct for guest task monitored
struct VMI_guest_task_monitored {
  struct kvm_vcpu *vcpu; 
  struct mm_struct *guest_mm; //guest mm struct pointer in the GPA
  pid_t guest_pid; // guest task pid
  char  comm[TASK_COMM_LEN]; // comm
  struct proc_dir_entry *pde;  // guest task proc dir
  struct list_head list; // attached to monitored_qp list of monitored tasks 
};

//helper functions 
int VMI_print_guest_task_map(struct seq_file *p, void *v);
int VMI_print_guest_task_comm(struct seq_file *p, void *v);
int VMI_shadow_page_table_walk(struct kvm_vcpu *vcpu, gpa_t gpa, hfn_t *pfn, pteval_t *flags, int *level);
int VMI_guest_page_table_walk(struct kvm_vcpu *vcpu, gva_t gva, u64 cr3, gfn_t *gfn, pteval_t *flags, int *level);


// given a gVA get the gPA (similar to native /proc/<pid>/pagemap)
static ssize_t VMI_pagemap_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
  unsigned long src;
  unsigned long svpfn;
  unsigned long vaddr;
  struct VMI_guest_task_monitored *vmt;
  struct kvm_vcpu *vcpu;

  struct page *tmp_page;
  u64 tmp_addr;

  char buffer[256];
  pteval_t flags;
  int level;
  pteval_t spte_flags;
  int spte_level;
  int ret;

  hfn_t task_pfn, mm_pfn;
  struct page *task_page, *mm_page;
  struct task_struct *task;
  struct mm_struct *mm;
  pgd_t *pgd;
  u64 task_addr, mm_addr;

  kvm_pfn_t gfn;
  kvm_pfn_t pfn;
 
  vmt = (struct VMI_guest_task_monitored*) PDE_DATA(file_inode(file));
  vcpu = vmt->vcpu;
  src  = *ppos;
  svpfn = src >> PAGE_SHIFT;
  vaddr = svpfn << PAGE_SHIFT;

  /*
  ret = VMI_shadow_page_table_walk(vcpu, __pa(vmt->guest_task), &task_pfn, &flags, &level);
  if(ret)
    goto out;

  task_page = pfn_to_page(task_pfn);
  task_addr = kmap_atomic(task_page);
  task = (struct task_struct*)(task_addr | (__pa(vmt->guest_task)&(~PAGE_MASK)));
  mm = task->mm;  
  kunmap_atomic(task_addr);
  */

  ret = VMI_shadow_page_table_walk(vcpu, __pa(vmt->guest_mm), &mm_pfn, &flags, &level);
  if(ret){
    goto out;
  }
  
  mm_page = pfn_to_page(mm_pfn);
  mm_addr = kmap_atomic(mm_page);
  mm = (struct mm_struct*)(mm_addr | (__pa(vmt->guest_mm) &(~PAGE_MASK))); 
  pgd = mm->pgd;
  kunmap_atomic(mm_addr);

  ret = VMI_guest_page_table_walk(vcpu, vaddr, __pa(pgd), &gfn, &flags, &level);
  if(ret){ 
    goto guest_unmapped;
  }
  
  
  ret = VMI_shadow_page_table_walk(vcpu, gfn<<PAGE_SHIFT, &pfn, &spte_flags, &spte_level);
  if(ret){
    goto host_unmapped;
  }

  sprintf(buffer, "GFN:0x%llx\tGFLAGS:0x%llx\tGLEVEL:0x%x\tPFN:0x%llx\tHFLAGS:0x%llx\tHLEVEL:0x%x\n", gfn, flags, level, pfn, spte_flags, spte_level);

  if (copy_to_user(buf, buffer, sizeof(buffer))) {
    ret = -EFAULT;
    goto out;
  }
  return 0;

guest_unmapped:
  sprintf(buffer, "GFN:0xFFFFFFFF\tGFLAGS:0\tGLEVEL:0\tPFN:0\tHFLAGS:0\tHLEVEL:0\n");

  if (copy_to_user(buf, buffer, sizeof(buffer))) {
    ret = -EFAULT;
    goto out;
  }
  return 0;

host_unmapped:
  sprintf(buffer, "GFN:0x%llx\tGFLAGS:0x%llx\tGLEVEL:0x%llx\tPFN:0xFFFFFFFF\tHFLAGS:0\tHLEVEL:0\n", gfn, flags, level);
  
  if (copy_to_user(buf, buffer, sizeof(buffer))) {
    ret = -EFAULT;
    goto out;
  }
  return 0;

out:
  return -1;
}


static int VMI_pagemap_open(struct inode *inode, struct file *file)
{
    return 0;
}

static const struct file_operations proc_VMI_pagemap_operations = {
  .llseek = mem_lseek, 
  .read   = VMI_pagemap_read,
  .open   = VMI_pagemap_open,
};


// print the gVA map of the guest task (vmas, similar to native /proc/<pid>/maps)
static int proc_VMI_map_open(struct inode *inode, struct file *file)
{
  return single_open(file, VMI_print_guest_task_map, PDE_DATA(inode));
}

static const struct file_operations proc_VMI_map_fops = {
  .open = proc_VMI_map_open,
  .read = seq_read,
  .llseek = seq_lseek,
  .release = single_release 
};

// print the command name of the guest task
static int proc_VMI_comm_open(struct inode *inode, struct file *file)
{
  return single_open(file, VMI_print_guest_task_comm, PDE_DATA(inode));
}

static const struct file_operations proc_VMI_comm_fops = {
  .open = proc_VMI_comm_open,
  .read = seq_read,
  .llseek = seq_lseek,
  .release = single_release 
};

// create /proc/VMI/<qemu_pid>/<guest_task_comm> directory
// create maps, pagemap, comm intefaces and register the corresponding fops
struct proc_dir_entry* VMI_create_guest_task_dir(struct VMI_guest_task_monitored *vmt, struct proc_dir_entry *vm_pde){
  
  struct proc_dir_entry *pde;
  struct proc_dir_entry *pde_map;
  struct proc_dir_entry *pde_pagemap;
  struct proc_dir_entry *pde_comm;
  char buf[512];

  sprintf(buf, "%d", vmt->guest_pid);

  pde = proc_mkdir_data(buf, 0, vm_pde, NULL);
  if(!pde)
    return -ENOMEM;
 
  // guest task gVA map (vmas, similar to native /proc/<pid>/maps)
  pde_map = proc_create_data("maps", 0, pde, &proc_VMI_map_fops, vmt);
  if(!pde)
    return -ENOMEM;
  
  // given a gVA get the gPA (similar to native /proc/<pid>/pagemap)
  pde_pagemap = proc_create_data("pagemap",  S_IFREG | S_IRUGO, pde, &proc_VMI_pagemap_operations, vmt);
  if(!pde_pagemap)
    return -ENOMEM;

  // the guest task comm
  pde_comm = proc_create_data("comm",  0, pde, &proc_VMI_comm_fops, vmt);
  if(!pde_comm)
    return -ENOMEM;
  
  return pde;
}

//create the /proc/VMI/ directory
//initialize the empty list of monitored qemu processes/VMs
static int __init VMI_init(void){

  proc_dir_VMI = proc_mkdir_data("VMI", 0, NULL, NULL);
  if(!proc_dir_VMI) 
    return -ENOMEM;

  list_of_monitored_qp = (struct list_head *) kmalloc (sizeof(struct list_head), GFP_KERNEL);
  INIT_LIST_HEAD(list_of_monitored_qp);

  return 0;
}

fs_initcall(VMI_init);

// Register guest task for monitoring
// called by kvm hypercall routine (KVM_VMI_REGISTER_TASK) 
int VMI_register_guest_task(struct kvm_vcpu *vcpu, struct task_struct *guest_task){

  hfn_t task_pfn, mm_pfn, pgd_pfn;
  struct page *task_page, *mm_page, *pgd_page;
  struct task_struct *task;
  struct mm_struct *mm;
  pgd_t *pgd;
  u64 task_addr, mm_addr, pgd_addr;
  int ret;
  char buf[512];
  struct monitored_qp *mvm = NULL;
  struct proc_dir_entry *mvm_pde = NULL;

  // allocate a struct to represent the guest task monitored
  struct VMI_guest_task_monitored *vmt = (struct VMI_guest_task_monitored *) kmalloc (sizeof(struct VMI_guest_task_monitored), GFP_KERNEL);
  pteval_t flags;
  int level;

  vmt->vcpu = vcpu;
 
  ret = VMI_shadow_page_table_walk(vcpu, __pa(guest_task), &task_pfn, &flags, &level);
  if(ret) {
    goto out;
  }

  task_page = pfn_to_page(task_pfn);
  task_addr = kmap_atomic(task_page);
  task = (struct task_struct*)(task_addr | (__pa(guest_task)&(~PAGE_MASK)));
  vmt->guest_pid = task->pid;
  vmt->guest_mm = task->mm;
  sprintf(vmt->comm, "%s", task->comm);
  kunmap_atomic(task_addr);


  // see if this guest task belongs to an already monitored VM/qemu process
  list_for_each_entry(mvm, list_of_monitored_qp, list){
    if(mvm->tgid == current->tgid){
      mvm_pde=mvm->pde;
      break; 
    }
  }
  
  // if not create a new monitored qemu process/VM struct and a corresponding /proc/VMI/<qemu_pid> directory 
  if(!mvm_pde){
    mvm = (struct monitored_qp *) kmalloc (sizeof(struct monitored_qp), GFP_KERNEL);
    mvm->tgid = current->tgid;
    mvm->list_of_monitored_tasks = (struct list_head *) kmalloc (sizeof(struct list_head), GFP_KERNEL);
    INIT_LIST_HEAD(mvm->list_of_monitored_tasks);
    sprintf(buf, "%u", current->tgid);
    mvm_pde = proc_mkdir_data(buf, 0, proc_dir_VMI, NULL);
    if(!mvm_pde){
      pr_crit("Could not create /proc/VMI/<qemu_pid>/ directory\n");
      return -ENOMEM;
    }
    mvm->pde = mvm_pde;
    list_add(&mvm->list, list_of_monitored_qp);
  }

  vmt->pde = VMI_create_guest_task_dir(vmt, mvm_pde);
  list_add(&vmt->list, mvm->list_of_monitored_tasks);

out:
  return -1;
}
EXPORT_SYMBOL(VMI_register_guest_task);


int VMI_remove_guest_task(struct kvm_vcpu *vcpu, struct mm_struct *mm){

  struct monitored_qp *mvm = NULL;
  struct VMI_guest_task_monitored *vmt = NULL;
  struct list_head * list_of_monitored_tasks = NULL;
  pid_t vm_tgid = current->tgid;

  list_for_each_entry(mvm, list_of_monitored_qp, list){
    if(mvm->tgid == current->tgid){
      list_of_monitored_tasks = mvm->list_of_monitored_tasks;
      break;
    }
  }
  
  if(list_of_monitored_tasks){
    list_for_each_entry(vmt, list_of_monitored_tasks, list){
		if(vmt->guest_mm == mm){
        	proc_remove(vmt->pde);
        	list_del(&vmt->list);
        	kfree(vmt);
       	 	break;
      	}
    }  
  }
  
  return 0;
}
EXPORT_SYMBOL(VMI_remove_guest_task);

//print the guest task GVA map
int VMI_print_guest_task_map(struct seq_file *p, void *v){

  hfn_t mm_pfn, vma_pfn, task_pfn;
  struct page *mm_page, *vma_page, *task_page;
  u64 mm_addr, vma_addr, task_addr;
  struct mm_struct *mm;
  struct vm_area_struct *vma;
  struct task_struct *task;
  int ret;
  struct VMI_guest_task_monitored *vmt = p->private;
  struct kvm_vcpu *vcpu = vmt->vcpu;


  pteval_t flags;
  int level;

  /*
  ret = VMI_shadow_page_table_walk(vcpu, __pa(vmt->guest_task), &task_pfn, &flags, &level);
  if(ret)
    goto out;

  task_page = pfn_to_page(task_pfn);
  task_addr = kmap_atomic(task_page);
  task = (struct task_struct*)(task_addr | (__pa(vmt->guest_task)&(~PAGE_MASK)));
  mm = task->mm;  
  kunmap_atomic(task_addr);
  */
 
  ret = VMI_shadow_page_table_walk(vcpu, __pa(vmt->guest_mm), &mm_pfn, &flags, &level);
  if(ret){
    goto out;
  }
  mm_page = pfn_to_page(mm_pfn);
  mm_addr = kmap_atomic(mm_page);
  mm = (struct mm_struct*)(mm_addr | (__pa(vmt->guest_mm) &(~PAGE_MASK))); 
  vma = mm->mmap;
  kunmap_atomic(mm_addr);

  while(vma){

    ret = VMI_shadow_page_table_walk(vcpu, __pa(vma), &vma_pfn, &flags, &level);
    if(ret) {
      	goto out;
	}

    vma_page = pfn_to_page(vma_pfn);
    vma_addr = kmap_atomic(vma_page);
    vma = (struct vm_area_struct*)(vma_addr | (__pa(vma)&(~PAGE_MASK)));
    seq_printf(p, "0x%llx-0x%llx\n",vma->vm_start, vma->vm_end);
    vma = vma->vm_next;
    kunmap_atomic(vma_addr);  
  }

out:
  return 0;
}
EXPORT_SYMBOL_GPL(VMI_print_guest_task_map);


//print the guest task comm
int VMI_print_guest_task_comm(struct seq_file *p, void *v){

  struct VMI_guest_task_monitored *vmt = p->private;
  seq_printf(p, "%s\n", vmt->comm);

out:
  return 0;
}
EXPORT_SYMBOL_GPL(VMI_print_guest_task_comm);
