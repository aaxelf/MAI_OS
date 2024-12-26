#define _XOPEN_SOURCE 900
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <wait.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <semaphore.h>

int print_int(const int num) {
    char buf[16];
    char res[32];
    int n = 0;
    int sign = (num < 0);
    int x = abs(num);
    if (x == 0) {
        write(STDOUT_FILENO, "0\n", 2);
        return 0;
    }
    while (x) {
        buf[n] = (x % 10) + '0';
        x = x / 10;
        n++;
    }
    if (sign) {
        res[0] = '-';
    }
    for (int i = 0; i < n; i++) {
        res[i + sign] = buf[n - i - 1];
    }
    res[n + sign] = '\n';
    write(STDOUT_FILENO, res, (n + sign + 1));
    return 0;
}

int read_line(char** buf, int* n){
    char c;
    int i = 0;
    while(1) {
        if (read(STDIN_FILENO, &c, sizeof(char)) == -1) {
            return -1;
        }
        (*buf)[i] = c;
        i++;
        if (i >= *n) {
            *buf = realloc(*buf, (*n) * 2 * sizeof(char));
            *n = *n * 2;
        }
        if (c == '\n') {
            (*buf)[i - 1] = '\0';
            break;
        }
    }
    return 0;
}

int main() {
    char* buf = malloc(128 * sizeof(char));
    int n = 128;
    if (read_line(&buf, &n) == -1) {
        char* msg = "fail to read file name\n";
        write(STDOUT_FILENO, msg, strlen(msg));
        exit(-1);
    }

    int file_fd = open(buf, O_RDONLY);
    if (file_fd == -1) {
        char* msg = "fail to open file\n";
        write(STDOUT_FILENO, msg, strlen(msg));
        exit(-1);
    }
    free(buf);

    int shm = shm_open("/memory", O_RDWR | O_CREAT, 0666);
    if (shm == -1) {
        char* msg = "fail to create shared memory\n";
        write(STDOUT_FILENO, msg, strlen(msg));
        exit(-1);
    }

    if (ftruncate(shm, sizeof(int) * 2) == -1) {
        char* msg = "fail to set size of shared memory\n";
        write(STDOUT_FILENO, msg, strlen(msg));
        exit(-1);
    };

    sem_t* sem_empty = sem_open("/semaphore_empty", O_CREAT, 0666, 1);
    if (sem_empty == SEM_FAILED) {
        char* msg = "fail to create semaphore\n";
        write(STDOUT_FILENO, msg, strlen(msg));
        exit(-1);
    }

    sem_t* sem_full = sem_open("/semaphore_full", O_CREAT, 0666, 0);
    if (sem_full == SEM_FAILED) {
        char* msg = "fail to create semaphore\n";
        write(STDOUT_FILENO, msg, strlen(msg));
        exit(-1);
    }

    __pid_t pid = fork();
    if (pid == -1) {
        char* msg = "fail to fork\n";
        write(STDOUT_FILENO, msg, strlen(msg));
        exit(-1);
    }

    if (pid == 0) {
        if (dup2(file_fd, STDIN_FILENO) == -1) {
            char* msg = "fail to reassign file descriptor\n";
            write(STDOUT_FILENO, msg, strlen(msg));
            exit(-1);
        }
        close(file_fd);
        
        char *arg[] = {"./child.out", NULL};
        if (execv("./child.out", arg) == -1) {
            char* msg = "fail to replace process image\n";
            write(STDOUT_FILENO, msg, strlen(msg));
            exit(-1);
        }
    } else {
        char* ptr = mmap(0, sizeof(int) * 2, PROT_READ|PROT_WRITE, MAP_SHARED, shm, 0);
        if (ptr == (char*)-1) {
            char* msg = "fail to memory map\n";
            write(STDOUT_FILENO, msg, strlen(msg));
            exit(-1);
        }

        memset(ptr, 0, sizeof(int) * 2);
        ptr[1] = 1;
        while (ptr[1] != 0) {
            sem_wait(sem_full);
            print_int(ptr[0]);
            sem_post(sem_empty);
        }

        waitpid(pid, 0, 0);
        shm_unlink("/memory");
    }
    return 0;
}