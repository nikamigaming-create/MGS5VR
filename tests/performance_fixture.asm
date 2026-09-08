PUBLIC MgsTestWorkerDelay
.code
; uint64_t (void* site, const void* worker, uint64_t flags[2])
MgsTestWorkerDelay PROC FRAME
    push rsi
    .pushreg rsi
    sub rsp,30h
    .allocstack 30h
    .endprolog
    mov rsi,rdx
    mov [rsp+20h],r8
    mov rax,rcx
    mov edx,1
    xor ecx,ecx
    stc
    pushfq
    pop rcx
    mov [r8],rcx
    call rax
    pushfq
    pop rcx
    mov r8,[rsp+20h]
    mov [r8+8],rcx
    add rsp,30h
    pop rsi
    ret
MgsTestWorkerDelay ENDP
END
