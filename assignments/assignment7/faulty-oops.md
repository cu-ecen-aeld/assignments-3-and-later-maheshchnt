##Analysis of OOPS

####Accessing the faulty module by writing hello world to it reported OOPs and caused system to reboot.
####From the code it looks obvious that the oops is because of NULL pointer access in the write function of the driver.
####From the below oops log which was resulted because of NULL, it can be seen that the PC (program counter) was at 0x14 offset from the start of faulty_write function which is of size 0x20.
####The log also some infromation such as cpu register dump and the function trace that resulted the OOPS.
####Using objdump, the instruction that caused the OOPS can be clearly seen--copied objdump below of reference. As shown in the objdump,  instruction (14:   b900003f        str     wzr, [x1]) while accessing the memory location stored in X1 register has resulted the crash. As PC pointed, the address offset of this instruction is 0x14 from "faulty_write" function address.  

---> echo “hello_world” > /dev/faulty
Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
Mem abort info:
  ESR = 0x96000045
  EC = 0x25: DABT (current EL), IL = 32 bits
  SET = 0, FnV = 0
  EA = 0, S1PTW = 0
  FSC = 0x05: level 1 translation fault
Data abort info:
  ISV = 0, ISS = 0x00000045
  CM = 0, WnR = 1
user pgtable: 4k pages, 39-bit VAs, pgdp=000000004204f000
[0000000000000000] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000
Internal error: Oops: 96000045 [#1] SMP
Modules linked in: hello(O) faulty(O) scull(O)
CPU: 0 PID: 128 Comm: sh Tainted: G           O      5.15.18 #1
Hardware name: linux,dummy-virt (DT)
pstate: 80000005 (Nzcv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--)
pc : faulty_write+0x14/0x20 [faulty]
lr : vfs_write+0xa8/0x2b0
sp : ffffffc008d13d80
x29: ffffffc008d13d80 x28: ffffff80020d4c80 x27: 0000000000000000
x26: 0000000000000000 x25: 0000000000000000 x24: 0000000000000000
x23: 0000000040001000 x22: 0000000000000012 x21: 00000055833c2a70
x20: 00000055833c2a70 x19: ffffff8002077800 x18: 0000000000000000
x17: 0000000000000000 x16: 0000000000000000 x15: 0000000000000000
x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000
x11: 0000000000000000 x10: 0000000000000000 x9 : 0000000000000000
x8 : 0000000000000000 x7 : 0000000000000000 x6 : 0000000000000000
x5 : 0000000000000001 x4 : ffffffc0006f7000 x3 : ffffffc008d13df0
x2 : 0000000000000012 x1 : 0000000000000000 x0 : 0000000000000000
Call trace:
 faulty_write+0x14/0x20 [faulty]
 ksys_write+0x68/0x100
 __arm64_sys_write+0x20/0x30
 invoke_syscall+0x54/0x130
 el0_svc_common.constprop.0+0x44/0xf0
 do_el0_svc+0x40/0xa0
 el0_svc+0x20/0x60
 el0t_64_sync_handler+0xe8/0xf0
 el0t_64_sync+0x1a0/0x1a4
Code: d2800001 d2800000 d503233f d50323bf (b900003f) 
---[ end trace 6881d821683aa4a9 ]---

Welcome to Buildroot
buildroot login: 

=================================================================================

mgudipati@mahesh:~/COURSE/AESD/ASSIGNMENT_7/ASSIGNMENT_5/assignment-5-maheshchnt/buildroot/output/build/ldd-2a16a2d63e1ef6a0bc775e057304dd679df3bff0/misc-modules$ /home/mgudipati/COURSE/AESD/ARM_TOOL_CHAIN/gcc-arm-10.2-2020.11-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-objdump -S faulty.ko 

faulty.ko:     file format elf64-littleaarch64


Disassembly of section .text:

0000000000000000 <faulty_write>:
   0:	d503245f 	bti	c
   4:	d2800001 	mov	x1, #0x0                   	// #0
   8:	d2800000 	mov	x0, #0x0                   	// #0
   c:	d503233f 	paciasp
  10:	d50323bf 	autiasp
  14:	b900003f 	str	wzr, [x1]
  18:	d65f03c0 	ret
  1c:	d503201f 	nop

0000000000000020 <faulty_read>:
  20:	d503233f 	paciasp
  24:	a9bd7bfd 	stp	x29, x30, [sp, #-48]!
  28:	d5384100 	mrs	x0, sp_el0
  2c:	910003fd 	mov	x29, sp
  30:	f9000bf3 	str	x19, [sp, #16]
  34:	92800005 	mov	x5, #0xffffffffffffffff    	// #-1
  38:	12800003 	mov	w3, #0xffffffff            	// #-1
  3c:	f100105f 	cmp	x2, #0x4
  40:	f941f804 	ldr	x4, [x0, #1008]
  44:	f90017e4 	str	x4, [sp, #40]

