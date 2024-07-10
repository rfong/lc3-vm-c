.ORIG x3000
LEA R0, HELLO_STR  ; load addr of prompt str into R0
PUTS

AND R1, R1, 0      ; R1 = 0

LOOP               ; label to begin loop
	LEA R0, LOOPING_STR
	PUTS

	ADD R1, R1, 1    ; R1 += 1
	ADD R2, R1, -10  ; R2 = R1 - 10
  BRn LOOP         ; goto LOOP for 10 iterations

HALT

;----------------------------------------
; strings
;----------------------------------------
HELLO_STR .STRINGZ "\nBegin.\n"
LOOPING_STR .STRINGZ "Looping.\n"

.END
