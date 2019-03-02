/* On demand Paging Implementation
 * (c) Jack Lange, 2012
 */

#include <linux/slab.h>

#include "petmem.h"
#include "on_demand.h"
#include "pgtables.h"
#include "swap.h"

struct mem_map *
petmem_init_process(void)
{
    struct mem_map * map;
    struct mem_map * first_entry;
    
    map = kmalloc(sizeof(struct mem_map), GFP_KERNEL);
    map->allocated = -1;

    first_entry = (struct mem_map *)kmalloc(sizeof(struct mem_map), GFP_KERNEL);
    first_entry->allocated = 0;
    first_entry->size = PETMEM_REGION_END - PETMEM_REGION_START;
    first_entry->start = PETMEM_REGION_START;

    INIT_LIST_HEAD(&map->node);
    list_add_tail(&(first_entry->node), &(map->node));
    
    
    return map;
}


void
petmem_deinit_process(struct mem_map * map)
{
    struct mem_map * tmp;

    list_for_each_entry(tmp, &(map->node), node){
	kfree(tmp);
    }

    kfree(map);
}


uintptr_t
petmem_alloc_vspace(struct mem_map * map,
		    u64              num_pages)
{
    struct mem_map * cur;
    struct mem_map * tmp;
    struct mem_map * new;

    unsigned long req_size = num_pages*PAGE_SIZE_4KB;

   // printk("Memory allocation\n");
/*

    for(i=1; i<=5; i++){
	cur = (struct mem_map *)kmalloc(sizeof(struct mem_map), GFP_KERNEL);
        cur->allocated = i;
	
	list_add_tail(&(cur->node), &(map->node));
    }
*/
     
/*
    list_for_each(pos, &(map->node)){
	tmp = list_entry(pos, struct mem_map, node);
	if(tmp->allocated == 0 && tmp->size > (num_pages*PAGE_SIZE_4KB)){
		printk("can allocate\n");
	}
	else{
		printk("cannot allocate\n");
        }
    }

*/

    /*
	look for first node that is not allocated and has size >= to requested size
    */
    list_for_each_entry_safe(cur, tmp ,&(map->node), node){
	//printk("cur->allocated: %d\n", cur->allocated);
	if(cur->allocated == 0 && cur->size >= req_size){
	    new = (struct mem_map *)kmalloc(sizeof(struct mem_map), GFP_KERNEL);
	    new->allocated = 0;
            new->size = cur->size - req_size;
            new->start = cur->start + req_size;
	    
            cur->allocated = 1;
            cur->size = num_pages*PAGE_SIZE_4KB;
    		
	    list_add_tail(&(new->node), &(map->node));
	    return cur->start;
	}
    }

    list_for_each_entry(cur, &(map->node), node){
	printk("cur->allocated: %d\n", cur->allocated);
    }
	
 
    return -1;
}

void
petmem_dump_vspace(struct mem_map * map)
{   
    struct mem_map * tmp;
   
    printk("\npetmem dump vspace:\n"); 

    list_for_each_entry(tmp, &(map->node), node){
    	printk("allocated: %d\n", tmp->allocated);
	printk("size:      %lx\n", tmp->size);
	printk("start:     %lx\n\n", tmp->start);
    }
    
    return;
}




// Only the PML needs to stay, everything else can be freed
void
petmem_free_vspace(struct mem_map * map,
		   uintptr_t        vaddr)
{
    unsigned long int pml4_index = PML4E64_INDEX(vaddr);
    unsigned long int pdpe_index = PDPE64_INDEX(vaddr);
    unsigned long int pde_index = PDE64_INDEX(vaddr);
    unsigned long int pte_index = PTE64_INDEX(vaddr);
    uintptr_t cr3 = get_cr3();

    pml4e64_t * pml;
    pdpe64_t * pdpe;
    pde64_t * pde;
    pte64_t * pte;
    
    unsigned long addr;

    pml = CR3_TO_PML4E64_VA(cr3)  + pml4_index*sizeof(pml4e64_t);

    pdpe = __va(BASE_TO_PAGE_ADDR(pml->pdp_base_addr)) + pdpe_index*sizeof(pdpe64_t);

    pde = __va(BASE_TO_PAGE_ADDR(pdpe->pd_base_addr)) + pde_index*sizeof(pde64_t);

    pte = __va(BASE_TO_PAGE_ADDR(pde->pt_base_addr)) + pte_index*sizeof(pte64_t);
    
    addr = __va(BASE_TO_PAGE_ADDR(pte->page_base_addr));

    //printk("addr == %lx\n", addr);

    petmem_free_pages(addr, 1);
    printk("Free Memory\n");
    return;
}


/* 
   error_code:
       1 == not present
       2 == permissions error
*/

int
petmem_handle_pagefault(struct mem_map * map,
			uintptr_t        fault_addr,
			u32              error_code)
{
    unsigned long int pml4_index = PML4E64_INDEX(fault_addr);
    unsigned long int pdpe_index = PDPE64_INDEX(fault_addr);
    unsigned long int pde_index = PDE64_INDEX(fault_addr);
    unsigned long int pte_index = PTE64_INDEX(fault_addr);
    uintptr_t cr3 = get_cr3();
    
    unsigned long pdpe_table_page;   
    unsigned long pde_table_page;    
    unsigned long pte_table_page;   
    uintptr_t user_page;

    pml4e64_t * pml;
    pdpe64_t * pdpe;
    pde64_t   * pde;
    pte64_t * pte;  
/* 
    printk("__va(0x0): %lx\n", (unsigned long)__va(0x0UL));
    printk("__va(0x40000000): %lx\n", (unsigned long)__va(0x40000000UL)); 

 
    printk("__pa(0xFFFFFFFFFFFFFFFFUL): %lx\n", __pa(0xFFFFFFFFFFFFFFFFUL)); 
    printk("__pa(0xFFFF800000000000UL): %lx\n", __pa(0xFFFF800000000000UL)); 
*/
    //printk("pml64_index: %ul\n pdpe_index: %ul\n pdp_index: %ul\n pte_index: %ul\n", pml4_index, pdpe_index, pde_index, pte_index);  

    
//    printk("Page fault! error_code: %d    fault address: %p\n", error_code, (void*)fault_addr);

    pml = CR3_TO_PML4E64_VA(cr3)  + pml4_index*sizeof(pml4e64_t);

    if(pml->present == 0){
	pdpe_table_page = __get_free_page(GFP_KERNEL);
	pml->pdp_base_addr = PAGE_TO_BASE_ADDR(__pa(pdpe_table_page));
        pml->present = 1;
        pml->writable = 1;
        pml->user_page = 1;
    }

    pdpe = __va(BASE_TO_PAGE_ADDR(pml->pdp_base_addr)) + pdpe_index*sizeof(pdpe64_t);

    if(pdpe->present == 0){		
        //pde_table_page = (unsigned long)kmalloc(sizeof(PAGE_SIZE_4KB), GFP_KERNEL);
        pde_table_page = __get_free_page(GFP_KERNEL);
        pdpe->pd_base_addr = PAGE_TO_BASE_ADDR(__pa(pde_table_page));	
	pdpe->present = 1;
        pdpe->writable = 1;
        pdpe->user_page = 1;
    }
    

    pde = __va(BASE_TO_PAGE_ADDR(pdpe->pd_base_addr)) + pde_index*sizeof(pde64_t);
    
    if(pde->present == 0){ 
        //pte_table_page = (unsigned long)kmalloc(sizeof(PAGE_SIZE_4KB), GFP_KERNEL);
        pte_table_page = __get_free_page(GFP_KERNEL);
        pde->pt_base_addr = PAGE_TO_BASE_ADDR(__pa(pte_table_page));	
	pde->present = 1;
        pde->writable = 1;
        pde->user_page = 1;
        pde->accessed = 1;
    }

    pte = __va(BASE_TO_PAGE_ADDR(pde->pt_base_addr)) + pte_index*sizeof(pte64_t); 

    if(pte->present == 0){
	//user_page = get_zeroed_page(GFP_KERNEL);
        user_page = petmem_alloc_pages(1);
        pte->page_base_addr = PAGE_TO_BASE_ADDR(__pa(user_page));	
	//pte->page_base_addr = user_page;
	pte->present = 1;
        //invlpg(__va(user_page));
        invlpg(user_page);
        pte->user_page = 1;
        pte->writable = 1;
        pte->accessed =1;
        pte->dirty = 1;
    }

    return 0;
    //return -1;
}
