	.arch armv8-a+crc
	.file	"asm-offsets.c"
// GNU C11 (Ubuntu 13.3.0-6ubuntu2~24.04) version 13.3.0 (aarch64-linux-gnu)
//	compiled by GNU C version 13.3.0, GMP version 6.3.0, MPFR version 4.2.1, MPC version 1.3.1, isl version isl-0.26-GMP

// GGC heuristics: --param ggc-min-expand=100 --param ggc-min-heapsize=131072
// options passed: -mstrict-align -mgeneral-regs-only -mbranch-protection=none -march=armv8-a+crc -mlittle-endian -mabi=lp64 -gdwarf-4 -Os -std=gnu11 -fno-builtin -ffreestanding -fshort-wchar -fno-strict-aliasing -fno-stack-protector -fno-delete-null-pointer-checks -fstack-usage -fno-pic -ffunction-sections -fdata-sections -fno-common -ffixed-x18 -fasynchronous-unwind-tables -fstack-clash-protection
	.text
.Ltext0:
	.file 1 "lib/asm-offsets.c"
	.section	.text.startup.main,"ax",@progbits
	.align	2
	.global	main
	.type	main, %function
main:
.LFB140:
	.loc 1 21 1 view -0
	.cfi_startproc
	.loc 1 23 2 view .LVU1
#APP
// 23 "lib/asm-offsets.c" 1
	
.ascii "->GENERATED_GBL_DATA_SIZE 480 (sizeof(struct global_data) + 15) & ~15"	//
// 0 "" 2
	.loc 1 26 2 view .LVU2
// 26 "lib/asm-offsets.c" 1
	
.ascii "->GENERATED_BD_INFO_SIZE 192 (sizeof(struct bd_info) + 15) & ~15"	//
// 0 "" 2
	.loc 1 29 2 view .LVU3
// 29 "lib/asm-offsets.c" 1
	
.ascii "->GD_SIZE 472 sizeof(struct global_data)"	//
// 0 "" 2
	.loc 1 31 2 view .LVU4
// 31 "lib/asm-offsets.c" 1
	
.ascii "->GD_BD 0 offsetof(struct global_data, bd)"	//
// 0 "" 2
	.loc 1 33 2 view .LVU5
// 33 "lib/asm-offsets.c" 1
	
.ascii "->GD_FLAGS 8 offsetof(struct global_data, flags)"	//
// 0 "" 2
	.loc 1 36 2 view .LVU6
// 36 "lib/asm-offsets.c" 1
	
.ascii "->GD_MALLOC_BASE 296 offsetof(struct global_data, malloc_base)"	//
// 0 "" 2
	.loc 1 39 2 view .LVU7
// 39 "lib/asm-offsets.c" 1
	
.ascii "->GD_RELOCADDR 120 offsetof(struct global_data, relocaddr)"	//
// 0 "" 2
	.loc 1 41 2 view .LVU8
// 41 "lib/asm-offsets.c" 1
	
.ascii "->GD_RELOC_OFF 160 offsetof(struct global_data, reloc_off)"	//
// 0 "" 2
	.loc 1 43 2 view .LVU9
// 43 "lib/asm-offsets.c" 1
	
.ascii "->GD_START_ADDR_SP 152 offsetof(struct global_data, start_addr_sp)"	//
// 0 "" 2
	.loc 1 45 2 view .LVU10
// 45 "lib/asm-offsets.c" 1
	
.ascii "->GD_NEW_GD 168 offsetof(struct global_data, new_gd)"	//
// 0 "" 2
	.loc 1 47 2 view .LVU11
// 47 "lib/asm-offsets.c" 1
	
.ascii "->GD_ENV_ADDR 72 offsetof(struct global_data, env_addr)"	//
// 0 "" 2
	.loc 1 49 2 view .LVU12
// lib/asm-offsets.c:50: }
	.loc 1 50 1 is_stmt 0 view .LVU13
#NO_APP
	mov	w0, 0	//,
	ret	
	.cfi_endproc
.LFE140:
	.size	main, .-main
	.text
.Letext0:
	.file 2 "include/errno.h"
	.section	.debug_info,"",@progbits
.Ldebug_info0:
	.4byte	0xc6
	.2byte	0x4
	.4byte	.Ldebug_abbrev0
	.byte	0x8
	.uleb128 0x1
	.4byte	.LASF12
	.byte	0xc
	.4byte	.LASF13
	.4byte	.LASF14
	.4byte	.Ldebug_ranges0+0
	.8byte	0
	.4byte	.Ldebug_line0
	.uleb128 0x2
	.byte	0x8
	.byte	0x7
	.4byte	.LASF0
	.uleb128 0x3
	.byte	0x4
	.byte	0x5
	.string	"int"
	.uleb128 0x2
	.byte	0x1
	.byte	0x8
	.4byte	.LASF1
	.uleb128 0x4
	.4byte	0x37
	.uleb128 0x2
	.byte	0x4
	.byte	0x7
	.4byte	.LASF2
	.uleb128 0x2
	.byte	0x2
	.byte	0x7
	.4byte	.LASF3
	.uleb128 0x2
	.byte	0x8
	.byte	0x5
	.4byte	.LASF4
	.uleb128 0x2
	.byte	0x8
	.byte	0x5
	.4byte	.LASF5
	.uleb128 0x2
	.byte	0x1
	.byte	0x6
	.4byte	.LASF6
	.uleb128 0x2
	.byte	0x1
	.byte	0x8
	.4byte	.LASF7
	.uleb128 0x2
	.byte	0x2
	.byte	0x5
	.4byte	.LASF8
	.uleb128 0x2
	.byte	0x8
	.byte	0x7
	.4byte	.LASF9
	.uleb128 0x5
	.4byte	0x3e
	.4byte	0x8b
	.uleb128 0x6
	.4byte	0x29
	.byte	0
	.byte	0
	.uleb128 0x4
	.4byte	0x7b
	.uleb128 0x7
	.4byte	.LASF15
	.byte	0x2
	.byte	0x1f
	.byte	0x13
	.4byte	0x8b
	.string	""
	.uleb128 0x2
	.byte	0x10
	.byte	0x4
	.4byte	.LASF10
	.uleb128 0x2
	.byte	0x1
	.byte	0x2
	.4byte	.LASF11
	.uleb128 0x8
	.4byte	.LASF16
	.byte	0x1
	.byte	0x14
	.byte	0x5
	.4byte	0x30
	.8byte	.LFB140
	.8byte	.LFE140-.LFB140
	.uleb128 0x1
	.byte	0x9c
	.byte	0
	.section	.debug_abbrev,"",@progbits
.Ldebug_abbrev0:
	.uleb128 0x1
	.uleb128 0x11
	.byte	0x1
	.uleb128 0x25
	.uleb128 0xe
	.uleb128 0x13
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x1b
	.uleb128 0xe
	.uleb128 0x55
	.uleb128 0x17
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x10
	.uleb128 0x17
	.byte	0
	.byte	0
	.uleb128 0x2
	.uleb128 0x24
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3e
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0xe
	.byte	0
	.byte	0
	.uleb128 0x3
	.uleb128 0x24
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3e
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0x8
	.byte	0
	.byte	0
	.uleb128 0x4
	.uleb128 0x26
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x5
	.uleb128 0x1
	.byte	0x1
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x6
	.uleb128 0x21
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2f
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0x7
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x1c
	.uleb128 0x8
	.byte	0
	.byte	0
	.uleb128 0x8
	.uleb128 0x2e
	.byte	0
	.uleb128 0x3f
	.uleb128 0x19
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x39
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0x19
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x7
	.uleb128 0x40
	.uleb128 0x18
	.uleb128 0x2117
	.uleb128 0x19
	.byte	0
	.byte	0
	.byte	0
	.section	.debug_aranges,"",@progbits
	.4byte	0x2c
	.2byte	0x2
	.4byte	.Ldebug_info0
	.byte	0x8
	.byte	0
	.2byte	0
	.2byte	0
	.8byte	.LFB140
	.8byte	.LFE140-.LFB140
	.8byte	0
	.8byte	0
	.section	.debug_ranges,"",@progbits
.Ldebug_ranges0:
	.8byte	.LFB140
	.8byte	.LFE140
	.8byte	0
	.8byte	0
	.section	.debug_line,"",@progbits
.Ldebug_line0:
	.section	.debug_str,"MS",@progbits,1
.LASF2:
	.string	"unsigned int"
.LASF12:
	.ascii	"GNU C11 13.3.0 -mstrict-align -mgeneral-regs-only -mbranch-p"
	.ascii	"rotection=none -march=armv8-a+crc -mlittle-endian -mabi=lp64"
	.ascii	" -gdwarf-4 -Os -std=gn"
	.string	"u11 -fno-builtin -ffreestanding -fshort-wchar -fno-strict-aliasing -fno-stack-protector -fno-delete-null-pointer-checks -fstack-usage -fno-pic -ffunction-sections -fdata-sections -fno-common -ffixed-x18 -fasynchronous-unwind-tables -fstack-clash-protection"
.LASF16:
	.string	"main"
.LASF0:
	.string	"long unsigned int"
.LASF9:
	.string	"long long unsigned int"
.LASF7:
	.string	"unsigned char"
.LASF13:
	.string	"lib/asm-offsets.c"
.LASF1:
	.string	"char"
.LASF14:
	.string	"/home/ngxxfus/Desktop/mod_uboot"
.LASF15:
	.string	"error_message"
.LASF11:
	.string	"_Bool"
.LASF5:
	.string	"long long int"
.LASF3:
	.string	"short unsigned int"
.LASF6:
	.string	"signed char"
.LASF4:
	.string	"long int"
.LASF10:
	.string	"long double"
.LASF8:
	.string	"short int"
	.ident	"GCC: (Ubuntu 13.3.0-6ubuntu2~24.04) 13.3.0"
	.section	.note.GNU-stack,"",@progbits
