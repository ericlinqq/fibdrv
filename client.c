#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"
#define BUF_SIZE 1000

int main()
{
    long long sz;

    char buf[BUF_SIZE];
    char write_buf[] = "testing writing";
    int offset = 1000; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        sz = write(fd, write_buf, strlen(write_buf));
        printf("Writing to " FIB_DEV ", returned the sequence %lld\n", sz);
    }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, BUF_SIZE);
        if (sz)
            printf("[Truncated]\t");
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               i, buf);
    }

    // for (int i = offset; i >= 0; i--) {
    //     lseek(fd, i, SEEK_SET);
    //     sz = read(fd, buf, 1);
    //     if (sz)
    //         printf("[Truncated]\t");
    //     printf("Reading from " FIB_DEV
    //            " at offset %d, returned the sequence "
    //            "%s.\n",
    //            i, buf);
    // }

    close(fd);
    return 0;
}
