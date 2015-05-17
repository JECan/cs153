#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "threads/synch.h"

#define CLOSE_ALL -1
#define ERROR -1
#define NOT_LOADED 0
#define LOAD_SUCCESS 1
#define LOAD_FAIL 2

struct process_info {
	bool wait; 
	bool exit;
	int status;
	int pid;
	int load;
	struct lock lock_wait;
	struct list_elem elem;
};

struct process_info* get_child_process (int pid);
void remove_child_process (struct process_info *cp);
struct process_info* add_child(int pid);
void remove_child_proc(void);

void process_close_file (int fd);

void syscall_init (void);

#endif /* userprog/syscall.h */
