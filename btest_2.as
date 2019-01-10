.entry main
.extern main
.extern sub
.extern extdef

main: mov undef, r1
	  sub r1, r2
	  inc r2
	  rts
	  
s: .struct 7, "qwerty"

loop: mov 11, r3
      not main
	  clr #4
	  lea str, loop
	  dec r2
	  jmp main
	  bne loop
	  red r9
	  stop

extdef: mov r1, r2
	  
label: .data 5, -5, 29, -29, 156

.string "zxcv"