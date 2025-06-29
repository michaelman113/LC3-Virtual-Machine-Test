        .ORIG x4000
        LD R2, ASCII_ZERO   ; R2 = x30 ('0')
        
LOOP    TRAP x31            ; R0 ← value from producer
        ADD R0, R0, R2      ; R0 += ASCII '0'
        TRAP x21            ; print as char
        TRAP x22            ; print newline (optional)
        BRnzp LOOP

ASCII_ZERO .FILL x30
        .END
