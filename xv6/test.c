#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int main(int argc,char *argv[])
{
	//set_priority(3,20);
	int a=0,b=0,k,n,id;
	float x=0;
	int z,d;

	if(argc<2)
		n=1; // default value
	else
		n=atoi(argv[1]); // form command line
	if(n<0||n>20)
		n=2;
	if(argc<3)
		d=1;
	else
		d=atoi(argv[2]);
	x=0;
	id=0;
	for (k = 0; k < n; ++k)
	{
		id=fork();
		if(id>0)
		{
			printf(1,"parent %d created child %d\n",getpid(),id );
			set_priority(id,100-id);
		}
		else
		{
			cps();
			for (z = 0; z < 1000000000; z+=d)
				x=x+3.14*4398;
			exit();
		}
	}
	for (int i=0;i<n;++i)
	{
		//cps();
		waitx(&a,&b);
		//printf(1,"%d %d\n",a,b);
	}
	exit();
}
