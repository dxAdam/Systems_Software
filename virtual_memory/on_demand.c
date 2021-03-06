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
    
    map = (struct mem_map *)kmalloc(sizeof(struct mem_map), GFP_KERNEL);
    map->allocated = -1;

    first_entry = (struct mem_map *)kmalloc(sizeof(struct mem_map), GFP_KERNEL);
    first_entry->allocated = 0;
    first_entry->size = PETMEM_REGION_END - PETMEM_REGION_START;
    first_entry->start = PETMEM_REGION_START;

    INIT_LIST_HEAD(&map->node);
 
    list_add(&first_entry->node, &map->node);
        
    return map;
}


void
petmem_deinit_process(struct mem_map * map)
{
    struct mem_map * cur;
    struct mem_map * tmp;
	

    list_for_each_entry_safe(cur, tmp, &map->node, node){
	if(cur->allocated == 1){
		petmem_free_vspace(map, (uintptr_t)cur->start);
	}	
	list_del(&cur->node);
	kfree(cur);
    }

	kfree(map);

}


/*
	returns
		1 - vaddr is in memory map
                0 - otherwise
*/
int
vaddr_in_map(uintptr_t vaddr, struct mem_map * map){
    
    struct mem_map * cur;

    list_for_each_entry(cur, &(map->node), node){
	if(cur->allocated == 1 && vaddr >= cur->start && vaddr <= cur->start + cur->size)
		return 1;
    }

    printk("vaddr not in mem_map\n");
    return 0;
}


uintptr_t
petmem_alloc_vspace(struct mem_map * map,
		    u64              num_pages)
{
    struct mem_map * cur;
    struct mem_map * new;

    unsigned long req_size = num_pages*PAGE_SIZE_4KB;

    printk("Memory allocation\n");

    /*
	look for first node that is not allocated and has size >= to requested size
    */
    list_for_each_entry(cur, &map->node, node){
	
	if(cur->allocated == 0 && cur->size >= req_size){
	    new = (struct mem_map *)kmalloc(sizeof(struct mem_map), GFP_KERNEL);
	    new->allocated = 0;
            new->size = cur->size - req_size;
            new->start = cur->start + req_size;
	    
            cur->allocated = 1;
            cur->size = req_size;
    		
	    list_add(&new->node, &cur->node);
	    return cur->start;
	}
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


int
pdpe_table_is_empty(pdpe64_t * pdpe){
	int i;

	for(i=0; i<MAX_PDPE64_ENTRIES; i++){
		if(pdpe->present == 1)
			return 0;
		
		pdpe = pdpe + 1;
	}
	
	return 1;
}

int
pde_table_is_empty(pde64_t * pde){
	int i;

	for(i=0; i<MAX_PDE64_ENTRIES; i++){
		if(pde->present == 1)
			return 0;
		
		pde = pde + 1;
	}
	
	return 1;
}


int
pte_table_is_empty(pte64_t * pte){
	int i;
	
	for(i=0; i<MAX_PTE64_ENTRIES; i++){
		if(pte->present == 1){
			printk("found pte->present == %d   at i==%d   pte==%p\n", pte->present, i, (void*)pte);
			return 0;
        	}
                pte = pte + 1;
        }
	
	return 1;
}

void
increment_pml_index(int * pml_index){

	*pml_index = *pml_index + 1;

	if(*pml_index == MAX_PML4E64_ENTRIES){
		return; // handle reached end of virtual memory
	}
	
	return;
}

void
increment_pdpe_index(pml4e64_t * pml, int * pdpe_index, int * pml_index){
	
	pdpe64_t * pdpe_base;
	pdpe_base = __va(BASE_TO_PAGE_ADDR(pml->pdp_base_addr));
	
	*pdpe_index = *pdpe_index + 1;

	if(*pdpe_index == MAX_PDPE64_ENTRIES){
		*pdpe_index = 0;
				
		if(pdpe_table_is_empty(pdpe_base)){
			pml->present = 0;
			free_page((unsigned long)pdpe_base);
			printk("freed pdpe_table\n");
		}

		increment_pml_index(pml_index);
	}
}

void
increment_pde_index(pdpe64_t * pdpe, pml4e64_t * pml, \
	            int * pde_index, int * pdpe_index, int * pml_index){

	pde64_t * pde_base;
	pde_base = __va(BASE_TO_PAGE_ADDR(pdpe->pd_base_addr));

	*pde_index = *pde_index + 1;

	if(*pde_index == MAX_PDE64_ENTRIES){
		*pde_index = 0;

		if(pde_table_is_empty(pde_base)){
			pdpe->present = 0;
			free_page((unsigned long)pde_base);
			printk("freed pde table\n");
		}
				
		increment_pdpe_index(pml, pdpe_index, pml_index);
	}
}

void
increment_pte_index(pde64_t * pde, pdpe64_t * pdpe, pml4e64_t * pml, \
                    int * pte_index, int * pde_index, int * pdpe_index, int * pml_index)
{

	pte64_t *pte_base;

	*pte_index = *pte_index + 1;	

	if(*pte_index == MAX_PTE64_ENTRIES){	
		//printk("*pte_index == MAX\n");
		*pte_index = 0;
	        
        	pte_base = __va(BASE_TO_PAGE_ADDR(pde->pt_base_addr));
		

		printk("checking if pte is empty\n");	
		if(pte_table_is_empty(pte_base)){
			pde->present = 0;
			free_page((unsigned long)pte_base);
			printk("freed pte table\n");
		}
	
	        increment_pde_index(pdpe, pml, pde_index, pdpe_index, pml_index);
	}
	
}


// Only the PML needs to stay, everything else can be freed
void
petmem_free_vspace(struct mem_map * map,
		   uintptr_t        vaddr)
{
    int pml_index = PML4E64_INDEX(vaddr);
    int pdpe_index = PDPE64_INDEX(vaddr);
    int pde_index = PDE64_INDEX(vaddr);
    int pte_index = PTE64_INDEX(vaddr);
    uintptr_t cr3 = get_cr3();

    pml4e64_t * pml;
    pdpe64_t * pdpe;
    pde64_t * pde;
    pte64_t * pte;
    
    unsigned long addr;
    unsigned long page_addr;
    unsigned long num_pages_to_free = 0;
    unsigned long num_pages_freed  = 0;

    struct mem_map * cur;
    struct mem_map * tmp;
    struct mem_map * next;
    struct mem_map * prev;
    
    printk("Free Memory\n");

    if(vaddr_in_map(vaddr, map) == 0){
	return;
    }
    
    page_addr = PAGE_ADDR(vaddr);

    /* 
 	now we remove vaddr from our memory map
    */
    list_for_each_entry_safe(cur, tmp, &(map->node), node){
	if(cur->start == page_addr && cur->allocated == 1){
		
                num_pages_to_free = cur->size / PAGE_SIZE_4KB;
                cur->allocated = 0;

	        // check if prev node is empty && combine if so
                prev = list_entry(cur->node.prev, struct mem_map, node);
		next = list_entry(cur->node.next, struct mem_map, node);

		if(prev->allocated == 0){
			prev->size += cur->size;
			list_del(&cur->node);
	        	kfree(cur);
			cur = prev;
		}

		//  check if next node is empty and combine if so
	        if(next->allocated == 0 && prev != next){
		       	next->size += cur->size;
                        next->start = cur->start;
                        list_del(&cur->node);
                        kfree(cur);
 			cur = next;
		}
         } 	
    }

    printk("need to free %lu pages of physical memory\n", num_pages_to_free);
   
    while(num_pages_freed < num_pages_to_free){
	
    	pml = CR3_TO_PML4E64_VA(cr3) + pml_index*sizeof(pml4e64_t);    	

	if(pml->present == 0){
		 num_pages_freed += (512-pdpe_index)*(512-pde_index)*(512-pte_index);
                 increment_pml_index(&pml_index);
		 pdpe_index = 0;
		 pde_index = 0;
		 pte_index = 0;
                 continue;
        }

        pdpe = __va(BASE_TO_PAGE_ADDR(pml->pdp_base_addr)) + pdpe_index*sizeof(pdpe64_t);

        if(pdpe->present == 0){
		num_pages_freed += (512-pde_index)*(512-pte_index);
                increment_pdpe_index(pml, &pdpe_index, &pml_index);
		pde_index = 0;
		pte_index = 0;
                continue;
        }
 
    	pde = __va(BASE_TO_PAGE_ADDR(pdpe->pd_base_addr)) + pde_index*sizeof(pde64_t);
	
	if(pde->present == 0){
		num_pages_freed += 512-pte_index;
		increment_pde_index(pdpe, pml, &pde_index, &pdpe_index, &pml_index);
                pte_index = 0;
		continue;
	}

	pte = __va(BASE_TO_PAGE_ADDR(pde->pt_base_addr)) + pte_index*sizeof(pte64_t);

	if(pte->present == 1){
    		pte->present = 0;    
    		addr = BASE_TO_PAGE_ADDR(pte->page_base_addr);
    		petmem_free_pages(addr, 1);
    		invlpg((unsigned long)__va(addr));
	}
        
	num_pages_freed += 1;
	
	increment_pte_index(pde, pdpe, pml, &pte_index, &pde_index, &pdpe_index, &pml_index);
    }

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
    int pml_index = PML4E64_INDEX(fault_addr);
    int pdpe_index = PDPE64_INDEX(fault_addr);
    int pde_index = PDE64_INDEX(fault_addr);
    int pte_index = PTE64_INDEX(fault_addr);
    uintptr_t cr3 = get_cr3();
    
    unsigned long pdpe_table_page;   
    unsigned long pde_table_page;    
    unsigned long pte_table_page;   
    uintptr_t user_page;

    pml4e64_t * pml;
    pdpe64_t * pdpe;
    pde64_t   * pde;
    pte64_t * pte;  

    printk("Page fault!  vaddr: %p\n", (void*)fault_addr);
/*  
    printk("pml index: %d\n", pml_index);
    printk("pdpe index: %d\n", pdpe_index);
    printk("pde index: %d\n", pde_index);
    printk("pte index: %d\n", pte_index);
*/  

  /*
	first we check if address is in our memory map
    */

    if(vaddr_in_map(fault_addr, map) == 0){
	return -1;
    } 

    pml = CR3_TO_PML4E64_VA(cr3)  + pml_index*sizeof(pml4e64_t);

    if(pml->present == 0){
	pdpe_table_page = get_zeroed_page(GFP_KERNEL);
	pml->pdp_base_addr = PAGE_TO_BASE_ADDR(__pa(pdpe_table_page));
        pml->present = 1;
        pml->writable = 1;
        pml->user_page = 1;
    }

    pdpe = __va(BASE_TO_PAGE_ADDR(pml->pdp_base_addr)) + pdpe_index*sizeof(pdpe64_t);

    if(pdpe->present == 0){		
        pde_table_page = get_zeroed_page(GFP_KERNEL);
        pdpe->pd_base_addr = PAGE_TO_BASE_ADDR(__pa(pde_table_page));	
	pdpe->present = 1;
        pdpe->writable = 1;
        pdpe->user_page = 1;
    }
    
    pde = __va(BASE_TO_PAGE_ADDR(pdpe->pd_base_addr)) + pde_index*sizeof(pde64_t);
    
    if(pde->present == 0){ 
        pte_table_page = get_zeroed_page(GFP_KERNEL);
        pde->pt_base_addr = PAGE_TO_BASE_ADDR(__pa(pte_table_page));	
	pde->present = 1;
        pde->writable = 1;
        pde->user_page = 1;
    }

    pte = __va(BASE_TO_PAGE_ADDR(pde->pt_base_addr)) + pte_index*sizeof(pte64_t); 

    if(pte->present == 0){
        user_page = petmem_alloc_pages(1);
	pte->page_base_addr = PAGE_TO_BASE_ADDR(user_page);
        pte->present = 1;
        pte->user_page = 1;
        pte->writable = 1;
    }


    return 0;
}
