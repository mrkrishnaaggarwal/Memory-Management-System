#include "mems.h"
#include <stdio.h>

int main(int argc, char const *argv[])
{
    // Initialize the MeMS system
    mems_init();
    int* ptr[10];

    // Allocate 10 arrays of 250 integers each
    printf("\n------- Allocating virtual addresses [mems_malloc] -------\n");
    for(int i=0; i<10; i++){
        ptr[i] = (int*)mems_malloc(sizeof(int) * 250);
        if (ptr[i] != NULL) {
            printf("Virtual address for ptr[%d]: %lu\n", i, (unsigned long)ptr[i]);
        }
    }

    // Access and modify data using the MeMS virtual address space
    printf("\n------ Accessing and writing to a virtual address [mems_get] -----\n");
    // Get the physical address corresponding to the second element of the first array
    int* phy_ptr_1 = (int*) mems_get(&ptr[0][1]);
    
    // Write a value to the target memory location
    *phy_ptr_1 = 200;

    // Get the physical address of the base of the array
    int* phy_ptr_0 = (int*) mems_get(&ptr[0][0]);
    printf("Virtual base address: %lu\tPhysical base address: %lu\n", (unsigned long)ptr[0], (unsigned long)phy_ptr_0);
    
    // Verify the written value by accessing it via the base pointer
    printf("Value at index [1]: %d\n", phy_ptr_0[1]);

    // Display the current memory statistics
    printf("\n--------- Printing memory stats [mems_print_stats] --------\n");
    mems_print_stats();

    // Demonstrate freeing and re-allocating memory
    printf("\n--------- Freeing and re-allocating a segment [mems_free] --------\n");
    printf("Freeing ptr[3]...\n");
    mems_free(ptr[3]);
    mems_print_stats();
    
    printf("\nRe-allocating space for ptr[3]...\n");
    ptr[3] = (int*)mems_malloc(sizeof(int) * 250);
    mems_print_stats();

    // Clean up and release all memory used by MeMS
    printf("\n--------- Unmapping all memory [mems_finish] --------\n\n");
    mems_finish();

    return 0;
}