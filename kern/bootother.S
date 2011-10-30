#include <inc/mmu.h>
#include <inc/memlayout.h>


# Each non-boot CPU ("AP") is started up in response to a STARTUP
# IPI from the boot CPU.  Section B.4.2 of the Multi-Processor
# Specification says that the AP will start in real mode with CS:IP
# set to XY00:0000, where XY is an 8-bit value sent with the
# STARTUP. Thus this code must start at a 4096-byte boundary.
#
# Because this code sets DS to zero, it must sit
# at an address in the low 2^16 bytes.
#
# bootothers (in main.c) sends the STARTUPs one at a time.
# It copies this code (start) at 0x7000.
# It puts the address of a newly allocated per-core stack in start-4,
# and the address of the place to jump to (mpmain) in start-8.
#
# This code is identical to boot/boot.S except:
#   - it does not need to enable A20
#   - it uses the address at start-4 for the %esp
#   - it jumps to the address at start-8 instead of calling bootmain
#
# Currently, we know the excat load address (0x7000) so we
# hardcoded it instead of letting the linker fill in the gaps.
# TODO - the above must be changed as it is only a temporary
#        fast hack.

.set K_CSEG, 0x8	# kernel code segment selector
.set K_DSEG, 0x10	# kernel data segment selector


.code16           
.globl bootother_start
bootother_start:
	  cli            

	  xorw    %ax,%ax
	  movw    %ax,%ds
	  movw    %ax,%es
	  movw    %ax,%ss

	  lgdt    (gdtdesc - bootother_start + 0x7000)
	  movl    %cr0, %eax
	  orl     $CR0_PE, %eax
	  movl    %eax, %cr0

	  ljmp    $(K_CSEG), $(start32 - bootother_start + 0x7000)

.code32
start32:
	  movw    $(K_DSEG), %ax
	  movw    %ax, %ds
	  movw    %ax, %es
	  movw    %ax, %ss
	  movw    $0, %ax
	  movw    %ax, %fs
	  movw    %ax, %gs

	  # setting temporary pgdir to map lower 4MB
	  movl $(entry_pgdir - KERNBASE), %eax
	  movl %eax, %cr3
	  movl %cr0, %eax
	  orl $(CR0_PE|CR0_PG|CR0_WP), %eax
	  movl %eax, %cr0
	
	  # switch to the stack allocated by bootothers()
	  #movl    bootother_start-4, %esp
	  movl	   (KERNBASE + 0x7000-4), %esp

	  # call mpmain()
	  call	*(KERNBASE + 0x7000-8)

	  movw    $0x8a00, %ax
	  movw    %ax, %dx
	  outw    %ax, %dx
	  movw    $0x8ae0, %ax
	  outw    %ax, %dx
spin:
	  jmp     spin

.p2align 2
gdt:
	  SEG_NULL
	  SEG(STA_X|STA_R, 0, 0xffffffff)
	  SEG(STA_W, 0, 0xffffffff)

gdtdesc:
	  .word   (gdtdesc - gdt - 1)
	  .long   (gdt - bootother_start + 0x7000)

.globl bootother_end
bootother_end:
	  nop