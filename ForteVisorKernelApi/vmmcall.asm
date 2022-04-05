.code

svm_vmmcall proc frame
	
	.endprolog

	push rbx ; prevent the corruption of certain guest registers used during vm unload
	push rcx

	vmmcall
    
	pop rcx
	pop rbx
	
	ret
	
svm_vmmcall endp

end