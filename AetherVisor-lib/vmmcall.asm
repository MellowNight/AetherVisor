.code
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

svm_vmmcall proc frame
	
	.endprolog

	PUSHAQ                  ; prevent the corruption of certain guest registers used during vm unload

	lea r11, [rsp+28h+88h]	; 6th param
	mov r11, qword ptr [r11]

	lea r12, [rsp+20h+88h]	; 5th param
	mov r12, qword ptr [r12]

	vmmcall

    POPAQ

	ret
	
svm_vmmcall endp

end