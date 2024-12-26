#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <wait.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <semaphore.h>

#define BUFFER_SIZE 512
#define _XOPEN_SOURCE 900

int is_num(char c) {
    return (c >= '0') && (c <= '9');
}

int is_space(char c) {
    return (c == ' ') || (c == '\t') || (c == '\n');
}

int is_prime(int num) {
    if (num < 2) {
        return 0;
    }
    
    for (int i = 2; i * i <= num; i++){
        if (num % i == 0) {
            return 0;
        }
    }
    return 1;
}

int process_line(int* res) {
    char buffer[BUFFER_SIZE];
    int index = 0;
    char c;
    int sign = 1;
    *res = 0;

    while(read(STDIN_FILENO, &c, sizeof(char)) > 0) {
        if (c == '-') {
            if (index != 0) {
                return -1;
            }
            sign = -1;
        } else if(is_num(c)) {
            *res = *res * 10 + (c - '0');
        } else if (is_space(c)) {
            break;
        } else {
            return -1;
        }
        index++;
    }
    *res *= sign;
    return (index > 0) ? 0 : -1;
}

int main() {
    int shm = shm_open("/memory", O_RDWR, 0666);
    if (shm == -1) {
        char* msg = "fail to open shared memory\n";
        write(STDOUT_FILENO, msg, strlen(msg));
        exit(-1);
    }

    sem_t* sem_empty = sem_open("/semaphore_empty", O_EXCL);
    if (sem_empty == SEM_FAILED) {
        char* msg = "fail to open semaphore\n";
        write(STDOUT_FILENO, msg, strlen(msg));
        exit(-1);
    }

    sem_t* sem_full = sem_open("/semaphore_full", O_EXCL);
    if (sem_full == SEM_FAILED) {
        char* msg = "fail to open semaphore\n";
        write(STDOUT_FILENO, msg, strlen(msg));
        exit(-1);
    }

    char* ptr = mmap(0, sizeof(int) * 2, PROT_READ|PROT_WRITE, MAP_SHARED, shm, 0);
    int num = 0;

    while (process_line(&num) != -1) {
        if (num <= 0) {
            sem_wait(sem_empty);
            ptr[0] = 0;
            sem_post(sem_full);
            break;
        }
        
        if (!is_prime(num)) {
            sem_wait(sem_empty);
            ptr[0] = num;
            sem_post(sem_full);
        } else {
            sem_wait(sem_empty);
            ptr[0] = 0;
            sem_post(sem_full);
            break;
        }
    }
    ptr[1] = 0;
    close(STDOUT_FILENO);
    return 0;
}