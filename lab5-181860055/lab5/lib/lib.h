#ifndef __lib_h__
#define __lib_h__

#include "types.h"

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

#define MAX_BUFFER_SIZE 256

#define O_WRITE 0x01
#define O_READ 0x02
#define O_CREATE 0x04
#define O_DIRECTORY 0x08

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

int printf(const char *format,...);

int scanf(const char *format,...);

pid_t fork();

int exec(const char *filename, char * const argv[]);

int sleep(uint32_t time);

int exit();

int write(int fd, uint8_t *buffer, int size, ...);

int read(int fd, uint8_t *buffer, int size, ...);

int sem_init(sem_t *sem, uint32_t value);

int sem_wait(sem_t *sem);

int sem_post(sem_t *sem);

int sem_destroy(sem_t *sem);

int getpid();

int open(const char* filename, uint8_t mode);

int lseek(int fd, uint32_t offset, uint8_t origin);

int remove(const char *filename);

int close(int fd);

// add rand method
int index;
int MT[624]; 

void srand(int seed);
void generate();
int rand();


#endif
