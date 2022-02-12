
.const
	KTRAP_FRAME_SIZE            equ     190h
	MACHINE_FRAME_SIZE          equ     28h

.code

extern HandleVmexit : proc

PUSHAQ macro
        push    rax
        push    rcx
        push    rdx
        push    rbx
        push    -1      ; Dummy for rsp.
        push    rbp
        push    rsi
        push    rdi
        push    r8
        push    r9
        push    r10
        push    r11
        push    r12
        push    r13
        push    r14
        push    r15
        endm
POPAQ macro
        pop     r15
        pop     r14
        pop     r13
        pop     r12
        pop     r11
        pop     r10
        pop     r9
        pop     r8
        pop     rdi
        pop     rsi
        pop     rbp
        pop     rbx    ; Dummy for rsp (this value is destroyed by the next pop).
        pop     rbx
        pop     rdx
        pop     rcx
        pop     rax
        endm


LaunchVm proc frame

	mov	rsp, rcx	; point stack pointer to &VpData->GuestVmcbPa

EnterVm:
	mov	rax, [rsp]	; put physical address of guest VMCB in rax

	vmload rax		; vmload extra state

	; int 3
	
	vmrun rax		; virtualize

	vmsave rax		; vmexit, save extra state

	PUSHAQ			; save all guest general registers

	mov rcx, [rsp + 8 * 16 + 2 * 8]		; pass reference to Virtual Processor data in arg 1
	mov rdx, rsp					    ; pass guest registers in arg 2


    sub rsp, 80h
    movaps xmmword ptr [rsp + 20h], xmm0
    movaps xmmword ptr [rsp + 30h], xmm1
    movaps xmmword ptr [rsp + 40h], xmm2
    movaps xmmword ptr [rsp + 50h], xmm3
    movaps xmmword ptr [rsp + 60h], xmm4
    movaps xmmword ptr [rsp + 70h], xmm5
	.endprolog			; end of RSP subtraction

	call HandleVmexit	; vmexit handler

    movaps xmm5, xmmword ptr [rsp + 70h]
    movaps xmm4, xmmword ptr [rsp + 60h]
    movaps xmm3, xmmword ptr [rsp + 50h]
    movaps xmm2, xmmword ptr [rsp + 40h]
    movaps xmm1, xmmword ptr [rsp + 30h]
    movaps xmm0, xmmword ptr [rsp + 20h]
    add rsp, 80h

	test al, al	; if return 1, then end VM

	POPAQ		; restore general purpose registers, RSP pops back to GuestVmcbPa

	jz	EnterVm
	jmp	EndVm

EndVm:
	; in HandleVmExit, rcx is set to guest stack pointer, and rbx is set to guest RIP
	; but guest state is already ended so we continue execution as host

	mov rsp, rcx

	jmp rbx

LaunchVm endp


svm_vmmcall proc frame
	
	.endprolog

	vmmcall
    ret
	
svm_vmmcall endp



	end