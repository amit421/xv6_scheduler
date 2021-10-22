#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"

int main(int argv,char *args[])
{
	int prev;
	cps();
	if(argv==3)
	{
		prev=set_priority(atoi(args[1]),atoi(args[2]));
		printf(1,"pid = %d old = %d new = %d\n",atoi(args[1]),prev,atoi(args[2]));
		cps();
	}
	exit();
}
