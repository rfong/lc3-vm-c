.ORIG x3000        ; start loading program here
LEA R0, HELLO_STR  ; load the address of the HELLO_STR string into R0
PUTs               ; output the string pointed to by R0 to the console
HALT               ; halt the program

HELLO_STR .STRINGZ "Hello World!"  ; store this string here in the program
.END               ; mark the end of the file
