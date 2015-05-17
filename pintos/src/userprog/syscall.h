#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "threads/synch.h"
struct process_info{
	bool wait; 
	bool exit;
	int exit_status;
	int pid;
	int load;
	struct lock lock_wait;
	struct list_elem elem;
};
void syscall_init (void);
void close_file(int fd);
void remove_proc(int pid);
struct process_info* get_proc(int pid);

#endif /* userprog/syscall.h */
