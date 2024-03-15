.code

extern g_ImportFunctions:QWORD

vSetDdrawflag PROC
    jmp g_ImportFunctions[0*8]
vSetDdrawflag ENDP

AlphaBlend PROC
    jmp g_ImportFunctions[1*8]
AlphaBlend ENDP

DllInitialize PROC
    jmp g_ImportFunctions[2*8]
DllInitialize ENDP

GradientFill PROC
    jmp g_ImportFunctions[3*8]
GradientFill ENDP

TransparentBlt PROC
    jmp g_ImportFunctions[4*8]
TransparentBlt ENDP

end