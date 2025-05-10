/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

 #include "string.h"
 #include "mm.h"
 #include "syscall.h"
 #include "libmem.h"
 #include "os-mm.h"
 #include <stdlib.h>
 #include <stdio.h>
 #include <pthread.h>
 static pthread_mutex_t mmvm_lock = PTHREAD_MUTEX_INITIALIZER;
void print_free_list(struct mm_struct *mm, int vmaid,  int rgid){
    struct vm_rg_struct *curr = get_vma_by_num(mm, vmaid)->vm_freerg_list;
    printf("Region ID = %d is released\n",rgid);
    int i = 0;
     while (curr != NULL)
     {
        printf("Freeing list %d ,range(%lu, %lu)\n",i++, curr->rg_start, curr->rg_end);
        curr = curr->rg_next;
     }
 }
 
 /* enlist_vm_freerg_list - add new rg to freerg_list
  *@mm: memory region
  *@rg_elmt: new region
  */
 int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt)
 {
     struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;
 
     if (rg_elmt->rg_start >= rg_elmt->rg_end)
         return -1;
 
     if (rg_node != NULL)
         rg_elmt->rg_next = rg_node;
 
     /* Enlist the new region */
     mm->mmap->vm_freerg_list = rg_elmt;
 
     return 0;
 }
 
 /* get_symrg_byid - get mem region by region ID
  *@mm: memory region
  *@rgid: region ID act as symbol index of variable
  */
 struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
 {
     if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
         return NULL;
 
     return &mm->symrgtbl[rgid];
 }
 
 /* __alloc - allocate a region memory
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@rgid: memory region ID (used to identify variable in symbol table)
  *@size: allocated size
  *@alloc_addr: address of allocated memory region
  */
 int __alloc(struct pcb_t *caller, int vmaid, int rgid, int size, int *alloc_addr)
 {
     struct vm_rg_struct rgnode;
     /* Try to allocate from a free virtual memory region */
     if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
     {
         caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
         caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
         *alloc_addr = rgnode.rg_start;
         pthread_mutex_unlock(&mmvm_lock);
         return 0;
     }
 
     /* Failed to find free region, try increasing VMA limit */
     struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
     if (cur_vma == NULL)
     {
         pthread_mutex_unlock(&mmvm_lock);
         return -1; /* Invalid VMA */
     }
 
     /* Align size to page boundary and store current sbrk */
     int inc_sz = PAGING_PAGE_ALIGNSZ(size);
     int old_sbrk = cur_vma->sbrk;
 
     /* Prepare system call to increase VMA limit */
     struct sc_regs regs;
     regs.a1 = SYSMEM_INC_OP; /* Operation: increase limit */
     regs.a2 = vmaid;         /* VMA ID */
     regs.a3 = inc_sz;        /* Size to increase */
 
     /* Invoke sys_memmap syscall (number 17) */
     int inc_limit_ret = syscall(caller, 17, &regs);
     if (inc_limit_ret != 0)
     {
         pthread_mutex_unlock(&mmvm_lock);
         return -1; /* Failed to increase limit */
     }
 
     /* Commit the limit increment */
     cur_vma->sbrk += inc_sz;
 
     /* Commit the allocation address and update symbol table */
     *alloc_addr = old_sbrk;
     get_free_vmrg_area(caller, vmaid, size, &rgnode);
     caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
     caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
 
     pthread_mutex_unlock(&mmvm_lock);
     return 0;
 }
 
 /* __free - remove a region memory
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@rgid: memory region ID (used to identify variable in symbol table)
  */
 int __free(struct pcb_t *caller, int vmaid, int rgid)
 {
     if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
         return -1;
 
     struct vm_rg_struct *rgnode = malloc(sizeof(struct vm_rg_struct));
     if (rgnode == NULL)
         return -1;
 
     rgnode->rg_start = caller->mm->symrgtbl[rgid].rg_start;
     rgnode->rg_end = caller->mm->symrgtbl[rgid].rg_end;
     rgnode->rg_next = NULL;
 
     /* Validate the region */
     if (rgnode->rg_start >= rgnode->rg_end)
     {
         free(rgnode);
         pthread_mutex_unlock(&mmvm_lock);
         return -1; /* Invalid region */
     }
 
     /* Clear the symbol table entry */
     caller->mm->symrgtbl[rgid].rg_start = 0;
     caller->mm->symrgtbl[rgid].rg_end = 0;
 
     /* Enlist the obsoleted memory region to the free list */
     enlist_vm_freerg_list(caller->mm, rgnode);
     print_pgtbl(caller, 0, -1);
     print_free_list(caller->mm, vmaid, rgid);
     pthread_mutex_unlock(&mmvm_lock);
     return 0;
 }
 
 /* liballoc - PAGING-based allocate a region memory
  *@proc: Process executing the instruction
  *@size: allocated size
  *@reg_index: memory region ID (used to identify variable in symbol table)
  */
 int liballoc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
 {
    pthread_mutex_lock(&mmvm_lock);
     int addr;
 
     /* By default using vmaid = 0 */
     if (__alloc(proc, 0, reg_index, size, &addr) == 0)
     {
        printf("===== PHYSICAL MEMORY AFTER ALLOCATION =====\n");

        printf("PID=%d - Region=%d - Address=%08X - Size=%d byte\n",proc->pid, reg_index, addr, size);
        print_pgtbl(proc, 0, -1);

         return addr; /* Return the allocated address */
     }

     return 0; /* Return 0 on failure */
 }
 
 /* libfree - PAGING-based free a region memory
  *@proc: Process executing the instruction
  *@reg_index: memory region ID (used to identify variable in symbol table)
  */
 int libfree(struct pcb_t *proc, uint32_t reg_index)
 {
    pthread_mutex_lock(&mmvm_lock);
    printf("===== PHYSICAL MEMORY AFTER DEALLOCATION =====\n");
     /* By default using vmaid = 0 */
    printf("PID=%d - Region=%d\n", proc->pid, reg_index);
     return __free(proc, 0, reg_index);
 }
 
 /* pg_getpage - get the page in ram
  *@mm: memory region
  *@pgn: PGN
  *@framenum: return FPN
  *@caller: caller
  */
 int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
 {
     uint32_t pte = mm->pgd[pgn];
     if (!PAGING_PAGE_PRESENT(pte))
     { /* Page is not online, make it actively living */
         int vicpgn, swpfpn, vicfpn;
         uint32_t vicpte;
 
         /* Find victim page */
         if(find_victim_page(caller->mm, &vicpgn)== -1){
            return -1;
         }
         vicpte = mm->pgd[vicpgn];
         vicfpn = PAGING_FPN(vicpte);
         /* Get free frame in MEMSWP */
         if (MEMPHY_get_freefp(caller->active_mswp, &swpfpn) != 0)
         {
             return 0; /* No free swap frame */
         }
         /* Swap victim frame to swap */
         struct sc_regs regs;
         regs.a2 = vicfpn;        /* Source: victim physical frame */
         regs.a3 = swpfpn;        /* Destination: swap frame */
         regs.a1 = SYSMEM_SWP_OP; /* Operation: swap */
 
         /* SYSCALL 17 sys_memmap */
         if (syscall(caller, 17, &regs) != 0)
         {
             return 0; /* Swap failed */
         }
 
         pte_set_swap(mm->pgd, vicpgn, swpfpn);
 
         int tgtfpn = PAGING_PTE_SWP(pte); /* Target frame in swap */
         if (tgtfpn != 0)                  /* Only swap if target is in swap */
         {
             regs.a2 = tgtfpn;        /* Source: target swap frame */
             regs.a3 = vicfpn;        /* Destination: victim’s physical frame */
             regs.a1 = SYSMEM_SWP_OP; /* Operation: swap */
 
             /* SYSCALL 17 sys_memmap */
             if (syscall(caller, 17, &regs) != 0)
             {
                 return 0; /* Swap failed */
             }
         }
 
         pte_set_fpn(&mm->pgd[pgn], vicfpn);
         enlist_pgn_node(&caller->mm->fifo_pgn, pgn);
     }
     *fpn = PAGING_FPN(mm->pgd[pgn]);
     return 0;
 }
 
 /* pg_getval - read value at given offset
  *@mm: memory region
  *@addr: virtual address to access
  *@data: value
  */
 int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
 {
     int pgn = PAGING_PGN(addr);
     int off = PAGING_OFFST(addr);
     int fpn;
     /* Get the page to MEMRAM, swap from MEMSWAP if needed */
     if (pg_getpage(mm, pgn, &fpn, caller) != 0)
         return -1; /* Invalid page access */
     int phyaddr = (fpn << 12) + off; /* Frame number * page size + offset */
 
     /* Read byte from physical memory using syscall */
     struct sc_regs regs;
     regs.a2 = phyaddr;        /* Physical address to read */
     regs.a3 = 0;              /* var to store value when read */
     regs.a1 = SYSMEM_IO_READ; /* Operation: read byte */
 
     /* SYSCALL 17 sys_memmap */
     int result = syscall(caller, 17, &regs);
     if (result != 0)
         return -1; /* Read failed */
 
     /* Update data with the read value */
     *data = (BYTE)regs.a3; /* Assume syscall returns value in a1 */
     return 0;
 }
 
 /* pg_setval - write value to given offset
  *@mm: memory region
  *@addr: virtual address to access
  *@value: value
  */
 int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
 {
     int pgn = PAGING_PGN(addr);
     int off = PAGING_OFFST(addr);
     int fpn;
 
     /* Get the page to MEMRAM, swap from MEMSWAP if needed */
     if (pg_getpage(mm, pgn, &fpn, caller) != 0)
         return -1; /* Invalid page access */
 
     int phyaddr = (fpn << 12) + off; /* Frame number * page size + offset */
 
     /* Write byte to physical memory using syscall */
     struct sc_regs regs;
     regs.a2 = phyaddr;         /* Physical address to write */
     regs.a3 = value;           /* Value to write */
     regs.a1 = SYSMEM_IO_WRITE; /* Operation: write byte */
 
     /* SYSCALL 17 sys_memmap */
     if (syscall(caller, 17, &regs) != 0)
         return -1; /* Write failed */
     return 0;
 }
 
 /* __read - read value in region memory
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@offset: offset to access in memory region
  *@rgid: memory region ID (used to identify variable in symbol table)
  */
 int __read(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE *data)
 {  
     struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
     struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
     if(currg->rg_start + offset > currg->rg_end){
        return -1;
     }
     if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
         return -1;
 
     return pg_getval(caller->mm, currg->rg_start + offset, data, caller);
 }
 
 /* libread - PAGING-based read a region memory */
 int libread(struct pcb_t *proc, uint32_t source, uint32_t offset, uint32_t *destination)
 {
    pthread_mutex_lock(&mmvm_lock);
     BYTE data;
     int val = __read(proc, 0, source, offset, &data);
 
     *destination = (val == 0) ? (uint32_t)data : 0;
     printf("===== PHYSICAL MEMORY AFTER READING =====\n");
 #ifdef IODUMP
     printf("read region=%d offset=%d value=%d\n", source, offset, data);
 #ifdef PAGETBL_DUMP
     print_pgtbl(proc, 0, -1); // Print max TBL
 #endif
     MEMPHY_dump(proc->mram);
 #endif
    pthread_mutex_unlock(&mmvm_lock);
    if(val == -1) printf("But Failed\n");
     return val;
 }
 
 /* __write - write a region memory
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@offset: offset to access in memory region
  *@rgid: memory region ID (used to identify variable in symbol table)
  */
 int __write(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE value)
 {
     struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
     struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
     if(currg->rg_start + offset > currg->rg_end){
        return -1;
     }
 
     if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
         return -1;
 
     return pg_setval(caller->mm, currg->rg_start + offset, value, caller);
 }
 
 /* libwrite - PAGING-based write a region memory */
 int libwrite(struct pcb_t *proc, BYTE data, uint32_t destination, uint32_t offset)
 {
    pthread_mutex_lock(&mmvm_lock);
    printf("===== PHYSICAL MEMORY AFTER WRITING =====\n");
 #ifdef IODUMP
     printf("write region=%d offset=%d value=%d\n", destination, offset, data);
 #endif
     int ret = __write(proc, 0, destination, offset, data);
#ifdef PAGETBL_DUMP
     print_pgtbl(proc, 0, -1); // Print max TBL
 #endif
     MEMPHY_dump(proc->mram);
     pthread_mutex_unlock(&mmvm_lock);
     if(ret == -1) printf("But Failed\n");
    return ret;
 }
 
 /* free_pcb_memphy - collect all memphy of pcb
  *@caller: caller
  */
 int free_pcb_memphy(struct pcb_t *caller)
 {
     int pagenum, fpn;
     uint32_t pte;
 
     for (pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
     {
         pte = caller->mm->pgd[pagenum];
 
         if (PAGING_PAGE_PRESENT(pte))
         {
             fpn = PAGING_PTE_FPN(pte);
             MEMPHY_put_freefp(caller->mram, fpn);
         }
         else
         {
             fpn = PAGING_PTE_SWP(pte);
             MEMPHY_put_freefp(caller->active_mswp, fpn);
         }
     }
 
     return 0;
 }
 
 /* find_victim_page - find victim page
  *@mm: memory region
  *@retpgn: return page number
  */
 int find_victim_page(struct mm_struct *mm, int *retpgn)
 {
     struct pgn_t *pg = mm->fifo_pgn;
     if (pg == NULL)
         return -1;
     struct pgn_t *victim = mm->fifo_pgn;
     *retpgn = victim->pgn; /* Return victim page number */
     /* Update FIFO list by removing the head */
     mm->fifo_pgn = victim->pg_next;
 
     /* Free the victim node */
     free(victim);
     return 0;
 }
 
 /* get_free_vmrg_area - get a free vm region
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@size: allocated size
  */
 int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg)
 {
     struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
     struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;
 
     if (rgit == NULL)
         return -1;
     /* Initialize newrg */
     newrg->rg_start = newrg->rg_end = -1;
 
     struct vm_rg_struct *prev = NULL;
     struct vm_rg_struct *curr = rgit;
 
     while (curr != NULL)
     {
         unsigned long region_size = curr->rg_end - curr->rg_start;
         if (region_size >= size)
         {
             /* Found a suitable region */
             newrg->rg_start = curr->rg_start;
             newrg->rg_end = curr->rg_start + size;
 
             /* Handle excess space by splitting */
             if (region_size > size)
             {
                 struct vm_rg_struct *remainder = malloc(sizeof(struct vm_rg_struct));
                 if (remainder == NULL)
                 {
                     return -1; /* Allocation failed */
                 }
                 remainder->rg_start = curr->rg_start + size;
                 remainder->rg_end = curr->rg_end;
                 remainder->rg_next = curr->rg_next;
 
                 if (prev == NULL)
                     cur_vma->vm_freerg_list = remainder;
                 else
                     prev->rg_next = remainder;
             }
             else
             {
                 /* Remove entire region from list */
                 if (prev == NULL)
                     cur_vma->vm_freerg_list = curr->rg_next;
                 else
                     prev->rg_next = curr->rg_next;
             }
 
             free(curr); /* Free the original node */
             return 0;
         }
         prev = curr;
         curr = curr->rg_next;
     }
     return -1;
 }