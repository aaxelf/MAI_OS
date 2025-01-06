#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <time.h>

typedef struct {
    void* (*allocator_create)(void* memory, size_t size);
    void (*allocator_destroy)(void* allocator);
    void* (*allocator_alloc)(void* allocator, size_t size);
    void (*allocator_free)(void* allocator, void* memory);
} AllocatorAPI;

void* std_allocator_create(void* memory, size_t size) {
    (void)size;
    (void)memory;
    return memory;
}

void std_allocator_destroy(void* const allocator) {
    (void)allocator;
}

void* std_allocator_alloc(void* const allocator, const size_t size) {
    u_int32_t* memory = mmap(NULL, size, PROT_READ | PROT_WRITE, 
                                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED) {
        return NULL;
    }
    *memory = (u_int32_t)(size + sizeof(u_int32_t));
    return memory + 1;
}

void std_allocator_free(void* const allocator, void* const memory) {
    if (memory == NULL) {
        return;
    }
    u_int32_t* mem = (u_int32_t*)memory - 1;
    munmap(mem, *mem);
}

void my_itoa(size_t value, char* buffer, int base) {
    char* digits = "0123456789";
    char temp[20];
    int i = 0;

    if (value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }

    while (value > 0) {
        temp[i++] = digits[value % base];
        value /= base;
    }

    for (int j = 0; j < i; j++) {
        buffer[j] = temp[i - j - 1];
    }
    buffer[i] = '\0';
}

void my_ftoa(double value, char* buffer, int precision) {
    size_t int_part = (size_t)value;
    my_itoa(int_part, buffer, 10);

    while (*buffer) {
        buffer++;
    }

    *buffer++ = '.';

    double frac_part = value - (double)int_part;
    for (int i = 0; i < precision; i++) {
        frac_part *= 10;
        int digit = (int)frac_part;
        *buffer++ = '0' + digit;
        frac_part -= digit;
    }

    *buffer = '\0';
}

void write_message(const char* msg1, size_t number, const char* msg2, double time) {
    char buffer[256];
    char number_str[20];
    char time_str[20];

    my_itoa(number, number_str, 10);
    my_ftoa(time, time_str, 6);

    size_t len1 = strlen(msg1);
    size_t len2 = strlen(number_str);
    size_t len3 = strlen(msg2);
    size_t len4 = strlen(time_str);

    memcpy(buffer, msg1, len1);
    memcpy(buffer + len1, number_str, len2);
    memcpy(buffer + len1 + len2, msg2, len3);
    memcpy(buffer + len1 + len2 + len3, time_str, len4);

    buffer[len1 + len2 + len3 + len4] = '\n';
    write(STDOUT_FILENO, buffer, len1 + len2 + len3 + len4 + 1);
}

int main(int argc, char *argv[]) {
    void* handle = NULL;
    AllocatorAPI api;

    if (argc > 1) {
        handle = dlopen(argv[1], RTLD_LAZY);
        if (!handle) {
            char* msg = "cannot open library\n";
            write(STDOUT_FILENO, msg, strlen(msg));
            exit(EXIT_FAILURE);
        }

        api.allocator_create = dlsym(handle, "allocator_create");
        api.allocator_destroy = dlsym(handle, "allocator_destroy");
        api.allocator_alloc = dlsym(handle, "allocator_alloc");
        api.allocator_free = dlsym(handle, "allocator_free");

        if (!api.allocator_create || !api.allocator_destroy || !api.allocator_alloc || !api.allocator_free) {
            char* msg = "standart allocator error\n";
            write(STDOUT_FILENO, msg, strlen(msg));
            exit(EXIT_FAILURE);
        }
    } else {
        api.allocator_create = std_allocator_create;
        api.allocator_destroy = std_allocator_destroy;
        api.allocator_alloc = std_allocator_alloc;
        api.allocator_free = std_allocator_free;
    }

    size_t memory_size = 4096;
    size_t size_data = 2048;

    void *memory = mmap(NULL, memory_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED) {
        char* msg = "mmap error\n";
        write(STDOUT_FILENO, msg, strlen(msg));
        exit(EXIT_FAILURE);
    }

    void *allocator = api.allocator_create(memory, memory_size);
    if (!allocator) {
        char* msg = "mmap error\n";
        write(STDOUT_FILENO, msg, strlen(msg));
        munmap(memory, memory_size);
        exit(EXIT_FAILURE);
    }

    clock_t start, end;
    double time_used;

    start = clock();
    void *ptr1 = api.allocator_alloc(allocator, size_data);
    end = clock();
    time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    write_message("time to allocate ", size_data, " bytes: ", time_used);

    start = clock();
    api.allocator_free(allocator, ptr1);
    end = clock();
    time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    write_message("time to free ", size_data, " bytes: ", time_used);

    api.allocator_destroy(allocator);

    munmap(memory, memory_size);

    if (handle) {
        dlclose(handle);
    }

    return 0;
}