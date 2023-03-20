extern sandbox_execute_event : qword
extern sandbox_mem_access_event : qword
extern BranchCallbackInternal : proc
extern branch_trace_finish_event : qword
extern syscall_hook : qword

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

execute_handler_wrapper proc frame
	; return address       17 * 8 + 16
    ; original guest RIP   17 * 8 + 8
    ; last branch location 17 * 8 + 0 
    
    pushfq
    PUSHAQ

    .endprolog

    mov rcx, rsp                    ; pass the registers
    mov rdx, [rsp + 8 * 17 + 16]    ; pass the original return address on stack
    mov r8, [rsp + 8 * 17 + 8]      ; pass the original guest RIP
    mov r9, [rsp + 8 * 17]          ; pass the last branch location
        
    ; Align the stack pointer to 16 bytes
    push rbp
    mov rbp, rsp
    and rsp, 0FFFFFFFFFFFFFFF0h
    
    sub rsp, 20h

    call sandbox_execute_event
    
    add rsp, 20h

    mov rsp, rbp ; Add back the value that was subtracted
    pop rbp

    POPAQ
    popfq

    pop rax

    ret
	
execute_handler_wrapper endp

rw_handler_wrapper proc frame
	
    PUSHAQ
   
    .endprolog

    mov rcx, rsp                ; pass the registers
    mov rdx, [rsp + 8 * 16]     ; pass the guest RIP
    
    call sandbox_mem_access_event

    POPAQ

    ret
	
rw_handler_wrapper endp

branch_callback_wrapper proc frame
    .endprolog

    pushfq
    PUSHAQ
    
    mov rcx, rsp                    ; pass the registers
    mov rdx, [rsp + 8 * 17 + 8]     ; pass the return address
    mov r8, [rsp + 8 * 17]          ; pass the guest RIP

    ; Align the stack pointer to 16 bytes
    push rbp
    mov rbp, rsp
    and rsp, 0FFFFFFFFFFFFFFF0h
    
    sub rsp, 20h
    
    call BranchCallbackInternal

    add rsp, 20h

    mov rsp, rbp ; Add back the value that was subtracted
    pop rbp

    POPAQ
    popfq

    ret
	
branch_callback_wrapper endp

branch_trace_finish_event_wrap proc frame
	
    .endprolog

    pushfq
    PUSHAQ
    
    ; Align the stack pointer to 16 bytes
    push rbp
    mov rbp, rsp
    and rsp, 0FFFFFFFFFFFFFFF0h
    
    sub rsp, 20h

    call branch_trace_finish_event
    
    add rsp, 20h

    mov rsp, rbp ; Add back the value that was subtracted
    pop rbp

    POPAQ
    popfq

    ret
	
branch_trace_finish_event_wrap endp

syscall_hook_wrap proc frame
	
    .endprolog

    PUSHAQ
    
    mov rcx, rsp                    ; pass the registers
    mov rdx, [rsp + 8 * 16 + 8]     ; pass the return address
    mov r8, [rsp + 8 * 16]          ; pass the original guest RIP
    call syscall_hook

    POPAQ

    ret
	
syscall_hook_wrap endp

end