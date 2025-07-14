/*
* mems.h
*
* This header file contains the function declarations and data structures
* for a custom memory management system (MeMS). It implements a memory
* allocator using mmap to request memory from the OS and manages it
* through a segmented free-list approach.
*/

#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

/*
* The page size for memory allocation. While this can differ between systems,
* it is defined as a macro for consistent behavior and evaluation.
*/
#define PAGE_SIZE 4096

// Constants to identify the type of a memory segment
#define HOLE 0
#define PROCESS 1

// The starting virtual address for the MeMS address space
#define START_VIRTUAL_ADDRESS 1000

// Represents a contiguous block of memory requested from the OS
struct main_node {
    int num_of_pages;
    void* p_addr;
    void* v_addr_start;
    void* v_addr_end;
    struct main_node* next;
    struct main_node* prev;
    struct sub_node* sub_head; // Head of the list of segments within this block
    int padding[2]; // Ensures the struct size is 64 bytes for alignment
};

// Represents a segment (process or hole) within a main_node block
struct sub_node {
    int type; // HOLE or PROCESS
    int size;
    void* p_addr;
    void* v_addr_start;
    void* v_addr_end;
    struct sub_node* next;
    struct sub_node* prev;
    int padding[4]; // Ensures the struct size is 64 bytes for alignment
};

// Global pointers for managing the linked lists of nodes
void* main_node_tracker;
void* sub_node_tracker;
void* current_main_node_map;
void* current_sub_node_map;

// Global head for the main chain of allocated memory blocks
struct main_node* head_main = NULL;
void* start_virtual_address = NULL;

void init_free_list() {
    main_node_tracker = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    sub_node_tracker = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (main_node_tracker == MAP_FAILED || sub_node_tracker == MAP_FAILED) {
        perror("mmap failed");
        exit(0);
    }
    
    current_main_node_map = main_node_tracker;
    current_sub_node_map = sub_node_tracker;
}

struct main_node* add_main_node() {
    // if no more nodes can be added to the current mmap page
    if (main_node_tracker + sizeof(struct main_node) > current_main_node_map + PAGE_SIZE) {
        current_main_node_map = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (current_main_node_map == MAP_FAILED) {
            perror("mmap failed");
        }
        main_node_tracker = current_main_node_map + sizeof(struct main_node);
        return (struct main_node*)current_main_node_map;
    // else use the current mmap page
    } else {
        struct main_node* new_main_node = (struct main_node*)main_node_tracker;
        main_node_tracker = main_node_tracker + sizeof(struct main_node);
        return new_main_node;
    }
}

struct sub_node* add_sub_node() {
    if (sub_node_tracker + sizeof(struct sub_node) > current_sub_node_map + PAGE_SIZE) {
        current_sub_node_map = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (current_sub_node_map == MAP_FAILED) {
            perror("mmap failed");
        }
        sub_node_tracker = current_sub_node_map + sizeof(struct sub_node);
        return (struct sub_node*)current_sub_node_map;
    } else {
        struct sub_node* new_main_node = (struct sub_node*)sub_node_tracker;
        sub_node_tracker = sub_node_tracker + sizeof(struct sub_node);
        return new_main_node;
    }
}

/*
 * Initializes the MeMS system, setting up the free list and
 * other necessary global variables.
 */
void mems_init() {
    init_free_list();
    head_main = add_main_node();
    head_main->num_of_pages = 0;
    head_main->next = head_main;
    head_main->prev = head_main;
    head_main->sub_head = NULL;
    start_virtual_address = (void *)START_VIRTUAL_ADDRESS;
    head_main->v_addr_start = start_virtual_address;
    head_main->v_addr_end = start_virtual_address-1;
}

/*
 * Deallocates all memory managed by the MeMS system.
 * It unmaps all memory regions previously obtained from the OS via mmap.
 */
void mems_finish() {
    struct main_node* current_main_node = head_main->next;
    while (current_main_node != head_main) {
        struct main_node* temp = current_main_node;
        current_main_node = current_main_node->next;
        if (munmap(temp->p_addr, temp->num_of_pages * PAGE_SIZE) == -1) {
            perror("munmap failed on mems_finish");
        }
    }
    head_main->next = head_main;
    // Note: The pages used for tracking nodes are not unmapped here
    // in this implementation, as they are managed by the OS heap.
    // A more robust implementation might track and free these as well.
}

/*
 * Allocates a memory segment of a specified size.
 * It attempts to find a suitable hole in the free list. If none is found,
 * it requests more memory from the OS using mmap.
 * @param size The number of bytes to allocate.
 * @return A MeMS virtual address to the start of the allocated segment, or NULL on failure.
 */
void* mems_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    struct main_node* current_main_node = head_main->next;
    // Search for a suitable hole in existing pages
    while (current_main_node != head_main) {
        struct sub_node* current_sub_node = current_main_node->sub_head;
        while (current_sub_node != NULL) {
            if (current_sub_node->type == HOLE && current_sub_node->size >= size) {
                if (current_sub_node->size > size + sizeof(struct sub_node)) {
                    // Split the hole
                    struct sub_node* new_hole = add_sub_node();
                    new_hole->type = HOLE;
                    new_hole->size = current_sub_node->size - (int)size;
                    new_hole->p_addr = (void*)(current_sub_node->p_addr + size);
                    new_hole->v_addr_start = (void*)(current_sub_node->v_addr_start + size);
                    new_hole->v_addr_end = current_sub_node->v_addr_end;
                    new_hole->next = current_sub_node->next;
                    new_hole->prev = current_sub_node;

                    if (current_sub_node->next != NULL) {
                        current_sub_node->next->prev = new_hole;
                    }
                    current_sub_node->next = new_hole;
                    current_sub_node->size = (int)size;
                    current_sub_node->v_addr_end = (void*)(current_sub_node->v_addr_start + size - 1); 
                }
                current_sub_node->type = PROCESS;
                return current_sub_node->v_addr_start;
            }
            current_sub_node = current_sub_node->next;
        }
        current_main_node = current_main_node->next;
    }

    // No suitable hole found, allocate new page(s)
    current_main_node = current_main_node->prev;
    int num_of_pages = ceil((double)size / (double)PAGE_SIZE);
    void* p_addr = mmap(NULL, num_of_pages * PAGE_SIZE, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (p_addr == MAP_FAILED) {
        perror("mmap failed on mems_malloc");
        return NULL;
    }

    struct main_node* new_main_node = add_main_node();
    new_main_node->p_addr = p_addr;
    new_main_node->num_of_pages = num_of_pages;
    new_main_node->v_addr_start = current_main_node->v_addr_end + 1;
    new_main_node->v_addr_end = new_main_node->v_addr_start + (num_of_pages * PAGE_SIZE) - 1;
    new_main_node->next = head_main;
    new_main_node->prev = current_main_node;
    current_main_node->next = new_main_node;
    head_main->prev = new_main_node;

    struct sub_node* new_sub_node = add_sub_node();
    new_sub_node->type = PROCESS;
    new_sub_node->size = (int)size;
    new_sub_node->p_addr = p_addr;
    new_sub_node->v_addr_start = new_main_node->v_addr_start;
    new_sub_node->v_addr_end = (void*)(new_sub_node->v_addr_start + size - 1);
    new_sub_node->next = NULL;
    new_sub_node->prev = NULL;

    // Create a new hole for the remaining space
    if (size < num_of_pages * PAGE_SIZE) {
        struct sub_node* new_hole = add_sub_node();
        new_hole->type = HOLE;
        new_hole->size = num_of_pages * PAGE_SIZE - (int)size;
        new_hole->p_addr = (void*)(p_addr + size);
        new_hole->v_addr_start = (void*)(new_sub_node->v_addr_start + size);
        new_hole->v_addr_end = new_main_node->v_addr_end;
        new_hole->next = NULL;
        new_hole->prev = new_sub_node;
        new_sub_node->next = new_hole;
    }
    
    new_main_node->sub_head = new_sub_node;
    return new_sub_node->v_addr_start;
}

/*
 * Prints statistics about the current state of the MeMS system,
 * including page usage, fragmentation, and memory layout.
 */
void mems_print_stats() {
    if (head_main->next == head_main) {
        printf("MeMS Status: No pages allocated.\n");
        return;
    }

    struct main_node* current_main_node = head_main->next;
    int total_pages = 0;
    int total_unused_size = 0;
    int main_chain_len = 0;
    printf("\n--- MeMS System Stats ---\n");
    while (current_main_node != head_main) {
        total_pages += current_main_node->num_of_pages;
        printf("MAIN[%lu:%lu]-> ", (uintptr_t)current_main_node->v_addr_start, (uintptr_t)current_main_node->v_addr_end);
        main_chain_len++;
        struct sub_node* current_sub_node = current_main_node->sub_head;
        while (current_sub_node != NULL) {
            if (current_sub_node->type == HOLE) {
                printf("H[%lu:%lu](%d) <-> ", (uintptr_t)current_sub_node->v_addr_start, (uintptr_t)current_sub_node->v_addr_end, current_sub_node->size);
                total_unused_size += current_sub_node->size;
            } else {
                printf("P[%lu:%lu](%d) <-> ", (uintptr_t)current_sub_node->v_addr_start, (uintptr_t)current_sub_node->v_addr_end, current_sub_node->size);
            }
            current_sub_node = current_sub_node->next;
        }
        current_main_node = current_main_node->next;
        printf("NULL\n");
    }
    printf("Pages used: %d\n", total_pages);
    printf("Space unused: %d bytes\n", total_unused_size);
    printf("Main chain length: %d\n", main_chain_len);
    printf("-------------------------\n");
}

/*
 * Translates a MeMS virtual address to its corresponding physical address.
 * @param v_ptr The MeMS virtual address to translate.
 * @return The corresponding physical address, or NULL if the address is invalid.
 */
void* mems_get(void* v_ptr) {
    struct main_node* current_main_node = head_main->next;
    while (current_main_node != head_main) {
        // Quick check to see if v_ptr is within this main node's range
        if (v_ptr >= current_main_node->v_addr_start && v_ptr <= current_main_node->v_addr_end) {
            struct sub_node* current_sub_node = current_main_node->sub_head;
            while (current_sub_node != NULL) {
                if (v_ptr >= current_sub_node->v_addr_start && v_ptr <= current_sub_node->v_addr_end) {
                    if (current_sub_node->type == PROCESS) {
                        return current_sub_node->p_addr + (v_ptr - current_sub_node->v_addr_start);
                    } else {
                        return NULL; // Address points to a hole
                    }
                }
                current_sub_node = current_sub_node->next;
            }
        }
        current_main_node = current_main_node->next;
    }
    return NULL; // Address not found in any managed segment
}

void merge_holes() {
    struct main_node* current_main_node = head_main->next;
    while (current_main_node != head_main) {
        struct sub_node* current_sub_node = current_main_node->sub_head;
        while (current_sub_node != NULL) {
            if (current_sub_node->type == HOLE && current_sub_node->next != NULL && current_sub_node->next->type == HOLE) {
                struct sub_node* next_hole = current_sub_node->next;
                current_sub_node->size += next_hole->size;
                current_sub_node->v_addr_end = next_hole->v_addr_end;
                current_sub_node->next = next_hole->next;
                if (next_hole->next != NULL) {
                    next_hole->next->prev = current_sub_node;
                }
                // In a production system, the `next_hole` struct itself would be returned to a pool
                continue; // Re-check the current node in case it can merge again
            }
            current_sub_node = current_sub_node->next;
        }
        current_main_node = current_main_node->next;
    }
}

/*
 * Frees a previously allocated memory segment.
 * The freed segment is marked as a HOLE and merged with adjacent holes.
 * @param v_ptr The MeMS virtual address of the segment to free.
 */
void mems_free(void* v_ptr) {
    if(v_ptr == NULL) return;

    struct main_node* current_main_node = head_main->next;
    while (current_main_node != head_main) {
        struct sub_node* current_sub_node = current_main_node->sub_head;
        while (current_sub_node != NULL) {
            if (current_sub_node->v_addr_start == v_ptr && current_sub_node->type == PROCESS) {
                current_sub_node->type = HOLE;
                merge_holes();
                return;
            }
            current_sub_node = current_sub_node->next;
        }
        current_main_node = current_main_node->next;
    }
}