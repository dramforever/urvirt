CFLAGS += -nostdlib --static-pie -fno-plt -fpie -O -I ../common

.PHONY: all
all: rvirt-stub.bin rvirt-stub.sym.txt

rvirt-stub.elf: link.ld rvirt-stub.o
	$(LD) --no-dynamic-linker -pie -o $@ -T $^

%.bin: %.elf
	$(OBJCOPY) -j output -O binary $< $@

%.sym.txt: %.elf
	riscv64-unknown-linux-gnu-objdump -t $^ > $@

.PHONY: clean
clean:
	rm -f *.o *.bin *.elf *.sym.txt
