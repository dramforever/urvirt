.PHONY: run
run:
	$(MAKE) -C urvirt-stub
	$(MAKE) -C urvirt-loader
	qemu-riscv64 $(QEMUOPTS) urvirt-loader/urvirt-loader urvirt-stub/urvirt-stub.bin

