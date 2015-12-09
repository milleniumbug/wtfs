#define _FILE_OFFSET_BITS 64
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>

void test_read(const char* path, size_t size)
{
    int fd = open(path, O_RDWR);
    printf("open: %s\n", strerror(errno));
    char buf[4096] = { 0 };
    {
        int r = read(fd, buf, size < sizeof buf ? size : sizeof buf);
        printf("read: %d %s %s\n", r, strerror(errno), buf);
    }
    close(fd);
}

int main()
{
    const char* path = "/home/milleniumbug/dokumenty/PROJEKTY/InDevelopment/OS_Projekt_WTFS/asdf/asdf/koles";
    test_read(path, 0);
    test_read(path, 3);
    test_read(path, 5);
    test_read(path, 10);
    test_read(path, 4000);
}
