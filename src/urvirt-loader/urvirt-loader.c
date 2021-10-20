#define _GNU_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <stub-image>\n", argv[0]);
        exit(1);
    }

    int img_fd = open(argv[1], O_RDONLY);
    struct stat img_stat;
    int res = fstat(img_fd, &img_stat);
    if (res == -1) {
        perror("fstat");
        exit(1);
    }

    size_t file_size = img_stat.st_size;

    // Round up to page size
    size_t file_size_up = (file_size + 4095) & (~4095);

    void *image_addr = mmap(
        NULL,
        file_size_up,
        PROT_EXEC | PROT_READ,
        MAP_PRIVATE,
        img_fd, 0
    );

    if (image_addr == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    if (close(img_fd) == -1) {
        perror("close");
        exit(1);
    }

    fprintf(stderr, "Entering stub...\n");
    fflush(stdout);
    fflush(stderr);
    ((stub_entrypoint_ptr) image_addr)(image_addr, file_size_up);
}
