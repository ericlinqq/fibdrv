#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"
#define SAMPLE_SIZE 1000

static inline double test(int fd, char write_buf[], int size)
{
    double t[SAMPLE_SIZE] = {};
    double mean = 0.0, std = 0.0, result = 0.0;
    int count = 0;

    for (int j = 0; j < SAMPLE_SIZE; j++) {
        t[j] = (double) write(fd, write_buf, size);
        mean += t[j];
    }
    mean /= SAMPLE_SIZE;

    for (int j = 0; j < SAMPLE_SIZE; j++)
        std += (t[j] - mean) * (t[j] - mean);

    std = sqrt(std / (SAMPLE_SIZE - 1));

    for (int j = 0; j < SAMPLE_SIZE; j++) {
        if (t[j] >= mean - 2 * std && t[j] <= mean + 2 * std) {
            result += t[j];
            count++;
        }
    }
    result /= count;
    return result;
}

int main()
{
    char write_buf[] = "testing writing";
    int offset = 100; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        double naive_dynamic = test(fd, write_buf, 0);
        double naive_static = test(fd, write_buf, 1);
        double fdoubling = test(fd, write_buf, 2);
        double fdoubling_clz = test(fd, write_buf, 3);
        printf("%d %.5lf %.5lf %.5lf %.5lf\n", i, naive_dynamic, naive_static,
               fdoubling, fdoubling_clz);
    }

    close(fd);
    return 0;
}
