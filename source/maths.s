.GLOBAL	Addition
.THUMB_FUNC
Addition:	
	push { r4-r7, lr }
	mov r4, r1
	add r4, r2
	mov r0, #0;
	add r0, r4
	pop { r4-r7 }
	pop { r3 }	
	bx r3
