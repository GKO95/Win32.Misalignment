.CODE

CR0AMEnable PROC
	mov rax, cr0
	or  rax, 40000h
	mov cr0, rax
	ret
CR0AMEnable ENDP

CR0AMDisable PROC
	mov rax, cr0
	and rax, -262145
	mov cr0, rax
	ret
CR0AMDisable ENDP

END
