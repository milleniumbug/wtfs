#define _FILE_OFFSET_BITS 64
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>

int main()
{
    const char* path = "/dev/sdc3";
    int fd = open(path, O_RDWR);
    printf("open: %s\n", strerror(errno));
    char buf[4096] = { 0 };
    read(fd, buf, sizeof buf);
    printf("read: %s\n", strerror(errno));
    for(int i = 0; i < 4096; ++i)
        printf("%02hhX ", buf[i]);
    off_t newoff = lseek(fd, 0, SEEK_END);
    printf("lseek: %s\n", strerror(errno));
    printf("%" PRIdMAX "B\n", (intmax_t)newoff);
    printf("%" PRIdMAX "KB\n", (intmax_t)newoff/1024);
    printf("%" PRIdMAX "MB\n", (intmax_t)newoff/1024/1024);
    printf("%" PRIdMAX "GB\n", (intmax_t)newoff/1024/1024/1024);
    printf("%" PRIdMAX "TB\n", (intmax_t)newoff/1024/1024/1024/1024);
    close(fd);
}
