#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <memory>
#include <cstdio>

int main()
{
    const char* path = "/dev/sdb3";
    int fd = open(path, O_RDWR);
    perror("open");
    auto* f = static_cast<unsigned char*>(mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    perror("mmap");
    for(int i = 0; i < 4096; ++i)
        printf("%02hhX ", f[i]);
    munmap(f, 4096);
    close(fd);
}
