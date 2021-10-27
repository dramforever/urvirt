#define _GNU_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common.h"

size_t get_max_va() {
    FILE *maps_file = fopen("/proc/self/maps", "r");
    if (maps_file == 0) {
        perror("fopen");
        return 0;
    }
    char buf[LINE_MAX];

    size_t addr_top = 0;

    while (fgets(buf, sizeof(buf), maps_file)) {
        if (buf[strlen(buf) - 1] != '\n') {
            fprintf(stderr, "Cannot read line\n");
            fclose(maps_file);
            return 0;
        }

        size_t region_begin, region_end;
        if (sscanf(buf, "%zx-%zx", &region_begin, &region_end) < 2) {
            fprintf(stderr, "Cannot read line\n");
            fclose(maps_file);
            return 0;
        }

        if (region_end > addr_top) {
            addr_top = region_end;
        }
    }

    if (! feof(maps_file)) {
        perror("fgets");
        fclose(maps_file);
        return 0;
    }

    fclose(maps_file);
    return addr_top;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <stub-image>\n", argv[0]);
        exit(1);
    }

    int img_fd = open(argv[1], O_RDONLY);

    struct stat img_stat;
    fstat(img_fd, &img_stat);

    size_t file_size = img_stat.st_size;

    // Round up to page size
    size_t file_size_up = (file_size + 4095) & (~4095);

    void *stub_addr = mmap(
        NULL,
        file_size_up,
        PROT_EXEC | PROT_READ,
        MAP_PRIVATE,
        img_fd, 0
    );

    close(img_fd);

    size_t max_va = get_max_va();

    int config_fd_orig = memfd_create("config_fd", 0);
    int ram_fd_orig = memfd_create("ram_fd", 0);

    ftruncate(config_fd_orig, CONF_SIZE);
    ftruncate(ram_fd_orig, RAM_SIZE);

    dup2(config_fd_orig, CONFIG_FD);
    dup2(ram_fd_orig, RAM_FD);

    close(config_fd_orig);
    close(ram_fd_orig);

    struct urvirt_config *conf = (struct urvirt_config *) mmap(
        NULL, CONF_SIZE,
        PROT_READ | PROT_WRITE, MAP_SHARED,
        CONFIG_FD, 0
    );

    conf->stub_start = stub_addr;
    conf->stub_size = file_size_up;
    conf->max_va = max_va;

    fprintf(stderr, "max_va = %zx\n", max_va);

    munmap(conf, CONF_SIZE);

    fprintf(stderr, "Entering stub...\n");
    fflush(stdout);
    fflush(stderr);
    ((stub_entrypoint_ptr) stub_addr)();
}
