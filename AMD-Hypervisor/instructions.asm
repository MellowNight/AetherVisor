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

__rdpkru proc frame
	
	.endprolog

	rdpkru
    ret
	
__rdpkru endp

__readtr proc frame

	.endprolog

	str     ax
	ret
__readtr endp

__wrpkru proc frame
	
	.endprolog

	wrpkru
    ret
	
__wrpkru endp

end