        .ORIG x3000      ; VM’s PC_START
        AND  R0, R0, #0  ; running sum
        AND  R1, R1, #0  ; counter
        ADD  R2, R2, #1  ; constant 1
LOOP    ADD  R1, R1, R2  ; counter++
        ADD  R0, R0, R1  ; sum += counter
        BRnzp LOOP       ; spin forever
        .END
