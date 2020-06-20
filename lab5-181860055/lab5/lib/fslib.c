#include "lib.h"
#include "types.h"
#define DIRENTRY_SIZE 128
union DirEntry {
	uint8_t byte[DIRENTRY_SIZE];
	struct {
		int32_t inode;
		char name[64];
	};
};
typedef union DirEntry DirEntry;
int ls(const char *path){
    int fd;
    int ret;
    int size;
    uint8_t buffer[MAX_BUFFER_SIZE];
    int i;
    DirEntry* dirEntry;
    printf("ls %s\n",path);
    fd = open(path,O_READ|O_DIRECTORY);
    if(fd == -1){
        printf("ls failed\n");
        return -1;
    }
    size = lseek(fd,0,SEEK_END);
    lseek(fd,0,SEEK_SET);
    for(i=0;i<size/DIRENTRY_SIZE;++i){
        ret = read(fd,buffer,DIRENTRY_SIZE);
        if(ret == -1){
            printf("ls failed\n");
            return -1;
        }
        dirEntry = (DirEntry*)buffer;
        if(dirEntry->inode != 0){
            printf("%s ",dirEntry->name);
        }

    }
    printf("\n");
    ret = close(fd);
    printf("ls success\n");
    return 0;
}

int cat(const char *destFilePath){
    int fd;
    int ret;
    uint8_t buffer[MAX_BUFFER_SIZE];
    printf("cat %s\n",destFilePath);
    fd = open(destFilePath,O_READ);
    if(fd == -1){
        printf("cat failed\n");
        return -1;
    }
    while(1){
        ret = read(fd,buffer,MAX_BUFFER_SIZE/2);
        if(ret == -1){
            printf("cat faild\n");
            return -1;
        }
        if(ret == 0){
            break;
        }
        buffer[ret] = 0;
        printf("%s",buffer);
    }
    close(fd);
    printf("\ncat success\n");
    return 0;
}
	