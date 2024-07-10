.ORIG x3000
LEA R0, HELLO_STR  ; load addr of prompt str into R0
PUTS

AND R0, R0, 0      ; R0 = 0

LOOP               ; label to begin loop
	ADD R0, R0, 1    ; R0 += 1
	ADD R1, R0, -10  ; R1 = R0 - 10
  BRn LOOP         ; goto LOOP for 10 iterations

DONE
LEA R0, DONE_STR
PUTS

HALT

;----------------------------------------
; subroutines
;----------------------------------------

; prompt for a character and read it to R0
PROMPTC
  LEA R0, PROMPT_STR
  IN  ; get a char
  LEA R0, NEWLINE_STR
  PUTS

;----------------------------------------
; strings
;----------------------------------------
HELLO_STR .STRINGZ "\nBegin test.asm\n"
NEWLINE_STR .STRINGZ "\n"
PROMPT_STR .STRINGZ "Enter a character: "
DONE_STR .STRINGZ "All done!\n"

.END
