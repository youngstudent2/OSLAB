#include "lib.h"
#include "types.h"



int main(void) {
	// TODO in lab4
	printf("bounded_buffer\n");
	int n = 4;
	sem_t mutex,fullBuffers,emptyBuffers;
	sem_init(&mutex,1);
	sem_init(&fullBuffers,0); 
	sem_init(&emptyBuffers,n);

	int ret = fork();
	if(ret == 0){
		//生产者
		sem_wait(&emptyBuffers);
		sem_wait(&mutex);
		//write();
		sem_post(&mutex);
		sem_post(&fullBuffers);
	}
	else if(ret != -1){
		sem_wait(&fullBuffers);
		sem_wait(&mutex);
		//read();
		sem_post(&mutex);
		sem_post(&emptyBuffers);
	}

	exit();
	return 0;
}
