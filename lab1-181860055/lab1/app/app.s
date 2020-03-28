.code32

.global start
start:
	pushl $14
	pushl $message
	calll displayStr
loop:
	jmp loop
	
message:
	.string "Hello, World!\n\0"

displayStr:
	movl 4(%esp), %ebx
	movl 8(%esp), %ecx
	movl $0, %edi
	movb $0xf0, %ah
nextChar:
	movb (%ebx), %al
	movw %ax, %gs:(%edi)
	addl $2, %edi
	incl %ebx
	loopnz nextChar # loopnz decrease ecx by 1
	ret
