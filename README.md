# MeMS: A Custom Memory Management System

MeMS is a C-based memory manager that demonstrates the core principles of dynamic memory allocation. It uses the `mmap` system call to request memory pages from the operating system and manages this memory with a segmented free-list strategy. This project provides a practical look into how standard library functions like `malloc` and `free` can be implemented.

## ✨ Features

-   **Dynamic Allocation**: Allocate memory of any size using `mems_malloc()`.
-   **Memory Deallocation**: Free allocated memory blocks with `mems_free()`.
-   **Efficient Memory Reuse**: Implements a **first-fit** algorithm to find and reuse free memory blocks.
-   **Hole Coalescing**: Automatically merges adjacent free blocks to reduce memory fragmentation.
-   **Page-Based Management**: Requests memory from the OS in 4096-byte pages and manages them internally.
-   **System Statistics**: Use `mems_print_stats()` for a detailed view of the memory layout, including page usage, fragmentation, and block sizes.
-   **Memory Inspection**: Retrieve the contents of allocated memory using `mems_get()`.

## 🚀 Getting Started

Follow these instructions to compile and run the project on a UNIX-like system.

### Prerequisites

-   A C compiler (e.g., `gcc`)
-   The `make` build automation tool

### Compilation

In your project directory, run the `make` command to compile the source code:

```bash
make
```

This will generate an executable file named `example`.

### Execution

Run the compiled program from your terminal:

```bash
./example
```

The output will demonstrate the memory allocation, data writing, statistics printing, and memory deallocation processes.
