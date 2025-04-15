// #ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Virtual memory module mm/mm-vm.c
 */

 #include "string.h"
 #include "mm.h"
 #include <stdlib.h>
 #include <stdio.h>
 #include <pthread.h>
 
 /*get_vma_by_num - get vm area by numID
  *@mm: memory region
  *@vmaid: ID vm area to alloc memory region
  *
  */
 struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid)
 {
   struct vm_area_struct *pvma = mm->mmap;
 
   if (mm->mmap == NULL)
     return NULL;
 
   int vmait = pvma->vm_id;
 
   while (vmait < vmaid)
   {
     if (pvma == NULL)
       return NULL;
 
     pvma = pvma->vm_next;
     vmait = pvma->vm_id;
   }
 
   return pvma;
 }
 
 int __mm_swap_page(struct pcb_t *caller, int vicfpn, int swpfpn)
 {
   __swap_cp_page(caller->mram, vicfpn, caller->active_mswp, swpfpn);
   return 0;
 }
 
 /*get_vm_area_node - get vm area for a number of pages
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@incpgnum: number of page
  *@vmastart: vma end
  *@vmaend: vma end
  *
  */
struct vm_rg_struct *get_vm_area_node_at_brk(struct pcb_t *caller, int vmaid, int size, int alignedsz)
{
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  struct vm_rg_struct *newrg = malloc(sizeof(struct vm_rg_struct));

  if (!cur_vma) return NULL;

  newrg->rg_start = cur_vma->sbrk;
  newrg->rg_end   = newrg->rg_start + alignedsz;
  newrg->rg_next  = NULL;

  return newrg;
}
 
 /*validate_overlap_vm_area
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@vmastart: vma end
  *@vmaend: vma end
  *
  */
int validate_overlap_vm_area(struct pcb_t *caller, int vmaid, int vmastart, int vmaend)
{
  struct vm_area_struct *vma = get_vma_by_num(caller->mm, vmaid);
  struct vm_rg_struct *it = vma->vm_freerg_list;

  while (it != NULL)
  {
    if (!(vmaend <= it->rg_start || vmastart >= it->rg_end))
      return -1; // Overlap
    it = it->rg_next;
  }

  return 0;
}
 
 /*inc_vma_limit - increase vm area limits to reserve space for new variable
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@inc_sz: increment size
  *
  */
 int inc_vma_limit(struct pcb_t *caller, int vmaid, int inc_sz)
{
  int inc_amt = PAGING_PAGE_ALIGNSZ(inc_sz);
  int incnumpage = inc_amt / PAGING_PAGESZ;

  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  if (!cur_vma) return -1;

  struct vm_rg_struct *area = get_vm_area_node_at_brk(caller, vmaid, inc_sz, inc_amt);
  if (!area) return -1;

  int old_end = cur_vma->vm_end;

  if (validate_overlap_vm_area(caller, vmaid, area->rg_start, area->rg_end) < 0)
    return -1;

  if (vm_map_ram(caller, area->rg_start, area->rg_end,
                 old_end, incnumpage, area) < 0)
    return -1;

  // Cập nhật giới hạn VM
  cur_vma->vm_end = area->rg_end;
  cur_vma->sbrk   = area->rg_end;

  // Thêm vùng mới vào danh sách free
  area->rg_next = NULL;
  if (cur_vma->vm_freerg_list == NULL)
    cur_vma->vm_freerg_list = area;
  else {
    struct vm_rg_struct *it = cur_vma->vm_freerg_list;
    while (it->rg_next != NULL)
      it = it->rg_next;
    it->rg_next = area;
  }

  return 0;
}
 
 // #endif
 
