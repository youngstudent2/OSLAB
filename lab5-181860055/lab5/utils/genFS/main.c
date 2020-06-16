#include <stdio.h>
#include "utils.h"
#include "data.h"
#include "func.h"

int main(int argc, char *argv[]) {
    char driver[NAME_LENGTH];
    char srcFilePath[NAME_LENGTH];
    char destFilePath[NAME_LENGTH];

    stringCpy("fs.bin", driver, NAME_LENGTH - 1);
    format(driver, SECTOR_NUM, SECTORS_PER_BLOCK);

    stringCpy("/boot", destFilePath, NAME_LENGTH - 1);
    mkdir(driver, destFilePath);

    stringCpy(argv[1], srcFilePath, NAME_LENGTH - 1);
    stringCpy("/boot/initrd", destFilePath, NAME_LENGTH - 1);
    cp(driver, srcFilePath, destFilePath);

    stringCpy("/usr", destFilePath, NAME_LENGTH - 1);
    mkdir(driver, destFilePath);

    stringCpy(argv[2], srcFilePath, NAME_LENGTH - 1);
    stringCpy("/usr/print", destFilePath, NAME_LENGTH - 1);
    cp(driver, srcFilePath, destFilePath);

    stringCpy(argv[3], srcFilePath, NAME_LENGTH - 1);
    stringCpy("/usr/bounded_buffer", destFilePath, NAME_LENGTH - 1);
    cp(driver, srcFilePath, destFilePath);

    stringCpy(argv[4], srcFilePath, NAME_LENGTH - 1);
    stringCpy("/usr/philosopher", destFilePath, NAME_LENGTH - 1);
    cp(driver, srcFilePath, destFilePath);

    stringCpy(argv[5], srcFilePath, NAME_LENGTH - 1);
    stringCpy("/usr/reader_writer", destFilePath, NAME_LENGTH - 1);
    cp(driver, srcFilePath, destFilePath);


    /* for test rmdir */
    stringCpy("/tmp", destFilePath, NAME_LENGTH - 1);
    mkdir(driver, destFilePath);

    /* for test rm */
    stringCpy(argv[5], srcFilePath, NAME_LENGTH - 1);
    stringCpy("/tmp/reader_writer", destFilePath, NAME_LENGTH - 1);
    cp(driver, srcFilePath, destFilePath);

    stringCpy("/tmp", destFilePath, NAME_LENGTH - 1);
    ls(driver, destFilePath);

    stringCpy("/tmp/reader_writer", destFilePath, NAME_LENGTH - 1);
    rm(driver, destFilePath);

    stringCpy("/tmp", destFilePath, NAME_LENGTH - 1);
    ls(driver, destFilePath);

    stringCpy("/tmp", destFilePath, NAME_LENGTH - 1);
    rmdir(driver, destFilePath);

    /****/

    stringCpy("/", destFilePath, NAME_LENGTH - 1);
    ls(driver, destFilePath);

    stringCpy("/boot", destFilePath, NAME_LENGTH - 1);
    ls(driver, destFilePath);

    stringCpy("/usr", destFilePath, NAME_LENGTH - 1);
    ls(driver, destFilePath);

    return 0;
}
