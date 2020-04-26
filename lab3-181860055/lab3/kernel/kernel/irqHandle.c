#include "x86.h"
#include "device.h"

extern TSS tss;
extern ProcessTable pcb[MAX_PCB_NUM];
extern int current;

extern int displayRow;
extern int displayCol;

extern uint32_t keyBuffer[MAX_KEYBUFFER_SIZE];
extern int bufferHead;
extern int bufferTail;

void syscallHandle(struct TrapFrame *tf);
void syscallWrite(struct TrapFrame *tf);
void syscallPrint(struct TrapFrame *tf);
void syscallFork(struct TrapFrame *tf);
void syscallExec(struct TrapFrame *tf);
void syscallSleep(struct TrapFrame *tf);
void syscallExit(struct TrapFrame *tf);

void GProtectFaultHandle(struct TrapFrame *tf);

void timerHandle(struct TrapFrame *tf);
void keyboardHandle(struct TrapFrame *tf);

void irqHandle(struct TrapFrame *tf) { // pointer tf = esp
	/*
	 * 中断处理程序
	 */
	/* Reassign segment register */
	asm volatile("movw %%ax, %%ds"::"a"(KSEL(SEG_KDATA)));
	
	uint32_t tmpStackTop = pcb[current].stackTop;
	pcb[current].prevStackTop = pcb[current].stackTop;
	pcb[current].stackTop = (uint32_t)tf;

	switch(tf->irq) {
		case -1:
			break;
		case 0xd:
			GProtectFaultHandle(tf); // return
			break;
		case 0x20:
			//putInt(20);
			timerHandle(tf);         // return or iret
			break;
		case 0x21:
			keyboardHandle(tf);      // return
			break;
		case 0x80:
			syscallHandle(tf);       // return
			break;
		default:assert(0);
	}

	pcb[current].stackTop = tmpStackTop;
}

void syscallHandle(struct TrapFrame *tf) {
	switch(tf->eax) { // syscall number
		case 0:
			syscallWrite(tf);
			break; // for SYS_WRITE
		case 1:
			syscallFork(tf);
			break; // for SYS_FORK
		case 2:
			syscallExec(tf);
			break; // for SYS_EXEC
		case 3:
			syscallSleep(tf);
			break; // for SYS_SLEEP
		case 4:
			syscallExit(tf);
			break; // for SYS_EXIT
		default:break;
	}
}

void switch_proc(){
	

	int next = -1;
	for(int i=0;i<MAX_PCB_NUM;++i){
		if(pcb[i].state == STATE_RUNNABLE){
			next = i;
			putInt(next);
			break;
		}
	}

	pcb[next].state = STATE_RUNNING;
	pcb[current].state = STATE_RUNNABLE;
	//TODO:save some info?
	current = next;

	uint32_t tmpStackTop = pcb[current].stackTop;
	pcb[current].stackTop = pcb[current].prevStackTop;
	tss.esp0 = (uint32_t)&(pcb[current].stackTop);
	asm volatile("movl %0, %%esp" ::"m"(tmpStackTop)); // switch kernel stack
	asm volatile("popl %gs");
	asm volatile("popl %fs");
	asm volatile("popl %es");
	asm volatile("popl %ds");
	asm volatile("popal");
	asm volatile("addl $8, %esp");
	asm volatile("iret");
	
}

void timerHandle(struct TrapFrame *tf) {
	// TODO in lab3
	for(int i=0;i<MAX_PCB_NUM;++i){
		if(pcb[i].state == STATE_BLOCKED){
			if(--pcb[i].sleepTime==0)
				pcb[i].state = STATE_RUNNABLE;
		}		
	}
	if(--pcb[current].timeCount==0){
		putString("switch:");
		putInt(current);
		switch_proc();
		putInt(current);
	}
	return;
}

void keyboardHandle(struct TrapFrame *tf) {
	uint32_t keyCode = getKeyCode();
	if (keyCode == 0)
		return;
	putChar(getChar(keyCode));
	keyBuffer[bufferTail] = keyCode;
	bufferTail = (bufferTail + 1) % MAX_KEYBUFFER_SIZE;
	return;
}

void syscallWrite(struct TrapFrame *tf) {
	switch(tf->ecx) { // file descriptor
		case 0:
			syscallPrint(tf);
			break; // for STD_OUT
		default:break;
	}
}

void syscallPrint(struct TrapFrame *tf) {
	int sel = tf->ds; //TODO segment selector for user data, need further modification
	char *str = (char *)tf->edx;
	int size = tf->ebx;
	int i = 0;
	int pos = 0;
	char character = 0;
	uint16_t data = 0;
	asm volatile("movw %0, %%es"::"m"(sel));
	for (i = 0; i < size; i++) {
		asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str + i));
		if (character == '\n') {
			displayRow++;
			displayCol = 0;
			if (displayRow == 25){
				displayRow = 24;
				displayCol = 0;
				scrollScreen();
			}
		}
		else {
			data = character | (0x0c << 8);
			pos = (80 * displayRow + displayCol) * 2;
			asm volatile("movw %0, (%1)"::"r"(data),"r"(pos + 0xb8000));
			displayCol++;
			if (displayCol == 80){
				displayRow++;
				displayCol = 0;
				if (displayRow == 25){
					displayRow = 24;
					displayCol = 0;
					scrollScreen();
				}
			}
		}
	}
	
	updateCursor(displayRow, displayCol);
	//TODO take care of return value
}

void syscallFork(struct TrapFrame *tf) {
	// TODO in lab3
	putString("fork\n");
	//TODO make sure how to use gdt 

	//find a dead process
	struct ProcessTable* new_pcb = NULL;
	int slot = -1;
	for(int i=0;i<MAX_PCB_NUM;++i){
		if(pcb[i].state == STATE_DEAD){
			new_pcb = &pcb[i];
			slot = i;
			break;
		}
	}
	if(new_pcb == NULL){
		pcb[current].regs.eax = -1; //fork fail
		return;
	}
		
	//拷贝父进程内容到子进程中
	memcpy((void*)new_pcb->stack,(void*)pcb[current].stack,MAX_STACK_SIZE*sizeof(uint32_t));
	memcpy(&new_pcb->regs,tf,sizeof(struct TrapFrame));
	
	new_pcb->stackTop = (uint32_t)&(new_pcb->regs);
	new_pcb->prevStackTop = (uint32_t)&(new_pcb->stackTop);
	new_pcb->regs.es = new_pcb->regs.ss = new_pcb->regs.ds = USEL(2+slot*2);
	new_pcb->regs.gs = new_pcb->regs.fs = new_pcb->regs.es;
	new_pcb->regs.cs = USEL(1+slot*2);	
	new_pcb->state = STATE_RUNNABLE;
	new_pcb->timeCount = MAX_TIME_COUNT;
	new_pcb->sleepTime = 0;
	new_pcb->pid = slot;
	putString("fork new process:");
	putInt(slot);
	new_pcb->regs.eax = 0; // return success value
	pcb[current].regs.eax = new_pcb->pid; // return pid for parent process
	
}

void syscallExec(struct TrapFrame *tf) {
	// TODO in lab3
	putString("exec\n");
	//putInt(tf->eip);
	char filename[100];
	int sel = tf->ds;
	char *str = (char*)tf->ecx;
	char character = 1;
	asm volatile("movw %0, %%es"::"m"(sel));
	for(int i=0;character!='\0';++i){
		asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str + i));
		//putChar(character);
		filename[i] = character;
	}
	
	putString(filename);
	uint32_t entry;
	int ret = loadElf(filename, (current + 1) * 0x100000, &entry);
	if(ret == -1){
		tf->eax = -1;
		putString("load elf failed!\n");
		return;
	}
	
	tf->eip = entry;
	putInt(entry);

	return;
}

void syscallSleep(struct TrapFrame *tf) {
	// TODO in lab3
	putString("sleep\n");
	pcb[current].state = STATE_BLOCKED;
	pcb[current].sleepTime = tf->ecx;
	pcb[current].timeCount = MAX_TIME_COUNT;
	asm volatile("int $0x20");
	return;
}

void syscallExit(struct TrapFrame *tf) {
	// TODO in lab3
	putString("exit");
	pcb[current].state = STATE_DEAD;
	asm volatile("int $0x20");
	return;
}

void GProtectFaultHandle(struct TrapFrame *tf){
	assert(0);
	return;
}
