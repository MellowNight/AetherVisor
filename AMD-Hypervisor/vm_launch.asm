include  instructions.asm

.const
	KTRAP_FRAME_SIZE            equ     190h
	MACHINE_FRAME_SIZE          equ     28h

.code
extern svm_vmmcall : proc
extern HandleVmexit : proc

LaunchVm proc frame

	mov	rsp, rcx	; point stack pointer to &VpData->guest_vmcbPa

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

	POPAQ	; restore general purpose registers, RSP pops back to guest_vmcbPa

	jz	EnterVm
	jmp	EndVm

EndVm:
	; in HandleVmExit, rcx is set to guest stack pointer, and rbx is set to guest RIP
	; but guest state is already ended so we continue execution as host

	mov rsp, rcx

	jmp rbx

LaunchVm endp

	end