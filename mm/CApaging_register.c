#include<linux/types.h>
#include<linux/utsname.h>
#include<linux/syscalls.h>
#include <linux/contiguity.h>

char capaging_process[CONFIG_NR_CPUS][MAX_PROC_NAME_LEN];


SYSCALL_DEFINE3(capaging_register, const char __user**, proc_name, unsigned int, num_procs, int, option)
{
    unsigned int i;
    char *temp;
    char proc[MAX_PROC_NAME_LEN];
    unsigned long ret = 0;

    struct task_struct *tsk;
    unsigned long pid;

    if ( option > 0 ) {
        for ( i = 0 ; i < CONFIG_NR_CPUS ; i++ ) {
            if ( i < num_procs ){
              stac();
              ret = strncpy_from_user(proc, proc_name[i], MAX_PROC_NAME_LEN);
              clac();
            }
            else
                temp = strncpy(proc,"",MAX_PROC_NAME_LEN);
            temp = strncpy(capaging_process[i], proc, MAX_PROC_NAME_LEN-1);
        }
    }

    if ( option < 0 ) {
        for ( i = 0 ; i < CONFIG_NR_CPUS ; i++ ) {
            if( i < num_procs ) {
                stac();
                ret = kstrtoul(proc_name[i],10,&pid);
                clac();
                if ( ret == 0 ) {
                    tsk = find_task_by_vpid(pid);
                    tsk->mm->capaging = 1;
                }
            }
        }
    }
    return 0;
}

int capaging_unregister(const char* proc_name)
{
    unsigned int i;
    char proc[MAX_PROC_NAME_LEN];
    char *temp;
    
    temp = strncpy(proc,"",MAX_PROC_NAME_LEN);

    for ( i = 0 ; i < CONFIG_NR_CPUS ; i++ )     {
        if (!strncmp(proc_name, capaging_process[i], MAX_PROC_NAME_LEN))
            temp = strncpy(capaging_process[i], proc, MAX_PROC_NAME_LEN-1);           
    }
    return 0;
}

int is_capaging_process(const char* proc_name)
{
    unsigned int i;

    for ( i = 0 ; i < CONFIG_NR_CPUS ; i++ )     {
        if (!strncmp(proc_name, capaging_process[i], MAX_PROC_NAME_LEN))
            return 1;
    }
    return 0;
}
