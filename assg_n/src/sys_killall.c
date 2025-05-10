/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */
 #include <stdlib.h>
 #include "common.h"
 #include "syscall.h"
 #include "stdio.h"
 #include "libmem.h"
 #include "string.h"
 #include "queue.h"
 
 #define MAX_MLQ_PRIORITIES 140 // 140 priority levels for mlq_ready_queue
 int static success = 0;
 static const char *get_proc_name_frompath(const char *path) {
    if (!path || path[0] == '\0') {
        printf("Warning: Invalid path '%s', returning empty name\n", path ? path : "(null)");
        return ""; // Return empty string for NULL or empty path
    }
    const char *last_slash = strrchr(path, '/');
    if (last_slash == NULL) {
        return path; // No '/' found, return entire path
    }
    const char *name = last_slash + 1;
    if (name[0] == '\0') {
        printf("Warning: Path '%s' ends with '/', returning empty name\n", path);
        return ""; // Path ends with '/', return empty
    }
    return name; // Return name after last '/'
}

 // Extract process name from path (skip "input/proc/" prefix)
 char *get_proc_name(struct pcb_t *proc) {
    // Dynamically allocate memory for proc_name_
    char *proc_name_ = (char *)malloc(101); // +1 for null terminator
    if (!proc_name_) {
        return NULL; // Handle allocation failure
    }

    memset(proc_name_, 0, 101); // Initialize memory to zero

    int i = 0;
    uint32_t data = 0;
    if(proc->code == NULL || proc->code->text == NULL ) {
        proc_name_[0] = '\0';
        return proc_name_;
    }
    while (i < 100) { // Prevent buffer overflow
        if(libread(proc, proc->code->text->arg_1, i, &data)==-1){
            free(proc_name_);
            return get_proc_name_frompath(proc->path);
        }
        if (data == -1 || data == 255) { // Check for termination conditions
            proc_name_[i] = '\0';
            break;
        }

        proc_name_[i] = (char)data;
        i++;
    }
    success = 1;
    // Ensure the string is null-terminated
    proc_name_[100] = '\0';
    return proc_name_;
}
 
 int __sys_killall(struct pcb_t *caller, struct sc_regs *regs)
 {
    pthread_mutex_lock(&queue_lock);
     char proc_name[100] = {0}; // Initialize to avoid garbage data
     uint32_t data;
     uint32_t memrg = regs->a1; // Memory region ID from syscall argument
     int already_freed = 0;
     // Read process name from memory region
    int i = 0;
    data = 0;
    while(data != -1 && data != 255){
        libread(caller, memrg, i, &data);
        proc_name[i]= data;
        if(data == -1 || data == 255) {
            proc_name[i]='\0';
        }
        i++;
    }
     printf("The procname retrieved from memregionid %d is \"%s\"\n", memrg, proc_name);
 
     // Macro to process a queue: dequeue ALL processes, terminate matching ones, enqueue back non-matching ones
     #define TERMINATE_MATCHING(queue) \
         if (queue && !empty(queue)) { \
             struct queue_t temp_queue ; /* Temporary queue for non-matching processes */ \
             temp_queue.size = 0; \
             struct pcb_t *proc; \
             /* Dequeue ALL processes until queue is empty */ \
             while (!empty(queue)) { \
                 proc = dequeue(queue); /* Dequeue front process (MLQ_SCHED mode) */ \
                 if (!proc) continue; /* Skip invalid process */ \
                 char *name = get_proc_name_frompath(proc->path); \
                 if (strcmp(name, proc_name) == 0 && name !="") { \
                     /* Suitable process: terminate it */ \
                     printf("Terminating process: %s (pid=%d, path=%s)\n", name, proc->pid, proc->path); \
                     if( already_freed == 0 ){\
                        libfree(proc, proc->code->text->arg_1); \
                        already_freed = 1;\
                     }\
                 } else { \
                        enqueue(&temp_queue, proc); \
                 } \
                 if(success){ \
                    /*free(name); \*/success = 0; \
                }\
             } \
             /* Enqueue ALL non-matching processes back to original queue */ \
             while(empty(&temp_queue) != 1) { \
                 proc = dequeue(&temp_queue); \
                 enqueue(queue, proc); \
             } \ 
         }
     // Process running_list: dequeue all processes, terminate matching, enqueue back non-matching
     TERMINATE_MATCHING(caller->running_list);
     // Process mlq_ready_queue: dequeue all processes, terminate matching, enqueue back non-matching
     for (int priority = 0; priority < MAX_MLQ_PRIORITIES; priority++) {
         struct queue_t *queue = &caller->mlq_ready_queue[priority]; /* Address of queue at priority */
         if (queue && !empty(queue)) {
             TERMINATE_MATCHING(queue);
         }
     }
     pthread_mutex_unlock(&queue_lock);
     return 0; // Success
 }