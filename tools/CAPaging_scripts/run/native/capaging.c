#include <stdio.h>
#include <string.h>
#include <stdlib.h> 
#include <sys/types.h>
#include <unistd.h> 
#include <libgen.h>
#include <sys/wait.h>

#define CAPAGING_SYSCALL 337
void main (int argc, char* argv[])
{
	pid_t pid;
	int status;
	int ret = 0;
        char *process_names[1];
	
	if(argc < 2)
	{
		printf("Usage: ./capaging <name | pid | command | help> {arguments}\n");
		printf("Example1: ./capaging command ls\n");
		printf("Example2: ./capaging command ./test <arguments>\n");
		printf("Example3: ./capaging pid 1124\n");
		return;
	}

	if(strcmp(argv[1],"command")==0)
	{
		if(argc < 3)
		{
			printf("Please provide a command to run\n");
			return;
		}
	  process_names[0] = basename(argv[2]);
	  ret = syscall(CAPAGING_SYSCALL,process_names,1,1);
	  switch ((pid = fork()))
	  {
	  case -1:
	          perror ("Fork Failed !!");
	          break;
	  case 0:
	          execvp(argv[2], &argv[2]);
	          exit(0);
	          break;
	  default:
	          wait(&status);
	          break;
	  }
	}
	else if (strcmp(argv[1],"name")==0)
	{
		ret = syscall(CAPAGING_SYSCALL,&argv[2],argc-2,1);
	}
	else if (strcmp(argv[1],"pid")==0)
	{
		ret = syscall(CAPAGING_SYSCALL,&argv[2],argc-2,-1);
	}
	else if((strcmp(argv[1],"help")==0))
	{
		printf("Usage: ./capaging <name | pid | command | help> {arguments}\n");
		printf("Example1: ./capaging command ls\n");
		printf("Example2: ./capaging command ./test <arguments>\n");
		printf("Example3: ./capaging pid 1124\n");
    return;
	}
	else
	{
		printf("Cannot run command provided. Run ./capaging help for more information\n");
		return;
	}
    return;
}
