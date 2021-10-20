CFLAGS += -nostdlib --static-pie -fno-plt -fpie -O -I ../common

.PHONY: all
all: urvirt-stub.bin urvirt-stub.sym.txt

urvirt-stub.elf: link.ld urvirt-stub.o
	$(LD) --no-dynamic-linker -pie -o $@ -T $^

%.bin: %.elf
	$(OBJCOPY) -j output -O binary $< $@

%.sym.txt: %.elf
	riscv64-unknown-linux-gnu-objdump -t $^ > $@

.PHONY: clean
clean:
	rm -f *.o *.bin *.elf *.sym.txt
