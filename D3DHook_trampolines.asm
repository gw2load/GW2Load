EXTERN HkSwapChainPresent : PROC
EXTERN HkSwapChain1Present1 : PROC
EXTERN HkSwapChainResizeBuffers : PROC
EXTERN HkSwapChain3ResizeBuffers1 : PROC

Padder MACRO func : REQ
    nop
    nop
    nop
    nop
    nop
    jmp func
ENDM

.CODE

TrSwapChainPresent PROC
    Padder HkSwapChainPresent
TrSwapChainPresent ENDP

TrSwapChain1Present1 PROC
    Padder HkSwapChain1Present1
TrSwapChain1Present1 ENDP

TrSwapChainResizeBuffers PROC
    Padder HkSwapChainResizeBuffers
TrSwapChainResizeBuffers ENDP

TrSwapChain3ResizeBuffers1 PROC
    Padder HkSwapChain3ResizeBuffers1
TrSwapChain3ResizeBuffers1 ENDP

END