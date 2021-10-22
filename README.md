# Xv6-Scheduler
## PERFORMANCE REPORT


## The Program test.c was used as sample benchmark program.
### Task Performed:
The program works by calling k children quickly without delays. Then after creating child process,Parent waits for each of them to finish and calls waitx system call for each child process.

Calculated for k=10.
Following are the run times and wait times of processes in each Scheduling algorithm.

### ● DEFAULT (Round Robin Scheduling):
```
Wait time Run time
    1480   166
    1488   167
    1486   166
    1494   167
    1488   166
    1493   166
    1487   166
    1498   168
    1487   167
    1478   168
```

### ● FCFS(First Come First Served Scheduling):
```
Wait time Run time
  4        167
 171       167
 337       167
 504       167
 671       172
 842       167
 1009      177
 1186      166
 1352      167
 1518      167
```

### ● PBS(Priority Based Scheduling):
```
Wait time Run time
    0       167
   168      167
   335      170
   507      166
   673      167
   840      177
   1018     166
   1184     167
   1352     167
   1519     168
```

### ● MLFQ(Multi-level Feedback Queue Scheduling):
```
Wait time Run time
  1435      166
  1442      167
  1449      167
  1457      167
  1465      167
  1460      167
  1468      166
  1474      167
  1483      166
  1490      167
```

Explanation for how a process could exploit MLFQ by voluntarily relinquishing control:
Suppose the process is in a queue which has time slice of t units. The process could avoid going to a lower priority queue by voluntarily relinquishing control just before t units. If it keeps repeating this, it can complete the whole task in the same queue.

