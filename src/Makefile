.PHONY: build
build:
	$(MAKE) -C urvirt-stub
	$(MAKE) -C urvirt-loader
	$(MAKE) -C test-kernel

.PHONY: run
run: build
	qemu-riscv64 $(QEMUOPTS) urvirt-loader/urvirt-loader urvirt-stub/urvirt-stub.bin test-kernel/kernel.bin

