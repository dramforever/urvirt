CFLAGS = -MMD -ffreestanding -mcmodel=medany -O

OBJECTS = test-kernel.o entry.o
DEPENDS = $(OBJECTS:%.o=%.d)

kernel.bin: kernel.elf
	$(OBJCOPY) --strip-all -O binary $< $@

kernel.elf: linker.ld $(OBJECTS)
	$(LD) -o $@ -T $^

.PHONY: clean
clean:
	rm -f *.bin *.elf *.o *.d

-include $(DEPENDS)
