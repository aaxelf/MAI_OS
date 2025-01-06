#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define MAX_BLOCK_SIZE 4096
#define MIN_BLOCK_SIZE 16
#define SIZE_DATA 32

typedef struct FreeBlock {
    struct FreeBlock* next;
} FreeBlock;

typedef struct {
    FreeBlock* free_lists[SIZE_DATA];
    void* memory_start;
    size_t total_size;
    size_t cur_pos;
} MKKAllocator;

int get_free_list_index(size_t size) {
    int idx = 0;
    size_t cur_size = MIN_BLOCK_SIZE;
    while (cur_size < size && cur_size <= MAX_BLOCK_SIZE) {
        cur_size *= 2;
        idx++;
    }
    return idx;
}

MKKAllocator* allocator_create(void *const memory, size_t size) {
    if (size > MAX_BLOCK_SIZE) {
        char* msg = "enter smaller size\n";
        write(STDOUT_FILENO, msg, strlen(msg));
        return NULL;
    }

    MKKAllocator* allocator = mmap(NULL, sizeof(MKKAllocator), PROT_READ | PROT_WRITE, 
                                                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (allocator == MAP_FAILED) {
        char* msg = "mmap error, cannot create allocator\n";
        write(STDOUT_FILENO, msg, strlen(msg));
        return NULL;
    }

    allocator->memory_start = (MKKAllocator*)memory;
    allocator->total_size = size - sizeof(MKKAllocator);
    allocator->cur_pos = 0;

    memset(allocator->free_lists, 0, sizeof(allocator->free_lists)); 
    return allocator;
}

void* allocator_alloc(MKKAllocator* allocator, size_t size) {
    if (!allocator || size <= 0 || size > MAX_BLOCK_SIZE) {
        return NULL;
    }

    int idx = get_free_list_index(size);
    if (!allocator->free_lists[idx]) {
        size_t block_size = MIN_BLOCK_SIZE << idx;
        if (allocator->cur_pos + block_size > allocator->total_size) {
            char* msg = "not enough memory in allocator\n";
            write(STDOUT_FILENO, msg, strlen(msg));
            return NULL;
        }

        void *block = (char*)allocator->memory_start + allocator->cur_pos;
        allocator->cur_pos += block_size;
        
        return block;
    }

    FreeBlock* block = allocator->free_lists[idx];
    allocator->free_lists[idx] = block->next;

    return (void*)block;
}

void allocator_free(MKKAllocator* allocator, void* memory) {
    if (!allocator || !memory) {
        return;
    }

    size_t size = ((FreeBlock*)memory)->next ? MAX_BLOCK_SIZE : MIN_BLOCK_SIZE;
    int idx = get_free_list_index(size);
    FreeBlock* block = (FreeBlock*)memory;
    block->next = allocator->free_lists[idx];
    allocator->free_lists[idx] = block;
}

void allocator_destroy(MKKAllocator* allocator) {
    if (!allocator) {
        return;
    }
    allocator->memory_start = NULL;
    allocator->total_size = 0;

    memset(allocator->free_lists, 0, sizeof(allocator->free_lists));
}