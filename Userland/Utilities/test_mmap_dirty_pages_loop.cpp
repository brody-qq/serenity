#include <LibMain/Main.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <LibCore/System.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

ErrorOr<int> serenity_main(Main::Arguments args)
{
    if (args.argc < 2 || args.argc > 3) {
        printf("error: usage is %s <file> [test-private-mmaps] \n", args.argv[0]);
        return 1;
    }

    int fd = open(args.argv[1], O_RDWR);
    if (fd == -1) {
        perror(nullptr);
        return 1;
    }

    unsigned int shared_or_priv_flag = (args.argc == 3) ? MAP_PRIVATE : MAP_SHARED;
    auto stat= TRY(Core::System::fstat(fd));
    // NOTE msync is not happy unless the size is a multiple of 4096
    size_t size = stat.st_size - (stat.st_size % 4096);
    char* mmap_region = (char*)mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_FILE | shared_or_priv_flag, fd, 0);
    close(fd);

    printf("PID %d\n", getpid());

    unsigned int val = 0;
    unsigned int offset = 0;
    for (;;) {
        sleep(1);

        char *ptr = mmap_region + offset;
        *ptr = val++;

        offset += 4096;
        if(offset >= size) {
            offset = 0;
            auto res = msync(mmap_region, size, MS_SYNC);
            printf("msync result: %d\n", res);
            if (res == -1) {
                printf("%s\n", strerror(errno));
                return 1;
            }
        }
    }
    return 0;
}
