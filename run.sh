nasm boot.asm -o boot.bin
nasm stage2.asm -o stage2.bin

nasm -f elf32 to_longmode/to_longmode.asm -o to_longmodeasm.o
g++ -c -m32 -ffreestanding -fno-exceptions -fno-rtti -nostdlib -I./ -c to_longmode/to_longmodecpp.cpp -o to_longmodecpp.o
ld -m elf_i386 -T to_longmode/linker.ld -o to_longmode.elf to_longmodeasm.o to_longmodecpp.o
objcopy -O binary to_longmode.elf to_longmode.bin
dd if=to_longmode.bin of=to_longmode2.bin bs=2048 count=2048 conv=sync



nasm -f elf64 kernel/kernelasm.asm -o kernelasm.o
g++ -c -m64 -ffreestanding -fno-exceptions -fno-rtti -nostdlib -I./ -c kernel/kernelcpp.cpp -o kernelcpp.o
ld -T kernel/linker.ld -o kernel.elf kernelasm.o kernelcpp.o
objcopy -O binary kernel.elf kernel.bin
SIZE=$(stat -c%s kernel.bin)
COUNT=$(( (SIZE + 2047) / 2048 ))
dd if=kernel.bin of=kernel2.bin bs=2048 count=$COUNT conv=sync


cat boot.bin stage2.bin to_longmode2.bin kernel2.bin > os.bin
cat boot.bin stage2.bin to_longmode2.bin kernel2.bin > os2.bin
truncate -s 16M os.bin
rm -f kernelasm.o
rm -f kernelcpp.o
rm -f kernel.elf
rm -f kernel.bin
rm -f boot.bin
rm -f stage2.bin
rm -f to_longmode2.bin
rm -f kernel2.bin
rm -f to_longmodeasm.o
rm -f to_longmodecpp.o
rm -f to_longmode.elf
rm -f to_longmode.bin
qemu-system-x86_64 \
  -machine q35 \
  -m 512M \
  -drive file=os.bin,format=raw,if=none,id=usbdisk \
  -device ich9-usb-ehci1,id=ehci \
  -device usb-storage,drive=usbdisk,bootindex=1 
