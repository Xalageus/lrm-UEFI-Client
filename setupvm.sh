#!/bin/sh
# Cleanup existing files
rm -rf /opt/lrm_uefi_client
mkdir /opt/lrm_uefi_client

# Create iso
dd if=/dev/zero of=build/root/efiboot.img bs=512 count=4096
mkfs.msdos -F 12 build/root/efiboot.img
mmd -i build/root/efiboot.img ::EFI
mmd -i build/root/efiboot.img ::EFI/BOOT
mcopy -i build/root/efiboot.img build/root/EFI/BOOT/BOOTX64.EFI ::EFI/BOOT/BOOTX64.EFI
xorriso -as mkisofs -V 'LRM_UEFI_CLIENT' -e efiboot.img -no-emul-boot -o /opt/lrm_uefi_client/lrm_uefi_client.iso build/root/

rm -f build/root/efiboot.img

# Create hdd
qemu-img create -f qcow2 /opt/lrm_uefi_client/hdd.qcow2 512M
modprobe nbd max_part=8
qemu-nbd --connect=/dev/nbd0 /opt/lrm_uefi_client/hdd.qcow2
sfdisk /dev/nbd0 < hdd.layout
mkfs.fat -F 32 /dev/nbd0p1
qemu-nbd --disconnect /dev/nbd0
sleep 1
rmmod nbd

# Copy firmware
cp build/rom/OVMF_CODE.fd /opt/lrm_uefi_client/OVMF_CODE.fd
cp build/rom/OVMF_VARS.fd /opt/lrm_uefi_client/OVMF_VARS.fd

chmod -R a+rw /opt/lrm_uefi_client

# Create vm
virt-install --name lrm_uefi_client --memory 512 --vcpus 1 --disk /opt/lrm_uefi_client/hdd.qcow2,bus=sata --import --cdrom /opt/lrm_uefi_client/lrm_uefi_client.iso --os-variant generic --network default --boot loader=/opt/lrm_uefi_client/OVMF_CODE.fd,loader.type=pflash,loader_ro=yes,nvram=/opt/lrm_uefi_client/OVMF_VARS.fd --boot cdrom --print-xml 1 > build/vm.xml
virsh define build/vm.xml
rm -f build/vm.xml
