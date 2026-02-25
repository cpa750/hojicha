.global _start
.type _start, @function
.extern main

_start:
	call main

	/* Invalid instruction to trap the kernel into terminating the process */
	ud2

.size _start, . - _start

.section .note.GNU-stack,"",@progbits
