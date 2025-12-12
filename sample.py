import xsdb
import time

# required files
platform_file = "DP168_bd_wrapper_2.xsa"
bitstream_file = "DP168_bd_wrapper.bit"
elf_file = "MPapp_component2.elf"

# start session
session = xsdb.start_debug_session()

# connect to hw_server (can be run as: C:\Xilinx\Vitis\2024.1\bin\unwrapped\win64.o\.\hw_server.exe)
session.connect(url="tcp:127.0.0.1:3121")

# set JTAG target
targets = session.targets()
print(targets)

jtag_target = session.targets("--set", filter="name =~ xc7a35t")

# # download bitstream
# jtag_target.fpga(file="top_running_lights.bit")

# open hardware design
jtag_target.loadhw("--regs", hw=platform_file)
jtag_target.fpga(file=bitstream_file)
time.sleep(3)

# set target
hart_target = session.targets("--set", filter="name =~ *Hart*")

# download ELF to target
hart_target.dow(elf_file)
time.sleep(3)
hart_target.rst()

# disconnect to hw_server
#hart_target.stop()
#jtag_target.stop()
session.disconnect()
