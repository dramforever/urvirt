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

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <stub-image> <kernel-image>\n", argv[0]);
        exit(1);
    }

    int stub_img_fd = open(argv[1], O_RDONLY);

    struct stat img_stat;
    fstat(stub_img_fd, &img_stat);

    size_t file_size = img_stat.st_size;

    // Round up to page size
    size_t file_size_up = (file_size + 4095) & (~4095);

    void *stub_addr = mmap(
        NULL,
        file_size_up,
        PROT_EXEC | PROT_READ,
        MAP_PRIVATE,
        stub_img_fd, 0
    );

    close(stub_img_fd);

    int kernel_img_fd = open(argv[2], O_RDONLY);
    fstat(kernel_img_fd, &img_stat);

    size_t kernel_size = img_stat.st_size;

    dup2(kernel_img_fd, KERNEL_FD);
    close(kernel_img_fd);

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
    conf->kernel_size = kernel_size;

    munmap(conf, CONF_SIZE);

    fprintf(stderr, "[urvirt] Entering urvirt-stub\n");
    fflush(stdout);
    fflush(stderr);
    ((stub_entrypoint_ptr) stub_addr)();
}
