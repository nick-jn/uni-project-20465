.entry main
.extern str

main: mov str, r1
	  sub r1, r2
	  inc r2
	  rts
	  
s: .struct 7, "qwerty"

loop: mov r2, r3
      not str
	  clr s
	  lea str, loop
	  dec r2
	  jmp main
	  bne loop
	  red r7
	  stop
	  
label: .data 5, -5, 29, -29, 156

.string "zxcv"