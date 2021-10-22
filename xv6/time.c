#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"


int main (int argc,char *argv[])
{
	int pid;
	int a=3,b=4;	
	pid = fork ();
	if(pid == 0)
	{	
		if(exec(argv[1],&argv[1])<0)
			printf(1, "exec %s failed\n", argv[1]);
	}
	else
		waitx(&a,&b);
	printf(1, " Wait Time = %d\n ticks Run Time = %d\n",a,b); 
	exit();
}
