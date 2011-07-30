.text

.global i386_snapshot
i386_snapshot:
    movl (%esp), %edi
    pusha
    movl %esp, %esi
    movl $8, %ecx
    rep movsl
    popa
    ret

