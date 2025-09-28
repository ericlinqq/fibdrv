#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

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
        long long naive_dynamic = write(fd, write_buf, 0);
        long long naive_static = write(fd, write_buf, 1);
        long long fdoubling = write(fd, write_buf, 2);
        long long fdoubling_clz = write(fd, write_buf, 3);
        printf("%d %lld %lld %lld %lld\n", i, naive_dynamic, naive_static,
               fdoubling, fdoubling_clz);
    }

    close(fd);
    return 0;
}
