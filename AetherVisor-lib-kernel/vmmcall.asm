.code

svm_vmmcall proc frame
	
	.endprolog

	push rbx ; prevent the corruption of certain guest registers used during vm unload
	push rcx
	push r11
	push r12

	lea    r11, [rsp+50h]	; 6th param
	mov r11, qword ptr [r11]

	lea r12, [rsp+48h]	; 5th param
	mov r12, qword ptr [r12]

	vmmcall

	pop r12
    pop r11
	pop rcx
	pop rbx


	ret
	
svm_vmmcall endp

end