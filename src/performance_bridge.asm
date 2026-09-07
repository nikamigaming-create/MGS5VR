; Only the observed critical workers yield without a one-millisecond sleep.
; The displaced LEA and subsequent native sleep call execute in the trampoline.
EXTERN MgsPerformanceTrampoline:QWORD
PUBLIC MgsPerformanceIntercept
.code
MgsPerformanceIntercept PROC FRAME
    pushfq
    .allocstack 8
    .endprolog
    cmp dword ptr [rsi+8],4
    jg keepDelay
    xor edx,edx
keepDelay:
    popfq
    jmp qword ptr [MgsPerformanceTrampoline]
MgsPerformanceIntercept ENDP
END
