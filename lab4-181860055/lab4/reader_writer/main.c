#include "lib.h"
#include "types.h"
#define MAX_SLEEP_TIME 128
#define sleepTime rand()%MAX_SLEEP_TIME

inline void W(int index){
	printf("Writer %d : write\n",index);
	sleep(sleepTime);
}

inline void R(int index,int rcount){
	printf("Reader %d : read, total reader: %d\n",index,rcount);
	sleep(sleepTime);
}

void writer(int index,sem_t* writemutex){
	int round = 5;
	while(round-->0){
		//printf("writer %d wait\n",index);
		sem_wait(writemutex);
		sleep(sleepTime);
		W(index);
		sem_post(writemutex);
		//printf("writer %d finish writing\n",index);
		sleep(sleepTime);
	}
}

void reader(int index,sem_t* writemutex,sem_t* countmutex){
	int round = 5;
	int Rcount = 0;
	while(round-->0){
		//printf("reader %d wait for count\n",index);
		sem_wait(countmutex);
		sleep(sleepTime);
		read(SH_MEM, (uint8_t *)&Rcount, 4, 0);
		//printf("Rcount = %d ++\n",Rcount);
		if(Rcount == 0){
			//printf("reader %d wait writer write\n",index);
			sem_wait(writemutex);
			sleep(sleepTime);
		}
		++Rcount;
		write(SH_MEM, (uint8_t *)&Rcount, 4, 0);
		//printf("reader %d release Rcount\n",index);
		sem_post(countmutex);
		sleep(sleepTime);


		R(index,Rcount);
		//printf("reader %d wait for count\n",index);

		sem_wait(countmutex);
		sleep(sleepTime);
		read(SH_MEM, (uint8_t *)&Rcount, 4, 0);
		//printf("Rcount = %d --\n",Rcount);
		--Rcount;
		write(SH_MEM, (uint8_t *)&Rcount, 4, 0);
		
		if(Rcount == 0){
			//printf("reader %d tell writer write\n",index);
			sem_post(writemutex);
			sleep(sleepTime);
		}
		sem_post(countmutex);
		//printf("reader %d sleep\n",index);
		sleep(sleepTime);
	}
}




int main(void) {
	// TODO in lab4
	srand(0);
	printf("reader_writer\n");

	int ret;
	sem_t writemutex,countmutex;
	int reader_num = 3;
	int writer_num = 3;
	sem_init(&writemutex,1);
	sem_init(&countmutex,1);
	int rcount = 0;
	write(SH_MEM,(uint8_t*)&rcount,4,0);

	while(writer_num-->0){
		ret = fork();
		if(ret == 0){
			//printf("writer %d setup\n",writer_num);
			writer(writer_num,&writemutex);
			exit();
		}
	}

	while(reader_num-->0){
		ret = fork();
		if(ret == 0){
			//printf("reader %d setup\n",reader_num);
			reader(reader_num,&writemutex,&countmutex);
			exit();
		}
	}	
	


	exit();
	return 0;
}
