dd if=./binfile/mbr.bin of=./hd60M.img bs=512 count=1 conv=notrunc
dd if=./binfile/loader.bin of=./hd60M.img bs=512 count=3 seek=2 conv=notrunc
dd if=./binfile/kernel.bin of=./hd60M.img bs=512 count=200 seek=9 conv=notrunc