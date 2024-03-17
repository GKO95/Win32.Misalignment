.CODE

EFLAGACEnable PROC
	pushfq
	bts qword ptr[rsp], 12h ; set AC bit of eflags
	popfq
	ret
EFLAGACEnable ENDP

EFLAGACDisable PROC
	pushfq
	btr qword ptr[rsp], 12h ; reset AC bit of eflags
	popfq
	ret
EFLAGACDisable ENDP

END
