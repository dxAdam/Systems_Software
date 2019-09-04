# Kernel Development
Projects dealing with x86 Linux kernel development
 
## sync_tools 
This project involved writing synchronization primitives for multicore systems. We used inline assembly to implement memory barriers, atomic addition/subtraction, spinlocks, barriers, reader writer locks, and finally a lock-free queue..   
 
The functions we wrote are located in locking.c. 
  
## user_threads 
This project involved implementing a user level pthreads-like library. The library contained functions implementing the initialization and creation of threads, yielding and joining threads, and the termination of threads. 
 
The functions we wrote are located in pet_thread.c. 
 
## virtual_memory 
This project involved implementing page tables in the Linux kernel to be used with Ubuntu 18. The supplied project framework took a block of memory offline and made it available to us to allocate from. 
 
The functions we wrote are located in on_demand.c. 


