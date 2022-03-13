.code


svm_vmmcall proc frame
	
	.endprolog

	vmmcall
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