#include "lib.h"
#include "types.h"
#define MAX_SLEEP_TIME 128
#define sleepTime rand()%MAX_SLEEP_TIME

void produce(int index){
	printf("Producer %d : produce\n",index);
	sleep(sleepTime);
}
void consume(int index){
	printf("Consumer : consume\n");
	sleep(sleepTime);
}
void deposit(sem_t* mutex, sem_t* fullBuffers, sem_t* emptyBuffers,int index)
{
	//生产者
	int i = 4;
	while (i-- > 0)
	{
		//printf("producer %d is waiting for write\n",index);
		sem_wait(emptyBuffers);
		sleep(sleepTime);
		//printf("producer %d is waiting for critical area\n",index);
		sem_wait(mutex);
		sleep(sleepTime);
		produce(index);
		sem_post(mutex);
		sleep(sleepTime);
		//printf("producer %d quit critical area\n",index);
		sem_post(fullBuffers);
		sleep(sleepTime);
		//printf("producer %d sleep\n",index);
		//sleep(sleepTime);
	}
}
void remove(sem_t* mutex, sem_t* fullBuffers, sem_t* emptyBuffers,int index)
{
	int i = 4;
	while (i-- > 0)
	{
		//printf("consumer %d is waiting for read\n",index);
		sem_wait(fullBuffers);
		sleep(sleepTime);
		//printf("consumer %d is waiting for critical area\n",index);
		sem_wait(mutex);
		sleep(sleepTime);
		consume(index);
		sem_post(mutex);
		sleep(sleepTime);
		//printf("consumer %d quit critical area\n",index);
		sem_post(emptyBuffers);
		sleep(sleepTime);
		//printf("consumer %d sleep\n",index);
		//sleep(sleepTime);
	}
}
int main(void) {
	// TODO in lab4
	srand(0);
	printf("bounded_buffer\n");
	int n = 4;    	// buffer size	
	int producer = 4;
	int consumer = 1;
	sem_t mutex,fullBuffers,emptyBuffers;
	sem_init(&mutex,1);
	sem_init(&fullBuffers,0); 
	sem_init(&emptyBuffers,n);
	int ret;
	while(producer-->0){
		ret = fork();
		
		if(ret == 0){
			//printf("fork producer %d\n",producer);
			deposit(&mutex,&fullBuffers,&emptyBuffers,producer);
			exit();
		}		
	}
	while(consumer-->0){
		ret = fork();
		
		if(ret == 0){
			//printf("fork consumer %d\n",consumer);
			remove(&mutex,&fullBuffers,&emptyBuffers,consumer);
			exit();
		}	
	}
	exit();
	return 0;
}
