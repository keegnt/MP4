/*
 File: vm_pool.C
 
 Author:
 Date  : 2024/09/20
 
 */

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "vm_pool.H"
#include "console.H"
#include "utils.H"
#include "assert.H"
#include "page_table.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   V M P o o l */
/*--------------------------------------------------------------------------*/

VMPool::VMPool(unsigned long  _base_address,
    unsigned long  _size,
    ContFramePool *_frame_pool,
    PageTable     *_page_table) :
    base_address(_base_address),
    size(_size),
    frame_pool(_frame_pool),
    page_table(_page_table){
    
    // Initialize free region as the entire pool
    free_base_page[0] = base_address / PAGE_SIZE;
    free_length[0] = size / PAGE_SIZE;
    free_count = 1;
    _page_table->register_pool(this);

    Console::puts("Constructed VMPool object.\n");
    
}


unsigned long VMPool::allocate(unsigned long _size) {
    unsigned long num_pages_needed = (_size + PAGE_SIZE - 1) / PAGE_SIZE;
    Console::puts("searching for free region of size ");
    Console::putui(num_pages_needed);
    Console::puts(" pages in the vm pool for allocation\n");

    Console::puts("Before allocation - Free regions: ");
    Console::putui(free_count);
    Console::puts("\n Allocated regions: ");
    Console::putui(allocated_count);
    Console::puts("\n");

    for (unsigned int i = 0; i < free_count; ++i) {
        if (free_length[i] >= num_pages_needed) {
            unsigned long allocated_base = free_base_page[i];

            // Adjust the free region
            free_base_page[i] += num_pages_needed;
            free_length[i] -= num_pages_needed;

            if (free_length[i] == 0) {
                // Remove the free region if it is fully allocated
                free_base_page[i] = free_base_page[--free_count];
                free_length[i] = free_length[free_count];
            }

            // Add the allocated region to the allocated arrays
            allocated_base_page[allocated_count] = allocated_base;
            allocated_length[allocated_count] = num_pages_needed;
            allocated_count++;

            Console::puts("Allocated memory region from ");
            Console::putui(allocated_base);
            Console::puts(" to ");
            Console::putui(allocated_base+num_pages_needed);
            Console::puts("\n");

            Console::puts("After allocation - Free regions: ");
            Console::putui(free_count);
            Console::puts("\n Allocated regions: ");
            Console::putui(allocated_count);
            Console::puts("\n");

            return allocated_base * PAGE_SIZE;
        }
    }

    // No suitable free region found
    Console::puts("Allocation failed: No suitable free region found.\n");
    return 0;
}



void VMPool::release(unsigned long _start_address) {
    //  Convert the start address to a page number
    unsigned long start_page = _start_address / PAGE_SIZE;

    Console::puts("release called from address: ");
    Console::putui(_start_address);
    Console::puts("\n");

    if(!is_legitimate(_start_address)){
        Console::puts("Error: Region not found for release.\n");
    }
    Console::puts("Before release - Free regions: ");
    Console::putui(free_count);
    Console::puts("\n Allocated regions: ");
    Console::putui(allocated_count);
    Console::puts("\n");

    

    // Find the allocated region
    for (unsigned int i = 0; i < allocated_count; ++i) {
        if (allocated_base_page[i] == start_page) {
            Console::puts("Released memory region from page ");
            Console::putui(start_page);
            Console::puts(" to ");
            Console::putui(start_page + allocated_length[i]);
            Console::puts("\n");

            // Move the allocated region back to the free list
            free_base_page[free_count] = allocated_base_page[i];
            free_length[free_count] = allocated_length[i];
            free_count++;

            // Remove the allocated region
            allocated_base_page[i] = allocated_base_page[--allocated_count];
            allocated_length[i] = allocated_length[allocated_count];

             Console::puts("After release - Free regions: ");
            Console::putui(free_count);
            Console::puts("\n Allocated regions: ");
            Console::putui(allocated_count);
            Console::puts("\n");

            Console::puts("Released memory region\n");
            return;
        }
    }

    Console::puts("Error: Address not found in allocated regions.\n");
    

}

bool VMPool::is_legitimate(unsigned long _address) {
    Console::puts("checking if address: ");
    Console::putui(_address);
    Console::puts(" is valid\n");
    unsigned long page_number = _address / PAGE_SIZE;
    for (unsigned int i = 0; i < allocated_count; ++i) {
        if (page_number >= allocated_base_page[i] && page_number < allocated_base_page[i] + allocated_length[i]) {
            Console::puts("the address page: ");
            Console::putui(page_number);
            Console::puts(" is found between ");
            Console::putui(allocated_base_page[i]);
            Console::puts(" and ");
            Console::putui(allocated_base_page[i]+allocated_length[i]);
            Console::puts("\n");

            Console::puts("After legitimate - Free regions: ");
            Console::putui(free_count);
            Console::puts("\n Allocated regions: ");
            Console::putui(allocated_count);
            Console::puts("\n");

            return true;
        }
    }
    Console::puts("the address: ");
    Console::putui(_address);
    Console::puts(" is not found within any allocated region\n");

    Console::puts("Free regions:");
    Console::putui(free_count);
    Console::puts("\n");
    for (unsigned int i = 0; i < free_count; ++i) {
        Console::puts("Free region from ");
        Console::putui(free_base_page[i]);
        Console::puts(" to ");
        Console::putui(free_base_page[i] + free_length[i]);
        Console::puts("\n");
    }

    Console::puts("Allocated regions:");
    Console::putui(allocated_count);
    Console::puts("\n");
    for (unsigned int i = 0; i < allocated_count; ++i) {
        Console::puts("Allocated region from ");
        Console::putui(allocated_base_page[i]);
        Console::puts(" to ");
        Console::putui(allocated_base_page[i] + allocated_length[i]);
        Console::puts("\n");
    }

    return false;
}

