#include "assert.H"
#include "exceptions.H"
#include "console.H"
#include "paging_low.H"
#include "page_table.H"

PageTable * PageTable::current_page_table = nullptr;
unsigned int PageTable::paging_enabled = 0;
ContFramePool * PageTable::kernel_mem_pool = nullptr;
ContFramePool * PageTable::process_mem_pool = nullptr;
unsigned long PageTable::shared_size = 0;


void PageTable::init_paging(ContFramePool * _kernel_mem_pool,
                            ContFramePool * _process_mem_pool,
                            const unsigned long _shared_size)
{
    kernel_mem_pool = _kernel_mem_pool;
    process_mem_pool = _process_mem_pool;
    shared_size = _shared_size;
   
   
    Console::puts("Initialized Paging System\n");
}

PageTable::PageTable()
{
    pool_count=0;
    // Allocate a frame for the page directory from the kernel memory pool
   unsigned long page_directory_frame = process_mem_pool->get_frames(1); // Get physical frame number

   // The physical address of the page directory (no casting, just an integer)
   unsigned long page_directory_address = page_directory_frame * PAGE_SIZE;

   // Get the virtual address where this physical memory is mapped
   page_directory = reinterpret_cast<unsigned long*>(page_directory_address); // Now, we are casting it into a pointer

   // Clear all entries in the page directory
   for (int i = 0; i < ENTRIES_PER_PAGE; ++i) {
      page_directory[i] = 0;
   }

   // Allocate a frame for the first page table (for the first 4MB of memory)
   unsigned long first_page_table_frame = process_mem_pool->get_frames(1);
   if (first_page_table_frame == 0) {
      Console::puts("Failed to allocate first page table\n");
      return;
   }

   // The physical address of the first page table
   unsigned long first_page_table_address = first_page_table_frame * PAGE_SIZE;

   // Get the virtual address where this physical memory is mapped
   unsigned long *first_page_table = reinterpret_cast<unsigned long*>(first_page_table_address); // Only here we cast to access memory

   // Map the first 4MB of memory in the page table
   unsigned long address = 0;  // Holds the physical address for each page
   for (int i = 0; i < 1024; ++i) {
      first_page_table[i] = address | 0x3; // Set as present, supervisor, read/write (011 in binary)
      address += PAGE_SIZE; // Move to the next 4KB page
   }

   // Set the first entry in the page directory to point to the first page table (physical address)
   page_directory[0] = first_page_table_address | 0x3; // Present, supervisor, read/write

   // Mark all remaining page directory entries as not-present
   for (int i = 1; i < 1024; ++i) {
      page_directory[i] = 0x2; // Supervisor level, read/write, not present (010 in binary)
   }

   //Set up recursive mapping
   page_directory[ENTRIES_PER_PAGE - 1] = page_directory_address | 0x3; 



    Console::puts("Constructed Page Table object in process memory pool\n");
}



void PageTable::load()
{
    if (page_directory == nullptr) {
        Console::puts("Error: Page directory not set\n");
        return;
    }

    write_cr3(reinterpret_cast<unsigned long>(page_directory));
    current_page_table = this;   
    Console::puts("Loaded page table\n");
}

void PageTable::enable_paging()
{
    
    write_cr0(read_cr0() | 0x80000000);
    paging_enabled = 1; 
    Console::puts("Enabled paging\n");
}

void PageTable::handle_fault(REGS * _r)
{
    Console::puts("Page fault handler called\n");

    // Ensure we have a valid current page table
    if (current_page_table == nullptr) {
        Console::puts("Error: No current page table loaded\n");
        return;
    }

    Console::puts("retrieving faulting address...");
    // Get the address that caused the fault
    unsigned long faulting_address = read_cr2();
    Console::putui(faulting_address);
    Console::puts("\n");
    // Check if the faulting address is legitimate by checking with all registered VM pools
    bool legitimate = false;
    
    for (unsigned int i = 0; i < current_page_table->pool_count; i++) {
        if (current_page_table->vm_pools[i] && current_page_table->vm_pools[i]->is_legitimate(faulting_address)) { 
            legitimate = true;
            break;  // If the address is legitimate, we can handle the page fault
        }
    }

    if (!legitimate) {
        // If the address is not part of any VM pool then abort 
        Console::puts("Segmentation fault: Address not part of any registered pool\n");
        return;
    }

    Console::puts("Legitimate page fault. Handling...\n");

    // Calculate the PDE and PTE virtual addresses 
    unsigned long *pde = current_page_table->PDE_address(faulting_address);
    unsigned long *pte = current_page_table->PTE_address(faulting_address);

    // Check if the page table is present, if not allocate a new page table
    if (!(*pde & 0x1)) {  // Check if the present bit is set

        // Allocate a new page table from the process memory pool
        unsigned long new_page_table_frame = process_mem_pool->get_frames(1);
        if (new_page_table_frame == 0) {
            Console::puts("Failed to allocate new page table\n");
            return;
        }

        *pde = (new_page_table_frame * PAGE_SIZE) | 0x3;  // Present + read/write

        // Clear the newly allocated page table using recursive mapping
        unsigned long *new_page_table = reinterpret_cast<unsigned long*>(0xFFC00000 | ((faulting_address >> 22) & 0x3FF) << 12);
        for (int i = 0; i < ENTRIES_PER_PAGE; ++i) {
            new_page_table[i] = 0;
        }

    }


    // Check if the page is present, if not allocate a new frame for the page
    if (!(*pte & 0x1)) {  // Check if the page is present

        // Allocate a new frame for the page from the process memory pool
        unsigned long new_frame = process_mem_pool->get_frames(1);

        if (new_frame == 0) {
            Console::puts("Failed to allocate new frame\n");
            return;
        }

        // Set the PTE to point to the newly allocated frame
        *pte = (new_frame * PAGE_SIZE) | 0x3;  // Present + read/write

    }

    Console::puts("handled page fault\n");
}

void PageTable::register_pool(VMPool * _vm_pool)
{
    Console::puts("Registering VMPool object with page table\n");
    if (pool_count < MAX_POOLS) {
        vm_pools[pool_count++] = _vm_pool;
        Console::puts("Registered VM pool\n");
    } else {
        Console::puts("Error: Maximum number of VM pools reached\n");
    }
}

void PageTable::free_page(unsigned long _page_no) {
    // Calculate the virtual address corresponding to the page number
    unsigned long virtual_address = _page_no * PAGE_SIZE;

    // Get the PDE and PTE addresses using the helper functions
    unsigned long* pde = PDE_address(virtual_address);
    unsigned long* pte = PTE_address(virtual_address);

    // Check if the page is present by checking the present bit in the PTE
    if (!(*pte & 0x1)) {
        // Page is already invalid, no need to free it
        Console::puts("Error: Page is already invalid\n");
        return;
    }

    // Mark the page as invalid by clearing the present bit in the PTE
    *pte = 0;

    // Flush the TLB by reloading CR3 with the current value
    write_cr3(read_cr3());

    Console::puts("freed page\n");
}

unsigned long * PageTable::PDE_address(unsigned long addr){
    // Extract the PDE index from the logical address
    unsigned long pd_index = (addr >> 22) & 0x3FF;
    
    // Compute the virtual address of the PDE using recursive mapping
    return reinterpret_cast<unsigned long*>(0xFFFFF000 | (pd_index << 2));  // PDE address
}

unsigned long * PageTable::PTE_address(unsigned long addr){
    // Extract the PDE index and PTE index from the logical address
    unsigned long pd_index = (addr >> 22) & 0x3FF;
    unsigned long pt_index = (addr >> 12) & 0x3FF;
    
    // Compute the virtual address of the PTE using recursive mapping
    return reinterpret_cast<unsigned long*>(0xFFC00000 | (pd_index << 12) | (pt_index << 2));  // PTE address
}

