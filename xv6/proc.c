#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct proc* q0[100];
struct proc* q1[100];
struct proc* q2[100];
struct proc* q3[100];
struct proc* q4[100];
int c0=-1;
int c1=-1;
int c2=-1;
int c3=-1;
int c4=-1;
struct pstat total_stat;

struct 
{
	struct spinlock lock;
	struct proc proc[NPROC];
}ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

	void
pinit(void)
{
	initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
	return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
	struct cpu*
mycpu(void)
{
	int apicid, i;

	if(readeflags()&FL_IF)
		panic("mycpu called with interrupts enabled\n");

	apicid = lapicid();
	// APIC IDs are not guaranteed to be contiguous. Maybe we should have
	// a reverse map, or reserve a register to store &cpus[i].
	for (i = 0; i < ncpu; ++i) {
		if (cpus[i].apicid == apicid)
			return &cpus[i];
	}
	panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
	struct cpu *c;
	struct proc *p;
	pushcli();
	c = mycpu();
	p = c->proc;
	popcli();
	return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
	static struct proc*
allocproc(void)
{
	struct proc *p;
	char *sp;

	acquire(&ptable.lock);

	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
		if(p->state == UNUSED)
			goto found;
	release(&ptable.lock);
	return 0;

found:
	p->state = EMBRYO;
	p->pid = nextpid++;
	p->priority=60;
	total_stat.inuse[p->pid] = 1;
	p->pri = 0;
	p->clicks = 0;
	c0++;
	q0[c0] = p;
	total_stat.current_queue[p->pid] = 0;
	total_stat.ticks[p->pid][0] = 0;
	total_stat.ticks[p->pid][1] = 0;
	total_stat.ticks[p->pid][2] = 0;
	total_stat.ticks[p->pid][3] = 0;
	total_stat.ticks[p->pid][4] = 0;
	total_stat.pid[p->pid] = p->pid;
	total_stat.runtime[p->pid] = 0;
	total_stat.num_run[p->pid] = 0;
	total_stat.last[p->pid] = 0;
	release(&ptable.lock);

	// Allocate kernel stack.
	if((p->kstack = kalloc()) == 0)
	{
		p->state = UNUSED;
		return 0;
	}
	sp = p->kstack + KSTACKSIZE;

	// Leave room for trap frame.
	sp -= sizeof *p->tf;
	p->tf = (struct trapframe*)sp;

	// Set up new context to start executing at forkret,
	// which returns to trapret.
	sp -= 4;
	*(uint*)sp = (uint)trapret;

	sp -= sizeof *p->context;
	p->context = (struct context*)sp;
	memset(p->context, 0, sizeof *p->context);
	p->context->eip = (uint)forkret;
	p->ctime=ticks;
	p->etime=0;
	p->rtime=0;
	return p;
}

//PAGEBREAK: 32
// Set up first user process.
	void
userinit(void)
{
	struct proc *p;
	extern char _binary_initcode_start[], _binary_initcode_size[];

	p = allocproc();

	initproc = p;
	if((p->pgdir = setupkvm()) == 0)
		panic("userinit: out of memory?");
	inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
	p->sz = PGSIZE;
	memset(p->tf, 0, sizeof(*p->tf));
	p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
	p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
	p->tf->es = p->tf->ds;
	p->tf->ss = p->tf->ds;
	p->tf->eflags = FL_IF;
	p->tf->esp = PGSIZE;
	p->tf->eip = 0;  // beginning of initcode.S

	safestrcpy(p->name, "initcode", sizeof(p->name));
	p->cwd = namei("/");

	// this assignment to p->state lets other cores
	// run this process. the acquire forces the above
	// writes to be visible, and the lock is also needed
	// because the assignment might not be atomic.
	acquire(&ptable.lock);

	p->state = RUNNABLE;

	release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
	int
growproc(int n)
{
	uint sz;
	struct proc *curproc = myproc();

	sz = curproc->sz;
	if(n > 0){
		if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
			return -1;
	} else if(n < 0){
		if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
			return -1;
	}
	curproc->sz = sz;
	switchuvm(curproc);
	return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
	int
fork(void)
{
	int i, pid;
	struct proc *np;
	struct proc *curproc = myproc();

	// Allocate process.
	if((np = allocproc()) == 0){
		return -1;
	}

	// Copy process state from proc.
	if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
		kfree(np->kstack);
		np->kstack = 0;
		np->state = UNUSED;
		return -1;
	}
	np->sz = curproc->sz;
	np->parent = curproc;
	*np->tf = *curproc->tf;

	// Clear %eax so that fork returns 0 in the child.
	np->tf->eax = 0;

	for(i = 0; i < NOFILE; i++)
		if(curproc->ofile[i])
			np->ofile[i] = filedup(curproc->ofile[i]);
	np->cwd = idup(curproc->cwd);

	safestrcpy(np->name, curproc->name, sizeof(curproc->name));

	pid = np->pid;

	acquire(&ptable.lock);

	np->state = RUNNABLE;

	release(&ptable.lock);

	return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
	void
exit(void)
{
	struct proc *curproc = myproc();
	struct proc *p;
	int fd;

	if(curproc == initproc)
		panic("init exiting");

	// Close all open files.
	for(fd = 0; fd < NOFILE; fd++){
		if(curproc->ofile[fd]){
			fileclose(curproc->ofile[fd]);
			curproc->ofile[fd] = 0;
		}
	}

	begin_op();
	iput(curproc->cwd);
	end_op();
	curproc->cwd = 0;

	acquire(&ptable.lock);

	// Parent might be sleeping in wait().
	wakeup1(curproc->parent);

	// Pass abandoned children to init.
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
		if(p->parent == curproc){
			p->parent = initproc;
			if(p->state == ZOMBIE)
				wakeup1(initproc);
		}
	}

	// Jump into the scheduler, never to return.
	curproc->state = ZOMBIE;
	curproc->etime = ticks;
	sched();
	panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
	int
wait(void)
{
	struct proc *p;
	int havekids, pid;
	struct proc *curproc = myproc();

	acquire(&ptable.lock);
	for(;;){
		// Scan through table looking for exited children.
		havekids = 0;
		for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
			if(p->parent != curproc)
				continue;
			havekids = 1;
			if(p->state == ZOMBIE){
				// Found one.
				pid = p->pid;
				kfree(p->kstack);
				p->kstack = 0;
				freevm(p->pgdir);
				p->pid = 0;
				p->parent = 0;
				p->name[0] = 0;
				p->killed = 0;
				p->state = UNUSED;
				p->ctime=0;
				p->etime=0;
				p->rtime=0;
				release(&ptable.lock);
				return pid;
			}
		}

		// No point waiting if we don't have any children.
		if(!havekids || curproc->killed){
			release(&ptable.lock);
			return -1;
		}

		// Wait for children to exit.  (See wakeup1 call in proc_exit.)
		sleep(curproc, &ptable.lock);  //DOC: wait-sleep
	}
}

	int
set_priority(int ppiidd,int value)
{
	struct proc *p;
	acquire(&ptable.lock);
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
	{
		if(p->pid==ppiidd)
		{
			int var=p->priority;
			p->priority=value;
			release(&ptable.lock);
			return var;
		}
	}
	release(&ptable.lock);
	return -1;
}

	int
cps()
{
	struct proc *p;
	sti();
	acquire(&ptable.lock);
	cprintf("Ctime	Name	PID	State		Priority\n");
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
	{
		if(p->pid!=0)
		{
			if(p->state == RUNNABLE)
				cprintf("%d	%s	%d	RUNNABLE	%d\n",p->ctime,p->name,p->pid,p->priority);
			else if(p->state == RUNNING)
				cprintf("%d	%s	%d	RUNNING		%d\n",p->ctime,p->name,p->pid,p->priority);
			else if(p->state == SLEEPING)
				cprintf("%d	%s	%d	SLEEPING	%d\n",p->ctime,p->name,p->pid,p->priority);
		}
	}
	release(&ptable.lock);
	return 0;
}

	int
getpinfo(struct proc_stat* process_status)
{
	struct proc* p;
	acquire(&ptable.lock);
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
	{
		if(p->pid == process_status->pid)
		{
			process_status->runtime=p->rtime;
			process_status->num_run = total_stat.num_run[p->pid];
			process_status->current_queue = total_stat.current_queue[p->pid];
			for(int h=0;h<5;h++)
				process_status->ticks[h]=total_stat.ticks[p->pid][h];
			release(&ptable.lock);
			return 0;
		}
	}
	process_status->current_queue=-1;
	release(&ptable.lock);
	return -1;
}



	int
waitx(int *wtime,int *rtime)
{
	struct proc *p;
	int havekids, pid;
	struct proc *curproc = myproc();
	acquire(&ptable.lock);
	for(;;){
		// Scan through table looking for zombie children.
		havekids = 0;
		for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
			if(p->parent != curproc)
				continue;
			havekids = 1;
			if(p->state == ZOMBIE){
				// Found one.
				*wtime = p->etime - p->ctime - p->rtime;
				*rtime = p->rtime;
				p->ctime=0;
				p->etime=0;
				p->rtime=0;
				pid = p->pid;
				kfree(p->kstack);
				p->kstack = 0;
				freevm(p->pgdir);
				p->state = UNUSED;
				p->pid = 0;
				p->parent = 0;
				p->name[0] = 0;
				p->killed = 0;
				release(&ptable.lock);
				return pid;
			}
		}

		// No point waiting if we don't have any children.
		if(!havekids || curproc->killed){
			release(&ptable.lock);
			return -1;
		}

		// Wait for children to exit.  (See wakeup1 call in proc_exit.)
		sleep(curproc, &ptable.lock);  //DOC: wait-sleep
	}
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
	void
scheduler(void)
{
	struct cpu *c = mycpu();
	c->proc = 0;
	for(;;)
	{
		// Enable interrupts on this processor.
		sti();

		acquire(&ptable.lock);
		// Loop over process table looking for process to run.

#ifdef DEFAULT
		struct proc *p;
		for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
		{
			if(p->state != RUNNABLE)
				continue;
			c->proc = p;
			switchuvm(p);
			p->state = RUNNING;
			swtch(&(c->scheduler), p->context);
			switchkvm();
			c->proc = 0;
		}

#else

#ifdef FCFS
		struct proc *p;
		struct proc *minP = 0;
		for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
		{
			if(p->state == RUNNABLE)
			{
				if (minP!=0)
				{
					if(p->ctime < minP->ctime)
						minP = p;
				}
				else
					minP = p;
			}
		}
		if(minP!=0)
		{
			p=minP;
			c->proc=p;
			switchuvm(p);
			p->state=RUNNING;
			swtch(&(c->scheduler), p->context);
			switchkvm();
			c->proc=0;
		}

#else

#ifdef PBS
		struct proc *highp = 0;
		struct proc *p;
		for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
		{

			if(p->state == RUNNABLE)
			{
				if(highp !=0)
				{
					if(highp->priority > p->priority)
						highp=p;
				}
				else
					highp = p;
			}
		}
		if(highp !=0)
		{
			p=highp;
			c->proc = p;
			switchuvm(p);
			p->state = RUNNING;
			swtch(&(c->scheduler), p->context);
			switchkvm();
			c->proc = 0;
		}

#else

#ifdef MLFQ
		struct proc *p = 0;
		int mo=-1;
		long long current_time = 0;
		for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
		{
			if(current_time - total_stat.last[p->pid] >= 120)
			{
				if(total_stat.current_queue[p->pid] != 0 && p->state == RUNNABLE)
				{
					int g=total_stat.current_queue[p->pid];
					if(g==1)
					{
						c0++;
						p->pri=0;
						total_stat.current_queue[p->pid] = p->pri;
						total_stat.last[p->pid]=current_time;
						p->clicks=0;
						q0[c0]=p;
						int u;
						for(u=0;u<=c1;u++)
						{
							if(q1[u]->pid == p->pid)
								break;
						}
						q1[u]=0;
						for(int j=u;j<=c1-1;j++)
							q1[j] = q1[j+1];
						q1[c1] = 0;
						c1--;
					}
					else if(g==2)
					{
						c1++;
						p->pri=1;
						total_stat.current_queue[p->pid] = p->pri;
						total_stat.last[p->pid]=current_time;
						p->clicks=0;
						q1[c1]=p;
						int u;
						for(u=0;u<=c2;u++)
						{
							if(q2[u]->pid == p->pid)
								break;
						}
						q2[u]=0;
						for(int j=u;j<=c2-1;j++)
							q2[j] = q2[j+1];
						q2[c2] = 0;
						c2--;
					}
					else if(g==3)
					{
						c2++;
						p->pri=2;
						total_stat.current_queue[p->pid] = p->pri;
						total_stat.last[p->pid]=current_time;
						p->clicks=0;
						q2[c2]=p;
						int u;
						for(u=0;u<=c3;u++)
						{
							if(q3[u]->pid == p->pid)
								break;
						}
						q3[u]=0;
						for(int j=u;j<=c3-1;j++)
							q3[j] = q3[j+1];
						q3[c3] = 0;
						c3--;
					}
					else if(g==4)
					{
						c3++;
						p->pri=3;
						total_stat.current_queue[p->pid] = p->pri;
						total_stat.last[p->pid]=current_time;
						p->clicks=0;
						q3[c3]=p;
						int u;
						for(u=0;u<=c4;u++)
						{
							if(q4[u]->pid == p->pid)
								break;
						}
						q4[u]=0;
						for(int j=u;j<=c4-1;j++)
							q4[j] = q4[j+1];
						q4[c4] = 0;
						c4--;
					}
				}
			}
		}
		if(c0!=-1)
		{
			for(int i=0;i<=c0;i++)
			{
				if(q0[i]->state != RUNNABLE)
					continue;
				p=q0[i];
				c->proc=q0[i];
				total_stat.num_run[p->pid]++;
				total_stat.last[p->pid] = current_time;
				current_time++;
				p->clicks++;
				switchuvm(p);
				p->state = RUNNING;
				swtch(&(c->scheduler), p->context);
				switchkvm();
				total_stat.ticks[p->pid][0]++;
				if(p->clicks ==1)
				{
					c1++;
					p->pri=1;
					total_stat.current_queue[p->pid] = p->pri;
					p->clicks=0;
					q1[c1]=p;
					q0[i]=0;
					for(int j=i;j<=c0-1;j++)
						q0[j] = q0[j+1];
					q0[c0] = 0;
					c0--;
				}
				c->proc = 0;
				mo=0;
				break;
			}
		}
		if(mo==0)
		{
			mo=-1;
			release(&ptable.lock);
			continue;
		}
		if(c1!=-1)
		{
			for(int i=0;i<=c1;i++)
			{
				if(q1[i]->state != RUNNABLE)
					continue;
				p=q1[i];
				c->proc=q1[i];
				total_stat.num_run[p->pid]++;
				q1[i]=0;
				for(int j=i;j<=c1-1;j++)
					q1[j] = q1[j+1];
				q1[c1]=0;
				c1--;
				int k=0;
				for(k=0;k<2;k++)
				{
					if(p->state == RUNNABLE)
					{
						total_stat.last[p->pid] = current_time;
						current_time++;
						switchuvm(p);
						p->state = RUNNING;
						swtch(&(c->scheduler), p->context);
						switchkvm();
						p->clicks++;
						total_stat.ticks[p->pid][1]++;
					}
					else
						break;
				}
				if(k==2 && p->pid!=0)
				{
					c2++;
					p->pri=2;
					total_stat.current_queue[p->pid] = p->pri;
					p->clicks=0;
					q2[c2] = p;
				}
				else if(p->pid!=0)
				{
					p->pri=1;
					total_stat.current_queue[p->pid]=1;
					c1++;
					q1[c1]=p;
				}
				c->proc = 0;
				mo=0;
				break;
			}
		}
		if(mo==0)
		{
			mo=-1;
			release(&ptable.lock);
			continue;
		}
		if(c2!=-1)
		{
			for(int i=0;i<=c2;i++)
			{
				if(q2[i]->state != RUNNABLE)
					continue;
				p=q2[i];
				c->proc = q2[i];
				total_stat.num_run[p->pid]++;
				q2[i]=0;
				for(int j=i;j<=c2-1;j++)
					q2[j] = q2[j+1];
				q2[c2] =0;
				c2--;
				int k=0;
				for(k=0;k<4;k++)
				{
					if(p->state == RUNNABLE)
					{
						total_stat.last[p->pid] = current_time;
						current_time++;
						switchuvm(p);
						p->state = RUNNING;
						swtch(&(c->scheduler), p->context);
						switchkvm();
						p->clicks++;
						total_stat.ticks[p->pid][2]++;
					}
					else
						break;
				}
				if(k==4 && p->pid!=0)
				{
					c3++;
					p->pri=3;
					total_stat.current_queue[p->pid] = p->pri;
					p->clicks=0;
					q3[c3] = p;
				}
				else if(p->pid!=0)
				{
					p->pri=2;
					total_stat.current_queue[p->pid] = 2;
					c2++;
					q2[c2]=p;
				}
				c->proc=0;
				mo=0;
				break;
			}
		}
		if(mo==0)
		{
			mo=-1;
			release(&ptable.lock);
			continue;
		}
		if(c3!=-1)
		{
			for(int i=0;i<=c3;i++)
			{
				if(q3[i]->state != RUNNABLE)
					continue;
				p=q3[i];
				c->proc=q3[i];
				total_stat.num_run[p->pid]++;
				q3[i]=0;
				for(int j=i;j<=c3-1;j++)
					q3[j] = q3[j+1];
				q3[c3] =0;
				c3--;
				int k=0;
				for(k=0;k<8;k++)
				{
					if(p->state == RUNNABLE)
					{
						total_stat.last[p->pid] = current_time;
						current_time++;
						switchuvm(p);
						p->state = RUNNING;
						swtch(&(c->scheduler), p->context);
						switchkvm();
						p->clicks++;
						total_stat.ticks[p->pid][3]++;
					}
					else
						break;
				}
				if(k==8 && p->pid!=0)
				{
					c4++;
					p->pri=4;
					total_stat.current_queue[p->pid] = p->pri;
					p->clicks=0;
					q4[c4] = p;
				}
				else if(p->pid!=0)
				{
					p->pri=3;
					total_stat.current_queue[p->pid] = 3;
					c3++;
					q3[c3]=p;
				}
				c->proc = 0;
				mo=0;
				break;
			}
		}
		if(mo==0)
		{
			mo=-1;
			release(&ptable.lock);
			continue;
		}
		if(c4!=-1)
		{
			for(int i=0;i<=c4;i++)
			{
				if(q4[i]->state != RUNNABLE)
					continue;
				p=q4[i];
				c->proc=q4[i];
				total_stat.num_run[p->pid]++;		
				q4[i]=0;
				for(int j=i;j<=c4-1;j++)
					q4[j] = q4[j+1];
				q4[c4] =0;
				c4--;
				int k=0;
				for(k=0;k<16;k++)
				{
					if(p->state == RUNNABLE)
					{
						total_stat.last[p->pid] = current_time;
						current_time++;
						switchuvm(p);
						p->state = RUNNING;
						swtch(&(c->scheduler), p->context);
						switchkvm();
						p->clicks++;
						total_stat.ticks[p->pid][4]++;
					}
					else
						break;
				}
				if(p->pid!=0)
				{
					p->pri=4;
					total_stat.current_queue[p->pid] = 4;
					c4++;
					q4[c4]=p;
				}
				c->proc = 0;
				mo=0;
				break;
			}
		}
		if(mo==0)
		{
			mo=-1;
			release(&ptable.lock);
			continue;
		}
#endif
#endif
#endif
#endif
		release(&ptable.lock);
	}
}

// enter scheduler.  must hold only ptable.lock
// and have changed proc->state. saves and restores
// intena because intena is a property of this
// kernel thread, not this cpu. it should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
	void
sched(void)
{
	int intena;
	struct proc *p = myproc();

	if(!holding(&ptable.lock))
		panic("sched ptable.lock");
	if(mycpu()->ncli != 1)
		panic("sched locks");
	if(p->state == RUNNING)
		panic("sched running");
	if(readeflags()&FL_IF)
		panic("sched interruptible");
	intena = mycpu()->intena;
	swtch(&p->context, mycpu()->scheduler);
	mycpu()->intena = intena;
}

// give up the cpu for one scheduling round.
	void
yield(void)
{
	acquire(&ptable.lock);  //DOC: yieldlock
	myproc()->state = RUNNABLE;
	sched();
	release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
	void
forkret(void)
{
	static int first = 1;
	// Still holding ptable.lock from scheduler.
	release(&ptable.lock);

	if (first) {
		// Some initialization functions must be run in the context
		// of a regular process (e.g., they call sleep), and thus cannot
		// be run from main().
		first = 0;
		iinit(ROOTDEV);
		initlog(ROOTDEV);
	}

	// Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
	void
sleep(void *chan, struct spinlock *lk)
{
	struct proc *p = myproc();

	if(p == 0)
		panic("sleep");

	if(lk == 0)
		panic("sleep without lk");

	// Must acquire ptable.lock in order to
	// change p->state and then call sched.
	// Once we hold ptable.lock, we can be
	// guaranteed that we won't miss any wakeup
	// (wakeup runs with ptable.lock locked),
	// so it's okay to release lk.
	if(lk != &ptable.lock){  //DOC: sleeplock0
		acquire(&ptable.lock);  //DOC: sleeplock1
		release(lk);
	}
	// Go to sleep.
	p->chan = chan;
	p->state = SLEEPING;

	sched();

	// Tidy up.
	p->chan = 0;

	// Reacquire original lock.
	if(lk != &ptable.lock){  //DOC: sleeplock2
		release(&ptable.lock);
		acquire(lk);
	}
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
	static void
wakeup1(void *chan)
{
	struct proc *p;
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
		if (p->state == SLEEPING && p->chan == chan)	
			p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
	void
wakeup(void *chan)
{
	acquire(&ptable.lock);
	wakeup1(chan);
	release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
	int
kill(int pid)
{
	struct proc *p;

	acquire(&ptable.lock);
	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
		if(p->pid == pid){
			p->killed = 1;
			// Wake process from sleep if necessary.
			if(p->state == SLEEPING)
				p->state = RUNNABLE;
			release(&ptable.lock);
			return 0;
		}
	}
	release(&ptable.lock);
	return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
	void
procdump(void)
{
	static char *states[] = {
		[UNUSED]    "unused",
		[EMBRYO]    "embryo",
		[SLEEPING]  "sleep ",
		[RUNNABLE]  "runble",
		[RUNNING]   "run   ",
		[ZOMBIE]    "zombie"
	};
	int i;
	struct proc *p;
	char *state;
	uint pc[10];

	for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
		if(p->state == UNUSED)
			continue;
		if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
			state = states[p->state];
		else
			state = "???";
		cprintf("%d %s %s", p->pid, state, p->name);
		if(p->state == SLEEPING){
			getcallerpcs((uint*)p->context->ebp+2, pc);
			for(i=0; i<10 && pc[i] != 0; i++)
				cprintf(" %p", pc[i]);
		}
		cprintf("\n");
	}
}
