.PHONY: all debug clean reset start

pwd = $(shell pwd)
edk2 = $(pwd)/edk2
output_path = ./build
local_ovmf_rom = $(output_path)/rom/OVMF.fd
local_ovmf_vars = $(output_path)/rom/OVMF_VARS.fd
local_ovmf_code = $(output_path)/rom/OVMF_CODE.fd
exec_build = $(root_path)/EFI/BOOT/BOOTX64.EFI
debug_file = $(root_path)/EFI/BOOT/UefiClient.debug
root_path = $(output_path)/root

export EDK_TOOLS_PATH=$(edk2)/BaseTools
export PACKAGES_PATH=$(edk2):$(edk2_libc)

all: prepare compile deploy
debug: prepare compile_debug deploy_debug
q: copy_src compile deploy
qd: copy_src compile_debug deploy_debug
clean_all: clean clean_output

prepare: setup_edk2 prep_output copy_src
	sed -i '518i \ \ UefiClient/UefiClient.inf' $(edk2)/MdeModulePkg/MdeModulePkg.dsc
	sed -i '107i \ \ !include RedfishPkg/RedfishLibs.dsc.inc' $(edk2)/MdeModulePkg/MdeModulePkg.dsc
	sed -i '23i !include RedfishPkg/RedfishDefines.dsc.inc' $(edk2)/MdeModulePkg/MdeModulePkg.dsc

copy_src:
	@[ -d "$(edk2)/UefiClient" ] || { mkdir $(edk2)/UefiClient; }
	cp -r src/*.* $(edk2)/UefiClient

setup_edk2:
	@[ -d "$(edk2)" ] || { git clone https://github.com/tianocore/edk2.git -b"edk2-stable202208"; cd $(edk2); git submodule update --init; . ./edksetup.sh; make -C BaseTools; cd ..;}

compile:
	sed -i '28s/DEBUG/RELEASE/g' $(edk2)/Conf/target.txt
	cd $(edk2); . ./edksetup.sh; cd ..; \
	build -a X64 -t GCC5 -p MdeModulePkg/MdeModulePkg.dsc;

compile_debug:
	sed -i '28s/RELEASE/DEBUG/g' $(edk2)/Conf/target.txt
	cd $(edk2); . ./edksetup.sh; cd ..; \
	build -a X64 -t GCC5 -p MdeModulePkg/MdeModulePkg.dsc;

clean:
	rm -rf $(edk2)/UefiClient $(edk2)/Build $(debug_file) $(exec_build)
	cd $(edk2) && git restore MdeModulePkg/MdeModulePkg.dsc && git restore OvmfPkg/OvmfPkgX64.dsc && git restore OvmfPkg/OvmfPkgX64.fdf && cd ..
	-sed -i '28s/RELEASE/DEBUG/g' $(edk2)/Conf/target.txt
	rm -f debug.log

deploy_debug: make_rom
	-cp $(edk2)/Build/MdeModule/DEBUG_GCC5/X64/UefiClient.efi $(exec_build)
	-cp $(edk2)/Build/MdeModule/DEBUG_GCC5/X64/UefiClient.debug $(debug_file)
	-cp $(edk2)/Build/MdeModule/DEBUG_GCC5/X64/UefiClient.efi $(root_path)/EFI/BOOT/
	-cp -r $(edk2)/Build/MdeModule/DEBUG_GCC5/X64/UefiClient $(root_path)/EFI/BOOT/

deploy: make_rom
	-cp $(edk2)/Build/MdeModule/RELEASE_GCC5/X64/UefiClient/UefiClient/OUTPUT/UefiClient.efi $(exec_build)

start:
	qemu-system-x86_64 -drive if=pflash,format=raw,file=$(local_ovmf_rom) -drive format=raw,file=fat:rw:$(root_path)

start_debug:
	qemu-system-x86_64 -s  -S -drive if=pflash,format=raw,file=$(local_ovmf_rom) -drive format=raw,file=fat:rw:$(root_path) -debugcon file:debug.log -global isa-debugcon.iobase=0x402

reset: clean_all
	rm -rf $(edk2)

clean_output:
	rm -rf $(output_path)

make_rom:
ifeq ("$(wildcard $(local_ovmf_rom))","")
	sed -i '43s/FALSE/TRUE/g' $(edk2)/OvmfPkg/OvmfPkgX64.dsc
#	sed -i '968i \ \ NetworkPkg/HttpDxe/HttpDxe.inf' $(edk2)/OvmfPkg/OvmfPkgX64.dsc
#	sed -i '969i \ \ NetworkPkg/HttpBootDxe/HttpBootDxe.inf' $(edk2)/OvmfPkg/OvmfPkgX64.dsc
#	sed -i '970i \ \ NetworkPkg/HttpUtilitiesDxe/HttpUtilitiesDxe.inf' $(edk2)/OvmfPkg/OvmfPkgX64.dsc
#	sed -i '971i \ \ NetworkPkg/DnsDxe/DnsDxe.inf' $(edk2)/OvmfPkg/OvmfPkgX64.dsc
#	sed -i '199i \ \ HttpLib|NetworkPkg/Library/DxeHttpLib/DxeHttpLib.inf' $(edk2)/OvmfPkg/OvmfPkgX64.dsc
#	sed -i '407i INF NetworkPkg/HttpDxe/HttpDxe.inf' $(edk2)/OvmfPkg/OvmfPkgX64.fdf
#	sed -i '408i INF NetworkPkg/HttpBootDxe/HttpBootDxe.inf' $(edk2)/OvmfPkg/OvmfPkgX64.fdf
#	sed -i '409i INF NetworkPkg/HttpUtilitiesDxe/HttpUtilitiesDxe.inf' $(edk2)/OvmfPkg/OvmfPkgX64.fdf
#	sed -i '410i INF NetworkPkg/DnsDxe/DnsDxe.inf' $(edk2)/OvmfPkg/OvmfPkgX64.fdf
	sed -i '28s/DEBUG/RELEASE/g' $(edk2)/Conf/target.txt
	cd $(edk2); . ./edksetup.sh; cd ..; \
	build -a X64 -t GCC5 -p OvmfPkg/OvmfPkgX64.dsc;
	cp $(edk2)/Build/OvmfX64/RELEASE_GCC5/FV/OVMF.fd $(local_ovmf_rom)
	cp $(edk2)/Build/OvmfX64/RELEASE_GCC5/FV/OVMF_VARS.fd $(local_ovmf_vars)
	cp $(edk2)/Build/OvmfX64/RELEASE_GCC5/FV/OVMF_CODE.fd $(local_ovmf_code)
endif

clean_rom:
	rm -f $(local_ovmf_rom)

prep_output:
	@[ -d "$(output_path)" ] || { mkdir $(output_path); mkdir $(output_path)/rom; mkdir $(output_path)/root; mkdir $(output_path)/root/EFI; mkdir $(output_path)/root/EFI/BOOT; }