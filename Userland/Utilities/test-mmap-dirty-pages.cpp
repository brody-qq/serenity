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
    if (size < 4096) {
        printf("error: input file is too small, must be at least 4096 bytes large\n");
        return 1;
    }
    char* mmap_region = (char*)mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_FILE | shared_or_priv_flag, fd, 0);
    close(fd);

    auto write_loop = [&] (int increment = 1) {
        int len = 6;
        char str[len] = {'_', 'T', 'E', 'S', 'T', '_'};
        unsigned int offset = 0;
        while (offset < size) {
            sleep(1);

            char *ptr = mmap_region + offset;
            if(offset + len <= size)
                memcpy(ptr, str, len);

            offset += 4096 * increment;
        }
    };

    printf(">> test dirtying pages, then cleaning them with msync\n");
    printf("dirtying every page...\n");
    write_loop();
    printf("cleaning pages with msync\n");
    auto rc = msync(mmap_region, size, MS_SYNC);
    printf("msync result: %d\n", rc);
    if (rc == -1) {
        printf("%s\n", strerror(errno));
        return 1;
    }

    printf("dirtying every 2nd page...\n");
    write_loop(2);
    printf("cleaning pages with msync\n");
    rc = msync(mmap_region, size, MS_SYNC);
    printf("msync result: %d\n", rc);
    if (rc == -1) {
        printf("%s\n", strerror(errno));
        return 1;
    }

    printf(">> test writing to already dirty pages, only the 1st writes should cause page fault\n");
    printf("write_loop() 1\n");
    write_loop();
    printf("write_loop() 2\n");
    write_loop();
    printf("write_loop() 3\n");
    write_loop();
    printf("cleaning pages with msync\n");
    rc = msync(mmap_region, size, MS_SYNC);
    printf("msync result: %d\n", rc);
    if (rc == -1) {
        printf("%s\n", strerror(errno));
        return 1;
    }

    volatile char read_placeholder = 0;
    auto read_loop = [&] () {
        unsigned int offset = 0;
        while (offset < size) {
            sleep(1);

            char *ptr = mmap_region + offset;
            read_placeholder = *ptr;

            offset += 4096;
        }
    };

    printf(">> test reading from clean pages this should not cause page faults unless \'purge -c\' is run in a different shell\n");
    printf("read_loop() 1\n");
    read_loop();
    printf("read_loop() 2\n");
    read_loop();
    printf("read_loop() 3\n");
    read_loop();

    printf(">> test running munmap on region with dirty pages\n");
    printf("dirtying every page...\n");
    write_loop();
    printf("cleaning pages with munmap\n");
    rc = munmap(mmap_region, size);
    printf("munmap result: %d\n", rc);
    if (rc == -1) {
        printf("%s\n", strerror(errno));
        return 1;
    }

    return 0;
}
