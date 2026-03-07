	.arch armv8-a
	.file	"test_arm_cstm.c"
	.text
	.section	.rodata
	.align	3
.LC0:
	.string	"Result: 0x%X\n"
	.text
	.align	2
	.global	main
	.type	main, %function
main:
.LFB0:
	.cfi_startproc
	stp	x29, x30, [sp, -48]!
	.cfi_def_cfa_offset 48
	.cfi_offset 29, -48
	.cfi_offset 30, -40
	mov	x29, sp
	str	w0, [sp, 28]
	str	x1, [sp, 16]
	mov	w0, 48879
	movk	w0, 0xdead, lsl 16
	str	w0, [sp, 44]
	ldr	w0, [sp, 44]
#APP
// 9 "test_arm_cstm.c" 1
	.inst 0xE7FFFC01  // CSTM  x0, x0  (rn=0, rd=0)

// 0 "" 2
#NO_APP
	str	w0, [sp, 44]
	ldr	w1, [sp, 44]
	adrp	x0, .LC0
	add	x0, x0, :lo12:.LC0
	bl	printf
	mov	w0, 0
	ldp	x29, x30, [sp], 48
	.cfi_restore 30
	.cfi_restore 29
	.cfi_def_cfa_offset 0
	ret
	.cfi_endproc
.LFE0:
	.size	main, .-main
	.ident	"GCC: (Ubuntu 11.4.0-1ubuntu1~22.04) 11.4.0"
	.section	.note.GNU-stack,"",@progbits
