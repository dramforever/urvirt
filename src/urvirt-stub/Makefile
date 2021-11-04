CFLAGS += -MMD -nostdlib --static-pie -fno-plt -fpie -O -I ../common

OBJECTS = urvirt-stub.o seccomp-bpf.o handle-sbi.o riscv-priv.o
DEPENDS = $(OBJECTS:%.o=%.d)

.PHONY: all
all: urvirt-stub.bin urvirt-stub.sym.txt

urvirt-stub.elf: link.ld $(OBJECTS)
	$(LD) --no-dynamic-linker -pie -o $@ -T $^

%.bin: %.elf
	$(OBJCOPY) -j output -O binary $< $@

%.sym.txt: %.elf
	$(OBJDUMP) -t $^ > $@

.PHONY: clean
clean:
	rm -f *.o *.bin *.elf *.sym.txt *.d

-include $(DEPENDS)
