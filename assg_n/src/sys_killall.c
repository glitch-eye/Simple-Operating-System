/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

 #include "common.h"
 #include "syscall.h"
 #include "stdio.h"
 #include "libmem.h"
 #include "string.h"
 #include "queue.h"

int __sys_killall(struct pcb_t *caller, struct sc_regs* regs)
{
    char proc_name[100];
    uint32_t data;

    //hardcode for demo only
    uint32_t memrg = regs->a1;
    
    /* TODO: Get name of the target proc */
    //proc_name = libread..
    int i = 0;
    data = 0;
    while(data != -1){
        libread(caller, memrg, i, &data);
        proc_name[i]= data;
        if(data == -1) proc_name[i]='\0';
        i++;
    }
    printf("The procname retrieved from memregionid %d is \"%s\"\n", memrg, proc_name);

    #define TERMINATE_MATCHING(list) \
        if (!empty(list)) { \
            for (int j = 0; j < list->size; j++) { \
                struct pcb_t *proc = list->proc[j]; \
                char name_buf[100]; \
                int k = 0; \
                while(data != -1){ \
                    libread(caller, memrg, i, &data); \
                    name_buf[i]= data; \
                    if(data == -1) name_buf[i]='\0'; \
                    i++; \
                } \
                if (strcmp(name_buf, proc_name) == 0) { \
                    printf("Terminating process: %s (pid=%d)\n", name_buf, proc->pid); \
                    libfree(proc, proc->pid); \
                    list->proc[j] = null; \
                } \
            } \
        } \

    TERMINATE_MATCHING(caller->running_list);
    TERMINATE_MATCHING(caller->mlq_ready_queue);

    /* TODO: Traverse proclist to terminate the proc
     *       stcmp to check the process match proc_name
     */
    //caller->running_list
    //caller->mlq_ready_queu

    /* TODO Maching and terminating 
     *       all processes with given
     *        name in var proc_name
     */

    return 0; 
}
