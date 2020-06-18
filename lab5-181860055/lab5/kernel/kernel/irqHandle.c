#include "x86.h"
#include "device.h"
#include "fs.h"

#define SYS_WRITE 0
#define SYS_FORK 1
#define SYS_EXEC 2
#define SYS_SLEEP 3
#define SYS_EXIT 4
#define SYS_READ 5
#define SYS_SEM 6
#define SYS_GETPID 7
#define SYS_OPEN 8
#define SYS_LSEEK 9
#define SYS_CLOSE 10
#define SYS_REMOVE 11

#define STD_OUT 0
#define STD_IN 1
#define SH_MEM 3

#define SEM_INIT 0
#define SEM_WAIT 1
#define SEM_POST 2
#define SEM_DESTROY 3

#define O_WRITE 0x01
#define O_READ 0x02
#define O_CREATE 0x04
#define O_DIRECTORY 0x08

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

extern TSS tss;
extern ProcessTable pcb[MAX_PCB_NUM];
extern int current;

extern Semaphore sem[MAX_SEM_NUM];
extern Device dev[MAX_DEV_NUM];
extern File file[MAX_FILE_NUM];

extern SuperBlock sBlock;

extern int displayRow;
extern int displayCol;

extern uint32_t keyBuffer[MAX_KEYBUFFER_SIZE];
extern int bufferHead;
extern int bufferTail;

uint8_t shMem[MAX_SHMEM_SIZE];

void syscallHandle(struct TrapFrame *tf);
void syscallWrite(struct TrapFrame *tf);
void syscallRead(struct TrapFrame *tf);
void syscallFork(struct TrapFrame *tf);
void syscallExec(struct TrapFrame *tf);
void syscallSleep(struct TrapFrame *tf);
void syscallExit(struct TrapFrame *tf);
void syscallSem(struct TrapFrame *tf);
void syscallGetPid(struct TrapFrame *tf);

void syscallWriteStdOut(struct TrapFrame *tf);
void syscallReadStdIn(struct TrapFrame *tf);
void syscallWriteShMem(struct TrapFrame *tf);
void syscallReadShMem(struct TrapFrame *tf);

void GProtectFaultHandle(struct TrapFrame *tf);

void timerHandle(struct TrapFrame *tf);
void keyboardHandle(struct TrapFrame *tf);

void syscallSemInit(struct TrapFrame *tf);
void syscallSemWait(struct TrapFrame *tf);
void syscallSemPost(struct TrapFrame *tf);
void syscallSemDestroy(struct TrapFrame *tf);

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
		case SYS_WRITE:
			syscallWrite(tf);
			break; // for SYS_WRITE
		case SYS_READ:
			syscallRead(tf);
			break; // for SYS_READ
		case SYS_FORK:
			syscallFork(tf);
			break; // for SYS_FORK
		case SYS_EXEC:
			syscallExec(tf);
			break; // for SYS_EXEC
		case SYS_SLEEP:
			syscallSleep(tf);
			break; // for SYS_SLEEP
		case SYS_EXIT:
			syscallExit(tf);
			break; // for SYS_EXIT
		case SYS_SEM:
			syscallSem(tf);
			break; // for SYS_SEM
		case SYS_GETPID:
			syscallGetPid(tf);
			break; // for SYS_GETPID
		case SYS_OPEN:
			syscallOpen(tf);
			break;
		case SYS_LSEEK:
			syscallLseek(tf);
			break;
		case SYS_CLOSE:
			syscallClose(tf);
			break;
		case SYS_REMOVE:
			syscallRemove(tf);
			break;

		default:break;
	}
}

void timerHandle(struct TrapFrame *tf) {
	uint32_t tmpStackTop;
	int i = (current + 1) % MAX_PCB_NUM;
	while (i != current) {
		if (pcb[i].state == STATE_BLOCKED) {
			pcb[i].sleepTime--;
			if (pcb[i].sleepTime == 0) {
				pcb[i].state = STATE_RUNNABLE;
			}
		}
		i = (i + 1) % MAX_PCB_NUM;
	}

	if (pcb[current].state == STATE_RUNNING &&
			pcb[current].timeCount != MAX_TIME_COUNT) {
		pcb[current].timeCount++;
		return;
	}
	else {
		if (pcb[current].state == STATE_RUNNING) {
			pcb[current].state = STATE_RUNNABLE;
			pcb[current].timeCount = 0;
		}
		i = (current + 1) % MAX_PCB_NUM;
		while (i != current) {
			if (i != 0 && pcb[i].state == STATE_RUNNABLE) {
				break;
			}
			i = (i + 1) % MAX_PCB_NUM;
		}
		if (pcb[i].state != STATE_RUNNABLE) {
			i = 0;
		}
		current = i;
		// putChar('0' + current);
		pcb[current].state = STATE_RUNNING;
		pcb[current].timeCount = 1;
		tmpStackTop = pcb[current].stackTop;
		pcb[current].stackTop = pcb[current].prevStackTop;
		tss.esp0 = (uint32_t)&(pcb[current].stackTop);
		asm volatile("movl %0, %%esp"::"m"(tmpStackTop)); // switch kernel stack
		asm volatile("popl %gs");
		asm volatile("popl %fs");
		asm volatile("popl %es");
		asm volatile("popl %ds");
		asm volatile("popal");
		asm volatile("addl $8, %esp");
		asm volatile("iret");
	}
	return;
}

void keyboardHandle(struct TrapFrame *tf) {
	// TODO in lab4
	uint32_t keyCode = getKeyCode();
	if (keyCode == 0)
		return;
	//putChar(getChar(keyCode));
	//putInt(keyCode);
	keyBuffer[bufferTail] = keyCode;
	bufferTail = (bufferTail + 1) % MAX_KEYBUFFER_SIZE;
	
	
	// wake up dev[STD_IN]

	if(dev[STD_IN].value < 0){
		ProcessTable* pt = (ProcessTable*)((uint32_t)(dev[STD_IN].pcb.prev)-
							(uint32_t)&(((ProcessTable*)0)->blocked));
		dev[STD_IN].pcb.prev = (dev[STD_IN].pcb.prev)->prev;
		(dev[STD_IN].pcb.prev)->next = &(dev[STD_IN].pcb);
		pt->state = STATE_RUNNABLE;
		dev[STD_IN].value = 0;
	}
	

	return;
}

void syscallWrite(struct TrapFrame *tf) {	
	switch(tf->ecx) { // file descriptor
		case STD_OUT:
			if (dev[STD_OUT].state == 1) {
				syscallWriteStdOut(tf);
			}
			break; // for STD_OUT
		case SH_MEM:
			if (dev[SH_MEM].state == 1) {
				syscallWriteShMem(tf);
			}
			break; // for SH_MEM
		default:
			break;
	}
	int fd = tf->ecx - MAX_DEV_NUM;
	if(fd >=0 && fd < MAX_DEV_NUM+MAX_FILE_NUM){
		if(file[fd].state == 1){
			syscallWriteFile(tf);
		}
	}
}

void syscallWriteStdOut(struct TrapFrame *tf) {
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

void syscallWriteShMem(struct TrapFrame *tf) {
	// TODO in lab4
	
	int size = tf->ebx;
	int sel = tf->ds;
	int index = tf->esi;
	//int fd = tf->ecx;
	char *str = (char*)tf->edx;
	char character = 0;
	asm volatile("movw %0, %%es"::"m"(sel));
	for(int i=0;i<size;++i){
		asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str + i));
		*(shMem+index+i) = character;
	}
	return;
}

void syscallWriteFile(struct TrapFrame *tf){
	int fd = tf->ecx - MAX_DEV_NUM;
	if(!(file[fd].flags & O_WRITE)){
		tf->eax = 01;
		return;
	}
	int i = 0;
	int j = 0;
	int ret = 0;
	int count=0;
	int sel = tf->ds;
	char* *str = (char*)tf->edx;
	char buffer[SECTOR_SIZE * SECTORS_PER_BLOCK];
	int quotient = file[fd].offset / sBlock.blockSize;
	int remainder = file[fd].offset % sBlock.blockSize;
	int size = tf->ebx;
	asm volatile("movw %0, %%es"::"m"(sel));

	Inode inode;
	diskRead(&inode,sizeof(Inode),1,file[fd].inodeOffset);
	if(size<=0){
		tf->eax = 0;
		return;
	}

	if(inode.type == DIRECTORY_TYPE || file[fd].offset>inode.size){
		tf->eax = -1;
		return;
	}

	readSuperBlock(&sBlock);

	// write file

	int *fileOffset = &file[fd].offset;
	for(i=0,j=remainder;i<size;){
		if(quotient>=inode.blockCount){
			ret = allocBlock(&sBlock,&inode,file[fd].inodeOffset);
			if(ret == -1){
				break;
			}
		}
		ret = readBlock(&sBlock,&inode,quotient,buffer);
		if(ret == -1){
			break;
		}
		for(;j<SECTOR_SIZE*SECTORS_PER_BLOCK&&i<size;++j,++i){
			asm volatile("movb %%es:(%1), %0":"=r"(buffer + j):"r"(str + i));
			(*fileOffset)++;
		}
		j = 0;
		ret = writeBlock(&sBlock,&inode,quotient,buffer);
		if(ret == -1){
			break;
		}
		quotient++;
	}
	count = i;
	if(file[fd].offset > inode.size){
		inode.size = file[fd].offset;
		diskWrite(&inode,sizeof(Inode),1,file[fd].inodeOffset);
	}

	tf->eax = count;
	return;


}

void syscallRead(struct TrapFrame *tf) {
	switch(tf->ecx) {
		case STD_IN:
			if (dev[STD_IN].state == 1) {
				syscallReadStdIn(tf);
			}
			break;
		case SH_MEM:
			if (dev[SH_MEM].state == 1) {
				syscallReadShMem(tf);
			}
			break;
		default:
			break;
	}
	int fd = tf->ecx - MAX_DEV_NUM;
	if(fd >=0 && fd < MAX_DEV_NUM+MAX_FILE_NUM){
		if(file[fd].state == 1){
			syscallReadFile(tf);
		}
	}
}

void syscallReadStdIn(struct TrapFrame *tf) {
	// TODO in lab4
	if(dev[STD_IN].value == 0){
		pcb[current].blocked.next = dev[STD_IN].pcb.next;
		pcb[current].blocked.prev = &(dev[STD_IN].pcb);
		dev[STD_IN].pcb.next = &(pcb[current].blocked);
		(pcb[current].blocked.next)->prev = &(pcb[current].blocked);
		pcb[current].state = STATE_BLOCKED;
		pcb[current].sleepTime = 0xFFFFFFF;
		dev[STD_IN].value = -1;
		
	}
	else if(dev[STD_IN].value < 0){
		tf->eax = -1;
		return;
	}

	asm volatile("int $0x20");

	int sel = tf->ds;
	char *str = (char*)tf->edx;
	int i = 0;
	char character = 0;
	int count = 0;
	int size = (bufferTail + MAX_KEYBUFFER_SIZE - bufferHead) % MAX_KEYBUFFER_SIZE;
	asm volatile("movw %0, %%es"::"m"(sel));
	for(;i<size;++i){
		character = getChar(keyBuffer[bufferHead+i]);
		
		if(character>0){
			putChar(character);
			asm volatile("movb %0, %%es:(%1)"::"r"(character),"r"(str+count));
			++count;
		}
		
	}
	character = 0;
	asm volatile("movb %0, %%es:(%1)"::"r"(character),"r"(str+count));
	bufferTail = bufferHead;
	tf->eax = count;
	return;
}

void syscallReadShMem(struct TrapFrame *tf) {
	int size = tf->ebx;
	int sel = tf->ds;
	int index = tf->esi;
	//int fd = tf->ecx;
	char *str = (char*)tf->edx;
	char character = 0;
	
	asm volatile("movw %0, %%es"::"m"(sel));
	for(int i=0;i<size;++i){
		asm volatile("movb %0, %%es:(%1)"::"r"(character),"r"(str+i));
	}


	return;
}

void syscallReadFile(struct TrapFrame *tf) {
	int fd = tf->ecx - MAX_DEV_NUM;
	if(!(file[fd].flags&O_READ)){
		tf->eax = -1;
		return;
	}

	int ret = 0;
	int size = tf->ebx;
	int sel = tf->ds;
	char *str = (char*)tf->edx;
	int i = 0;
	int j = 0;
	char buffer[SECTOR_SIZE * SECTORS_PER_BLOCK];
	int quotient = file[fd].offset / sBlock.blockSize;
	int remainder = file[fd].offset % sBlock.blockSize;
	asm volatile("movw %0, %%es"::"m"(sel));

	Inode inode;
	diskRead(&inode,sizeof(Inode),1,file[fd].inodeOffset);

	if(file[fd].offset>inode.size){
		tf->eax = -1;
		return;
	}

	if(size>inode.size-file[fd].offset){ 		
		size = inode.size - file[fd].offset;
	}

	if(size<=0){
		tf->eax = 0;
		return;
	}

	readSuperBlock(&sBlock);
	int *fileOffset = &file[fd].offset;
	for(i=0,j=remainder;i<size;){
		if(quotient>=inode.blockCount){
			break;
		}
		ret = readBlock(&sBlock,&inode,quotient,buffer);
		if(ret == -1){
			break;
		}
		for(;j<SECTORS_PER_BLOCK*SECTOR_SIZE&&i<size;++j,++i){
			asm volatile("movb %0, %%es:(%1)"::"r"(buffer + j),"r"(str+i));
			(*fileOffset)++;
		}
		j = 0;
		quotient++;
	}
	tf->eax = i;
	return;

}

void syscallFork(struct TrapFrame *tf) {
	int i, j;
	for (i = 0; i < MAX_PCB_NUM; i++) {
		if (pcb[i].state == STATE_DEAD) {
			break;
		}
	}
	if (i != MAX_PCB_NUM) {
		pcb[i].state = STATE_PREPARING;

		enableInterrupt();
		for (j = 0; j < 0x100000; j++) {
			*(uint8_t *)(j + (i + 1) * 0x100000) = *(uint8_t *)(j + (current + 1) * 0x100000);
		}
		disableInterrupt();

		pcb[i].stackTop = (uint32_t)&(pcb[i].stackTop) -
			((uint32_t)&(pcb[current].stackTop) - pcb[current].stackTop);
		pcb[i].prevStackTop = (uint32_t)&(pcb[i].stackTop) -
			((uint32_t)&(pcb[current].stackTop) - pcb[current].prevStackTop);
		pcb[i].state = STATE_RUNNABLE;
		pcb[i].timeCount = pcb[current].timeCount;
		pcb[i].sleepTime = pcb[current].sleepTime;
		pcb[i].pid = i;
		pcb[i].regs.ss = USEL(2 + i * 2);
		pcb[i].regs.esp = pcb[current].regs.esp;
		pcb[i].regs.eflags = pcb[current].regs.eflags;
		pcb[i].regs.cs = USEL(1 + i * 2);
		pcb[i].regs.eip = pcb[current].regs.eip;
		pcb[i].regs.eax = pcb[current].regs.eax;
		pcb[i].regs.ecx = pcb[current].regs.ecx;
		pcb[i].regs.edx = pcb[current].regs.edx;
		pcb[i].regs.ebx = pcb[current].regs.ebx;
		pcb[i].regs.xxx = pcb[current].regs.xxx;
		pcb[i].regs.ebp = pcb[current].regs.ebp;
		pcb[i].regs.esi = pcb[current].regs.esi;
		pcb[i].regs.edi = pcb[current].regs.edi;
		pcb[i].regs.ds = USEL(2 + i * 2);
		pcb[i].regs.es = pcb[current].regs.es;
		pcb[i].regs.fs = pcb[current].regs.fs;
		pcb[i].regs.gs = pcb[current].regs.gs;
		/*XXX set return value */
		pcb[i].regs.eax = 0;
		pcb[current].regs.eax = i;
	}
	else {
		pcb[current].regs.eax = -1;
	}
	return;
}

void syscallExec(struct TrapFrame *tf) {
	int sel = tf->ds;
	char *str = (char *)tf->ecx;
	char tmp[128];
	int i = 0;
	char character = 0;
	int ret = 0;
	uint32_t entry = 0;
	asm volatile("movw %0, %%es"::"m"(sel));
	asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str + i));
	while (character != 0) {
		tmp[i] = character;
		i++;
		asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str + i));
	}
	tmp[i] = 0;

	ret = loadElf(tmp, (current + 1) * 0x100000, &entry);
	if (ret == -1) {
		tf->eax = -1;
		return;
	}
	tf->eip = entry;
	return;
}

void syscallSleep(struct TrapFrame *tf) {
	putString("sleep ");putInt(current);
	putString("sleepTime ");putInt(tf->ecx);
	if (tf->ecx == 0) {
		return;
	}
	else {
		pcb[current].state = STATE_BLOCKED;
		pcb[current].sleepTime = tf->ecx;
		asm volatile("int $0x20");
		return;
	}
	return;
}

void syscallExit(struct TrapFrame *tf) {
	pcb[current].state = STATE_DEAD;
	asm volatile("int $0x20");
	return;
}

void syscallSem(struct TrapFrame *tf) {
	switch(tf->ecx) {
		case SEM_INIT:
			syscallSemInit(tf);
			break;
		case SEM_WAIT:
			syscallSemWait(tf);
			break;
		case SEM_POST:
			syscallSemPost(tf);
			break;
		case SEM_DESTROY:
			syscallSemDestroy(tf);
			break;
		default:break;
	}
}

void syscallSemInit(struct TrapFrame *tf) {
	// TODO in lab4
	uint32_t value = tf->edx;
	Semaphore* p = NULL;
	for(int i=0;i<MAX_SEM_NUM;i++){
		if(sem[i].state == 0){
			p = &sem[i];
		}
	}
	if(p){
		p->value = value;
		p->state = 1;
		tf->eax = (uint32_t)p;
	}
	else{
		tf->eax = -1;
	}
	return;
}

void syscallSemWait(struct TrapFrame *tf) {
	// TODO in lab4
	Semaphore* p = (Semaphore*)tf->edx;
	p->value--;
	if(p->value<0){
		pcb[current].blocked.next = p->pcb.next;
		pcb[current].blocked.prev = &(p->pcb);
		p->pcb.next = &(pcb[current].blocked);
		(pcb[current].blocked.next)->prev = &(pcb[current].blocked);
		pcb[current].state = STATE_BLOCKED;
		pcb[current].sleepTime = 0xFFFFFFF;
		tf->eax = 0;
		asm volatile("int $0x20");
	}
	else{
		tf->eax = -1;
	}
	return;
}

void syscallSemPost(struct TrapFrame *tf) {
	// TODO in lab4
	Semaphore* p = (Semaphore*)tf->edx;
	p->value++;
	if(p->value<=0){

		ProcessTable* pt = (ProcessTable *)((uint32_t)(p->pcb.prev) - (uint32_t) & (((ProcessTable *)0)->blocked));
		p->pcb.prev = (p->pcb.prev)->prev;
		(p->pcb.prev)->next = &(p->pcb);
		pt->state = STATE_RUNNABLE;
		tf->eax = 0;
	}
	else{
		tf->eax = -1;
	}
	return;
}

void syscallSemDestroy(struct TrapFrame *tf) {
	// TODO in lab4
	Semaphore* p = (Semaphore*)tf->edx;
	if(p->value<0){
		tf->eax = -1;
		return;
	}
	p->value = 0;
	p->state = 0;
	p->pcb.next = &(p->pcb);
	p->pcb.prev = &(p->pcb);
	tf->eax = 0;
	return;
}

void syscallGetPid(struct TrapFrame *tf) {
	pcb[current].regs.eax = current;
	return;
}

void GProtectFaultHandle(struct TrapFrame *tf){
	assert(0);
	return;
}

void syscallOpen(struct TrapFrame *tf) {
	int i;
	int length = 0;
	int ret = 0;
	int size = 0;
	int baseAddr = (current + 1) * 0x100000; // base address of user process
	char *str = (char*)tf->ecx + baseAddr; // file path
	char fatherPath[NAME_LENGTH<<3];
	char filename[NAME_LENGTH];
	Inode fatherInode;
	Inode destInode;
	int fatherInodeOffset = 0;
	int destInodeOffset = 0;

	uint8_t mode = tf->edx;
	uint8_t o_create = mode & O_CREATE;
	uint8_t o_directory = mode & O_DIRECTORY;
	uint8_t o_read = mode & O_READ;
	uint8_t o_write = mode & O_WRITE;
	if(o_directory&&o_write){
		tf->eax = -1;
		return;
	}
	readSuperBlock(&sBlock);
	ret = readInode(&sBlock,&destInode,&destInodeOffset,str);

	
	if(ret == 0){
		if((destInode.type == DIRECTORY_TYPE)^(o_directory>0)){
			tf->eax = -1;
			return;
		}
		
		for(i=0;i<MAX_DEV_NUM;++i){
			if(dev[i].state&&dev[i].inodeOffset==destInodeOffset){
				--dev[i].value;
				if(dev[i].value<0){
					pcb[current].blocked.next = dev[i].pcb.next;
					pcb[current].blocked.prev = &(dev[i].pcb);
					dev[i].pcb.next = &(pcb[current].blocked);
					(pcb[current].blocked.next)->prev = &(pcb[current].blocked);
					pcb[current].state = STATE_BLOCKED;
					pcb[current].sleepTime = 0xFFFFFFF;
					tf->eax = 0;
					asm volatile("int $0x20");
				}
				tf->eax = i;
				return;
			}
		}
		

		for(i=0;i<MAX_FILE_NUM;++i){
			if(!file[i].state){
				file[i].state = 1;
				file[i].inodeOffset = destInodeOffset;
				file[i].flags = mode;
				file[i].offset = 0;
				tf->eax = i + MAX_DEV_NUM;			
				return;
			}
		}

	}
	else{
		if(!o_create){
			tf->eax = -1;
			return;
		}
		length = stringLen(str);
		if(str[length-1]=='/'){
			if(!o_directory){
				tf->eax = -1;
				return;
			}
			--length;
		}
		stringCpy(str,fatherPath,length);
		stringChrR(fatherPath,'/',&size);
		stringCpy(str+size+1,filename,length);

		readSuperBlock(&sBlock);
		ret = readInode(&sBlock, &fatherInode, &fatherInodeOffset, fatherPath);
		if(ret == -1){
			tf->eax = -1;
			return;
		}
		for (i = 0; i < MAX_FILE_NUM; ++i)
		{
			if (!file[i].state)
			{
				break;
			}
		}
		if (i == MAX_FILE_NUM)
		{
			tf->eax = -1;
			return;
		}
		readSuperBlock(&sBlock);
		ret = allocInode(&sBlock, &fatherInode, fatherInodeOffset, &destInode, &destInodeOffset,
						 filename, o_directory ? DIRECTORY_TYPE : REGULAR_TYPE);

		if (ret == -1)
		{
			tf->eax = -1;
			return;
		}
		file[i].state = 1;
		file[i].inodeOffset = destInodeOffset;
		file[i].flags = mode;
		file[i].offset = 0;
		tf->eax = i + MAX_DEV_NUM;
	}

	return;
}

void syscallLseek(struct TrapFrame *tf) {
	int fd = tf->ecx - MAX_DEV_NUM;
	if(fd<0||fd>=MAX_FILE_NUM){
		tf->eax = -1;
		return;
	}
	int target = -1;
	int whence = tf->ebx;
	Inode inode;
	diskReadd(&inode,sizeof(Inode),1,file[fd].inodeOffset);
	switch(whence){
		case SEEK_SET:
			target = tf->edx;
			break;
		case SEEK_CUR:
			target = tf->edx + file[fd].offset;
			break;
		case SEEK_END:
			target = tf->edx + inode.size;
			break;
		default:
			break;
	}

	if(target<0||target>inode.size){
		tf->eax = -1;
		return;
	}

	file[fd].offset = target;
	tf->eax = target;
	return;
}

void syscallClose(struct TrapFrame *tf) {
	if(tf->ecx<0||tf->ecx>=MAX_DEV_NUM+MAX_FILE_NUM){
		tf->eax = -1;
		return;
	}
	int fd = tf->ecx - MAX_DEV_NUM;
	if(fd>0){
		tf->eax = file[fd].state == 0 ? 0:-1;
		file[fd].state = 0;
		return;
	}
	else{
		int i = tf->ecx;
		if(dev[i].state){
			dev[i].value++;
			if(dev[i].value<=0){
				ProcessTable* pt = (ProcessTable *)((uint32_t)(dev[i].pcb.prev) - (uint32_t) & (((ProcessTable *)0)->blocked));
				dev[i].pcb.prev = (dev[i].pcb.prev)->prev;
				(dev[i].pcb.prev)->next = &(dev[i].pcb);
				pt->state = STATE_RUNNABLE;
				tf->eax = 0;
			}
			tf->eax = 0;
			return;
		}
		tf->eax = -1;
		return;
	}
	return;
}

void syscallRemove(struct TrapFrame *tf) {

	return;
}