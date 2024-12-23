#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>

typedef struct TASK {
    int low;
    int high;
    int busy;
    int* a;
} TASK;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void insertion_sort(int* a, int left, int right) {
    for (int i = left + 1; i <= right; i++) {
        int temp = a[i];
        int j = i - 1;
        while (j >= left && a[j] > temp) {
            a[j + 1] = a[j];
            j--;
        }
        a[j + 1] = temp;
    }
}

void merge(int* a, int left, int mid, int right) {
    int n1 = mid - left + 1;
    int n2 = right - mid;

    int* left_array = (int*)malloc(n1 * sizeof(int));
    int* right_array = (int*)malloc(n2 * sizeof(int));

    for (int i = 0; i < n1; i++) {
        left_array[i] = a[left + i];
    }
    for (int i = 0; i < n2; i++) {
        right_array[i] = a[mid + 1 + i];
    }

    int i = 0, j = 0, k = left;

    while (i < n1 && j < n2) {
        if (left_array[i] <= right_array[j]) {
            a[k++] = left_array[i++];
        } else {
            a[k++] = right_array[j++];
        }
    }

    while (i < n1) {
        a[k++] = left_array[i++];
    }

    while (j < n2) {
        a[k++] = right_array[j++];
    }

    free(left_array);
    free(right_array);
}

void timsort(int* a, int n) {
    int run = 32;
    for (int i = 0; i < n; i += run) {
        insertion_sort(a, i, (i + run - 1 < n - 1) ? i + run - 1 : n - 1);
    }

    for (int size = run; size < n; size = 2 * size) {
        for (int left = 0; left < n; left += 2 * size) {
            int mid = left + size - 1;
            int right = (left + 2 * size - 1 < n - 1) ? left + 2 * size - 1 : n - 1;

            if (mid < right) {
                merge(a, left, mid, right);
            }
        }
    }
}

void* timsort_thread(void* arg) {
    TASK* task = (TASK*)arg;
    timsort(task->a + task->low, task->high - task->low + 1);

    pthread_mutex_lock(&mutex);
    task->busy = 0;
    pthread_mutex_unlock(&mutex);

    return NULL;
}

long get_time_in_milliseconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

void write_message(int fd, const char* message) {
    size_t len = 0;
    while (message[len] != '\0') {
        len++;
    }
    write(fd, message, len);
}

void write_number_message(int fd, const char* prefix, long number, const char* suffix) {
    char buffer[128];
    char num_buffer[64];
    size_t prefix_len = 0, suffix_len = 0, num_len = 0;

    while (prefix[prefix_len] != '\0') {
        prefix_len++;
    }
    while (suffix[suffix_len] != '\0') {
        suffix_len++;
    }

    long temp = number;
    if (temp == 0) {
        num_buffer[num_len++] = '0';
    } else {
        while (temp > 0) {
            num_buffer[num_len++] = (temp % 10) + '0';
            temp /= 10;
        }
    }

    for (size_t i = 0; i < num_len / 2; i++) {
        char tmp = num_buffer[i];
        num_buffer[i] = num_buffer[num_len - 1 - i];
        num_buffer[num_len - 1 - i] = tmp;
    }

    size_t total_len = prefix_len + num_len + suffix_len;
    size_t pos = 0;

    for (size_t i = 0; i < prefix_len; i++) {
        buffer[pos++] = prefix[i];
    }
    for (size_t i = 0; i < num_len; i++) {
        buffer[pos++] = num_buffer[i];
    }
    for (size_t i = 0; i < suffix_len; i++) {
        buffer[pos++] = suffix[i];
    }

    write(fd, buffer, total_len);
}

int main(int argc, char** argv) {
    int max_array_elements = 100000000;
    int max_threads;

    if (argc < 2) {
        write_message(STDERR_FILENO, "usage: program thread_count\n");
        exit(EXIT_SUCCESS);
    }

    if (argc == 2) {
        max_threads = atoi(argv[1]);
    }

    time_t raw_time;

    write_number_message(STDERR_FILENO, "array[", max_array_elements, "]\n");
    write_number_message(STDERR_FILENO, "threads[", max_threads, "]\n");

    int* array = (int*)malloc(sizeof(int) * max_array_elements);

    srand((unsigned)time(NULL));
    for (int i = 0; i < max_array_elements; i++)
        array[i] = rand();

    write_message(STDERR_FILENO, "array randomized\n");

    pthread_t* threads = (pthread_t*)malloc(sizeof(pthread_t) * max_threads);
    TASK* tasklist = (TASK*)malloc(sizeof(TASK) * max_threads);

    int len = max_array_elements / max_threads;
    int low = 0;

    long start_time_ms = get_time_in_milliseconds();
    
    time(&raw_time);
    write_message(STDERR_FILENO, "now time is: ");
    write_message(STDERR_FILENO, ctime(&raw_time));

    for (int i = 0; i < max_threads; i++, low += len) {
        TASK* task = &tasklist[i];
        task->a = array;
        task->busy = 1;
        task->low = low;
        task->high = (i == max_threads - 1) ? max_array_elements - 1 : low + len - 1;

        pthread_create(&threads[i], 0, timsort_thread, task);
    }

    for (int i = 0; i < max_threads; i++)
        pthread_join(threads[i], NULL);

    pthread_mutex_lock(&mutex);
    TASK* taskm = &tasklist[0];
    for (int i = 1; i < max_threads; i++) {
        TASK* task = &tasklist[i];
        merge(taskm->a, taskm->low, task->low - 1, task->high);
    }
    pthread_mutex_unlock(&mutex);

    long end_time_ms = get_time_in_milliseconds();

    time(&raw_time);
    write_message(STDERR_FILENO, "now time is: ");
    write_message(STDERR_FILENO, ctime(&raw_time));

    write_number_message(STDERR_FILENO, "array sorted in ", end_time_ms - start_time_ms, " ms\n");

    free(tasklist);
    free(threads);
    free(array);

    pthread_mutex_destroy(&mutex);

    return 0;
}
