mov #1, r2
cmp dat, dat
add r3, r4
sub #-5, stc
not stc.2
clr r1
lea dat, stc
inc stc.1
dec r2
red dat
prn #127
jsr stc
rts
stop

dat: .data 5, -5, 217, -233
stc: .struct -99, "aaa"