#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t * q) {
        if (q == NULL) return 1;
	return (q->size == 0);
}

void enqueue(struct queue_t * q, struct pcb_t * proc) {
        /* TODO: put a new process to queue [q] */
        if (q->size == MAX_QUEUE_SIZE||proc == NULL||q == NULL)  {
                exit(EXIT_FAILURE);
        }
        else {
                q->proc[q->size++] =  proc; 
        }


}

struct pcb_t * dequeue(struct queue_t * q) {
        /* TODO: return a pcb whose prioprity is the highest
         * in the queue [q] and remember to remove it from q
         * */
        if (q == NULL || empty (q))  {
                exit(EXIT_FAILURE);
        }
        struct pcb_t* ret_pcb = NULL;
        int index = 0;
        #ifdef MLQ_SCHED
        ret_pcb = q->proc[0];
        #elif
        for (int i = 0; i < q->size ; i ++){
                if(i == 0){
                        ret_pcb = q->proc[0];
                }
                else{
                        if ( ret_pcb->priority > q->proc[i]->priority ){
                                ret_pcb =  q->proc[i];
                                index = i;
                        }
                }
        }
        #endif
        for (int i = index; i < q->size - 1; i++) {
                q->proc[i] = q->proc[i + 1];
        }
        q->size--; // Reduce the size of the queue
            
	return ret_pcb;
}

