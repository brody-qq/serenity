#include <LibMain/Main.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <LibCore/System.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <AK/Random.h>
#include <serenity.h>

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

    // write loop
    // this will cause write-faults on clean pages, and page-not-present-faults + write-faults on evicted pages
    pid_t pid = fork();
    switch (pid) {
    case -1:
        perror(nullptr);
        return 1;
        break;
    case 0: {
        int i = 0;
        char c = 'x';
        while(1) {
            usleep(700 * 1000);
            mmap_region[i] = c;
            printf("WRITE_LOOP: wrote value %c to index %d\n", c, i);
            i = (i + get_random_uniform(size)) % size;
        }
        break;
    }
    default:
        break;
    }

    // read loop
    // this will cause page-not-present faults on evicted pages
    pid = fork();
    switch (pid) {
    case -1:
        perror(nullptr);
        return 1;
        break;
    case 0: {
        int i = 0;
        while(1) {
            usleep(300 * 1000);
            char c = mmap_region[i];
            printf("READ_LOOP: read value %c from index %d\n", c, i);
            i = (i + get_random_uniform(size)) % size;
        }
        break;
    }
    default:
        break;
    }

    // msync loop
    // this will mark dirty pages as clean and write them to storage
    pid = fork();
    switch (pid) {
    case -1:
        perror(nullptr);
        return 1;
        break;
    case 0: {
        int i = 0;
        int pages = size / 4096;
        while(1) {
            usleep(1100 * 1000);
            msync(mmap_region + i*4096, 4096, MS_SYNC);
            printf("MSYNC_LOOP: msync()ed page %d\n", i);
            i = (i + get_random_uniform(pages)) % pages;
        }
        break;
    }
    default:
        break;
    }

    // purge loop
    // this will evict clean pages from memory
    pid = fork();
    switch (pid) {
    case -1:
        perror(nullptr);
        return 1;
        break;
    case 0:
        while(1) {
            usleep(500 * 1000);
            ::purge(PURGE_ALL_CLEAN_INODE);
            printf("PURGE_LOOP: called purge()\n");
        }
        break;
    default:
        break;
    }

    // keep parent process alive
    wait(NULL);

    return 0;
}
