CFLAGS += -MMD -nostdlib -fno-builtin --static-pie -fno-plt -fpie -O -I ../common

SOURCES = $(wildcard *.c)
OBJECTS = $(SOURCES:%.c=%.o)
DEPENDS = $(SOURCES:%.c=%.d)

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
