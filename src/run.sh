#!/bin/bash

set -xue

# qemu file path

QEMU=qemu-system-riscv32


# path per clang e flag per il compilatore
CC=clang
CFLAGS="-std=c11 -O2 -g3 -Wall -Wextra --target=riscv32-unknown-elf -fuse-ld=lld -fno-stack-protector -ffreestanding -nostdlib"


OBJCOPY=/usr/bin/llvm-objcopy

# costruiamo la applicazione shell
$CC $CFLAGS -Wl,-Tuser.ld -Wl,-Map=shell.map -o shell.elf application/shell.c user.c common.c
$OBJCOPY --set-section-flags .bss=alloc,contents -O binary shell.elf shell.bin
$OBJCOPY -Ibinary -Oelf32-littleriscv shell.bin shell.bin.o


# costruiamo il kernel (build)
$CC $CFLAGS -Wl,-Tkernel.ld -Wl,-Map=kernel.map -o kernel.elf \
	kernel.c common.c shell.bin.o


# start qemu

$QEMU -machine virt -bios default -nographic -serial mon:stdio --no-reboot \
	-d unimp,gues_errors,int,cpu_reset -D qemu.log \
	-drive id=drive0,file=lorem.txt,format=raw,if=none \
	-device virtio-blk-device,drive=drive0,bus=virtio-mmio-bus.0 \
	-kernel kernel.elf

