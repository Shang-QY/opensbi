nc -z  127.0.0.1 54320 || /usr/bin/gnome-terminal -x ./soc_term.py 54320 &
nc -z  127.0.0.1 54321 || /usr/bin/gnome-terminal -x ./soc_term.py 54321 &
while ! nc -z 127.0.0.1 54320 || ! nc -z 127.0.0.1 54321; do sleep 1; done
./qemu-system-riscv64 -d guest_errors -D guest_log.txt   \
  -M virt,pflash0=pflash0,pflash1=pflash1,aia=aplic-imsic,acpi=off,hmat=on,rpmi=on   \
  -dtb ./qemu-virt-new.dtb  \
  -m 4G,slots=2,maxmem=8G -object memory-backend-ram,size=2G,id=m0 -object memory-backend-ram,size=2G,id=m1   \
  -numa node,nodeid=0,memdev=m0 -numa node,nodeid=1,memdev=m1 -smp 2,sockets=2,maxcpus=2     \
  -bios ./PenglaiZone/build/platform/generic/firmware/fw_payload.elf    \
  -blockdev node-name=pflash0,driver=file,read-only=on,filename=./RISCV_VIRT_CODE.fd     \
  -blockdev node-name=pflash1,driver=file,filename=./RISCV_VIRT_VARS.fd     \
  -serial tcp:localhost:54320 -serial tcp:localhost:54321     \
  -drive file=fat:rw:~/intel/src/fat,id=hd0 -device virtio-blk-device,drive=hd0     \
  -nographic -netdev user,id=usernet,hostfwd=tcp::9990-:22
