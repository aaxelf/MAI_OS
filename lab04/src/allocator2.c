#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define SIZE_DATA 32

typedef struct BuddyBlock {
    size_t size;
    struct BuddyBlock *next;
    struct BuddyBlock *prev;
    int is_free;
} BuddyBlock;

typedef struct {
    void* memory;
    size_t total_size;
    BuddyBlock* free_lists[SIZE_DATA];
} BuddyAllocator;

void* allocator_create(void* const memory, const size_t size) {
    BuddyAllocator* allocator = (BuddyAllocator*)mmap(NULL, sizeof(BuddyAllocator), 
                                                    PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (allocator == MAP_FAILED) {
        char* msg = "mmap error, cannot create allocator\n";
        write(STDOUT_FILENO, msg, strlen(msg));
        return NULL;
    }

    allocator->memory = memory;
    allocator->total_size = size;

    for (int i = 0; i < SIZE_DATA; i++)
        allocator->free_lists[i] = NULL;

    BuddyBlock* initial_block = (BuddyBlock*)memory;
    initial_block->size = size;
    initial_block->next = NULL;
    initial_block->prev = NULL;
    initial_block->is_free = 1;

    int idx = 0;
    size_t block_size = 1;
    while (block_size < size) {
        block_size <<= 1;
        idx++;
    }

    initial_block->next = allocator->free_lists[idx];
    if (allocator->free_lists[idx]) {
        allocator->free_lists[idx]->prev = initial_block;
    }
    allocator->free_lists[idx] = initial_block;

    return allocator;
}

void allocator_destroy(void* const allocator) {
    if (!allocator) {
        return;
    }

    if (munmap(allocator, sizeof(BuddyAllocator)) == -1) {
        char* msg = "munmap error\n";
        write(STDOUT_FILENO, msg, strlen(msg));
        exit(EXIT_FAILURE);
    }
}

void* allocator_alloc(void* const allocator, const size_t size) {
    BuddyAllocator* buddy_allocator = (BuddyAllocator*)allocator;
    size_t block_size = 1;
    int order = 0;

    while (block_size < size) {
        block_size <<= 1;
        order++;
    }

    BuddyBlock* block = NULL;
    for (int i = order; i < SIZE_DATA; i++) {
        if (buddy_allocator->free_lists[i]) {
            block = buddy_allocator->free_lists[i];
            order = i;
            break;
        }
    }

    if (!block) {
        return NULL;
    }

    if (block->next) {
        block->next->prev = block->prev;
    }
    if (block->prev) {
        block->prev->next = block->next;
    } else {
        buddy_allocator->free_lists[order] = block->next;
    }

    while (order > 0 && block_size > size) {
        order--;
        block_size >>= 1;

        BuddyBlock* buddy = (BuddyBlock*)((char*)block + block_size);
        buddy->size = block_size;
        buddy->next = buddy_allocator->free_lists[order];
        buddy->prev = NULL;
        buddy->is_free = 1;

        if (buddy_allocator->free_lists[order]) { 
            buddy_allocator->free_lists[order]->prev = buddy;
        }
        buddy_allocator->free_lists[order] = buddy;
    }

    block->is_free = 0;
    return block;
}

void allocator_free(void* const allocator, void* const memory) {
    BuddyAllocator* buddy_allocator = (BuddyAllocator*)allocator;
    BuddyBlock* block = (BuddyBlock*)memory;
    block->is_free = 1;

    while (1) {
        size_t block_size = block->size;
        char* block_addr = (char*)block;
        ptrdiff_t offset = (block_addr - (char*)buddy_allocator->memory);
        char* buddy_addr = (char*)buddy_allocator->memory + (offset ^ block_size);

        if (buddy_addr < (char*)buddy_allocator->memory || buddy_addr >= (char*)buddy_allocator->memory + buddy_allocator->total_size)
            break;

        BuddyBlock* buddy = (BuddyBlock*)buddy_addr;

        if (buddy->is_free && buddy->size == block_size) {
            if (buddy->next) 
                buddy->next->prev = buddy->prev;
            if (buddy->prev) 
                buddy->prev->next = buddy->next;
            else {
                int order = 0;
                size_t temp_size = block_size;
                while (temp_size >>= 1) {
                    order++;
                }
                buddy_allocator->free_lists[order] = buddy->next;
            }

            if (block < buddy) {
                block->size <<= 1;
            } else {
                buddy->size <<= 1;
                block = buddy;
            }
        } else {
            break;
        }
    }

    int order = 0;
    size_t block_size = block->size;
    while (block_size >>= 1) {
        order++;
    }

    block->next = buddy_allocator->free_lists[order];
    block->prev = NULL;
    if (buddy_allocator->free_lists[order]) {
        buddy_allocator->free_lists[order]->prev = block;
    }
    buddy_allocator->free_lists[order] = block;
}