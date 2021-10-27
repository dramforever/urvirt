CFLAGS = -ffreestanding -mcmodel=medany

kernel.bin: kernel.elf
	$(OBJCOPY) --strip-all -O binary $< $@

kernel.elf: linker.ld test-kernel.o entry.o
	$(LD) -o $@ -T $^

test-kernel.o: test-kernel.c

.PHONY: clean
clean:
	rm -f *.bin *.elf *.o
