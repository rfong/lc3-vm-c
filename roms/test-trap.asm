.ORIG x3000
LEA R0, PROMPT_STR  ; load addr of prompt str into R0
TRAP_PUTS
TRAP_GETC
TRAP_OUT
;HALT
PROMPT_STR .STRINGZ "Enter a character: "
.END
