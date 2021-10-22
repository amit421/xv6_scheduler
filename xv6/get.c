#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"


struct proc_stat
{
	int pid;
	int runtime;
	int num_run;
	int current_queue;
	int ticks[5];
};

int main (int argc,char *argv[])
{
	struct proc_stat process_status;
	process_status.pid=atoi(argv[1]);
	getpinfo(&process_status);
	if(process_status.current_queue != -1)
	{
		printf(1,"Pid	Runtime(in ticks)	Num_run		Current_queue	Ticks\n");
		printf(1,"%d	%d			%d			%d	",process_status.pid,process_status.runtime,process_status.num_run,process_status.current_queue);
		printf(1,"%d\n",process_status.ticks[0]);
		printf(1,"								%d\n",process_status.ticks[1]);	
		printf(1,"								%d\n",process_status.ticks[2]);	
		printf(1,"								%d\n",process_status.ticks[3]);	
		printf(1,"								%d\n",process_status.ticks[4]);	
	}
	else
		printf(1,"Pid does not exist\n");
	exit();
}
