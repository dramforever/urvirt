#include "seccomp-bpf.h"

#include <stddef.h>

#define SC_OFF(name) (offsetof(struct seccomp_data, name))
#define SC_IP_LOW SC_OFF(instruction_pointer)
#define SC_IP_HIGH (SC_IP_LOW + 4)

size_t gen_addr_filter(size_t start_addr, size_t end_addr, struct sock_filter *out) {
    uint32_t start_addr_high = start_addr >> 32, start_addr_low = start_addr;
    uint32_t end_addr_high = end_addr >> 32, end_addr_low = end_addr;

#define load_high BPF_STMT(BPF_LD | BPF_W | BPF_ABS, SC_IP_HIGH)
#define load_low BPF_STMT(BPF_LD | BPF_W | BPF_ABS, SC_IP_LOW)

    struct sock_filter filt[] = {
        // check start
        /*  0 */    load_high,
        /*  1 */   BPF_JUMP(BPF_JMP | BPF_JEQ, start_addr_high, /* ih = sah */ 0, /* ih != sah */ 2),

        // ih = sah
        /*  2 */   load_low,
        /*  3 */   BPF_JUMP(BPF_JMP | BPF_JGE, start_addr_low, /* check end */ 1, /* trap */ 7),

        // ih != sah
        /*  4 */   BPF_JUMP(BPF_JMP | BPF_JGT, start_addr_high, /* check end */ 0, /* trap */ 6),

        // check end
        /*  5 */   load_high,
        /*  6 */   BPF_JUMP(BPF_JMP | BPF_JEQ, end_addr_high, /* ih = seh */ 0, /* ih != seh */ 2),

        // ih = seh
        /*  7 */   load_low,
        /*  8 */   BPF_JUMP(BPF_JMP | BPF_JGE, end_addr_low, /* trap */ 2, /* allow */ 1),

        // ih != seh
        /*  9 */   BPF_JUMP(BPF_JMP | BPF_JGT, end_addr_high, /* trap */ 1, /* allow */ 0),

        // allow
        /* 10 */   BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),

        // trap
        /* 11 */   BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_TRAP),

    };

    for (size_t i = 0; i < sizeof(filt); i ++) {
        ((char*)out)[i] = ((char*)filt)[i];
    }

    return sizeof(filt) / sizeof(struct sock_filter);
}
