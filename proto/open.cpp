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
    const char* path = "/home/milleniumbug/dokumenty/PROJEKTY/InDevelopment/OS_Projekt_WTFS/asdf/asdf/koles";
    int fd = open(path, O_RDWR|O_CREAT);
    printf("open: %s\n", strerror(errno));
    char buf[4096] = { 0 };
    read(fd, buf, sizeof buf);
    printf("read: %s\n", strerror(errno));
    off_t newoff = lseek(fd, 0, SEEK_END);
    printf("lseek: %s\n", strerror(errno));
    close(fd);
}
