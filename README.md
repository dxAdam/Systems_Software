# Systems Software
Projects from my Advanced Systems Software course at the University of Pittsburgh. This was my favorite course and these projects were some of the most interesting and challenging in the CS curriculum.
 
## [sync_tools](https://github.com/dxAdam/Systems_Software/tree/adam/sync_tools) 
This project involved writing synchronization primitives for multicore systems. We used inline assembly to implement memory barriers, atomic addition/subtraction, spinlocks, barriers, reader writer locks, and finally a lock-free queue..   
 
The code we wrote is located in [locking.c](https://github.com/dxAdam/Systems_Software/blob/adam/sync_tools/locking.c). 
  
## [virtual_memory](https://github.com/dxAdam/Systems_Software/tree/adam/virtual_memory) 
This project implements page tables in the Linux kernel to be used with Ubuntu 18. The supplied project framework takes a block of memory offline and makes it available to our module to allocate from. 
 
The code we wrote is located in [on_demand.c](https://github.com/dxAdam/Systems_Software/blob/adam/virtual_memory/on_demand.c). 

## [user_threads](https://github.com/dxAdam/Systems_Software/tree/adam/user_threads) 
This project involved implementing a user level pthreads-like library. The library contains functions handling the initialization and creation of threads, yielding and joining threads, and the termination of threads. 
 
The code we wrote is located in [pet_thread.c](https://github.com/dxAdam/Systems_Software/blob/adam/user_threads/pet_thread.c). 
