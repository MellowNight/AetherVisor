.code

svm_vmmcall proc frame
	
	.endprolog

	vmmcall
    ret
	
svm_vmmcall endp

end