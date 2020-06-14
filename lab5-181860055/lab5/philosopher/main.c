#include "lib.h"
#include "types.h"
#define N 5
#define MAX_SLEEP_TIME 128
#define sleepTime rand()%MAX_SLEEP_TIME

void think(int index){
	printf("Philosopher %d : think\n",index);
	sleep(sleepTime);
}

void eat(int index){
	printf("Philosopher %d : eat\n",index);
	sleep(sleepTime);
}

void philosopher(sem_t *forks,int index){
	int i=2;
	sem_t* left_fork = forks+index;
	sem_t* right_fork= forks+(index+1)%N;
	while(i-->0){
		think(index);
		if(index%2==0){
			sem_wait(left_fork);
			sleep(sleepTime);
			sem_wait(right_fork);
			sleep(sleepTime);
		}
		else{
			sem_wait(right_fork);
			sleep(sleepTime);
			sem_wait(left_fork);
			sleep(sleepTime);
		}
		eat(index);
		sem_post(left_fork);
		sleep(sleepTime);
		sem_post(right_fork);
		sleep(sleepTime);
	}
}

int main(void) {
	// TODO in lab4
	srand(0);
	printf("philosopher\n");
	sem_t forks[5];
	for(int i=0;i<5;++i)
		sem_init(forks+i,1);
	for(int i=0,ret=0;i<N;++i){
		ret = fork();
		if(ret == 0){
			philosopher(forks,i);
			exit();
		}

	}

	exit();
	return 0;
}
