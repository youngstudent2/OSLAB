#include "x86.h"
#include "device.h"
#include "fs.h"

int readSuperBlock(SuperBlock *superBlock) {
    diskRead((void*)superBlock, sizeof(SuperBlock), 1, 0);
    return 0;
}

int readBlock (SuperBlock *superBlock, Inode *inode, int blockIndex, uint8_t *buffer) {
    int divider0 = superBlock->blockSize / 4;
    int bound0 = POINTER_NUM;
    int bound1 = bound0 + divider0;

    uint32_t singlyPointerBuffer[divider0];
    
    if (blockIndex < bound0) {
        diskRead((void*)buffer, sizeof(uint8_t), superBlock->blockSize, inode->pointer[blockIndex] * SECTOR_SIZE);
        return 0;
    }
    else if (blockIndex < bound1) {
        diskRead((void*)singlyPointerBuffer, sizeof(uint8_t), superBlock->blockSize, inode->singlyPointer * SECTOR_SIZE);
        diskRead((void*)buffer, sizeof(uint8_t), superBlock->blockSize, singlyPointerBuffer[blockIndex - bound0] * SECTOR_SIZE);
        return 0;
    }
    else {
        return -1;
    }
}

int readInode(SuperBlock *superBlock, Inode *destInode, int *inodeOffset, const char *destFilePath) {
    int i = 0;
    int j = 0;
    int ret = 0;
    int cond = 0;
    *inodeOffset = 0;
    uint8_t buffer[superBlock->blockSize];
    DirEntry *dirEntry = NULL;
    int count = 0;
    int size = 0;
    int blockCount = 0;

    if (destFilePath == NULL || destFilePath[count] == 0) {
        return -1;
    }
    ret = stringChr(destFilePath, '/', &size);
    if (ret == -1 || size != 0) {
        return -1;
    }
    count += 1;
    *inodeOffset = superBlock->inodeTable * SECTOR_SIZE;
    diskRead((void *)destInode, sizeof(Inode), 1, *inodeOffset);

    while (destFilePath[count] != 0) {
        ret = stringChr(destFilePath + count, '/', &size);
        if(ret == 0 && size == 0) {
            return -1;
        }
        if (ret == -1) {
            cond = 1;
        }
        else if (destInode->type == REGULAR_TYPE) {
            return -1;
        }
        blockCount = destInode->blockCount;
        for (i = 0; i < blockCount; i ++) {
            ret = readBlock(superBlock, destInode, i, buffer);
            if (ret == -1) {
                return -1;
            }
            dirEntry = (DirEntry *)buffer;
            for (j = 0; j < superBlock->blockSize / sizeof(DirEntry); j ++) {
                if (dirEntry[j].inode == 0) {
                    continue;
                }
                else if (stringCmp(dirEntry[j].name, destFilePath + count, size) == 0) {
                    *inodeOffset = superBlock->inodeTable * SECTOR_SIZE + (dirEntry[j].inode - 1) * sizeof(Inode);
                    diskRead((void *)destInode, sizeof(Inode), 1, *inodeOffset);
                    break;
                }
            }
            if (j < superBlock->blockSize / sizeof(DirEntry)) {
                break;
            }
        }
        if (i < blockCount) {
            if (cond == 0) {
                count += (size + 1);
            }
            else {
                return 0;
            }
        }
        else {
            return -1;
        }
    }
    return 0;
}
/*
 *  Find a Block unused, this function will update SuperBlock and BlockBitmap and write back.
 *  Input: file, superblock.
 *  Output: blockOffset (sector as unit)
 *  Return -1 when failed.
 */
int getAvailBlock (SuperBlock *superBlock, int *blockOffset) {
    int j = 0;
    int k = 0;
    int blockBitmapOffset = 0;
    BlockBitmap blockBitmap;

    if (superBlock->availBlockNum == 0)
        return -1;
    superBlock->availBlockNum--;

    blockBitmapOffset = superBlock->blockBitmap;
    diskRead((void*)&blockBitmap,sizeof(BlockBitmap),1,blockBitmapOffset*SECTOR_SIZE);
    for (j = 0; j < superBlock->blockNum / 8; j++) {
        if (blockBitmap.byte[j] != 0xff) {
            break;
        }
    }
    for (k = 0; k < 8; k++) {
        if ((blockBitmap.byte[j] >> (7 - k)) % 2 == 0) {
            break;
        }
    }
    blockBitmap.byte[j] = blockBitmap.byte[j] | (1 << (7 - k));
    
    *blockOffset = superBlock->blocks + ((j * 8 + k) * superBlock->blockSize / SECTOR_SIZE);

    diskWrite((void*)superBlock,sizeof(SuperBlock),1,0);
    diskWrite((void*)&blockBitmap,sizeof(BlockBitmap),1,sizeof(SuperBlock));
    return 0;
}

/*
 *  Find a Inode unused, update superBlock InodeBitmap and write back.
 *  Input: file, superblock.
 *  Output: inodeOffset (byte as unit).
 *  Return -1 when failed.
 */
int getAvailInode (SuperBlock *superBlock, int *inodeOffset) {
    int j = 0;
    int k = 0;
    int inodeBitmapOffset = 0;
    int inodeTableOffset = 0;
    InodeBitmap inodeBitmap;

    if (superBlock->availInodeNum == 0)
        return -1;
    superBlock->availInodeNum--;

    inodeBitmapOffset = superBlock->inodeBitmap;
    inodeTableOffset = superBlock->inodeTable;
    diskRead((void*)&inodeBitmap,sizeof(InodeBitmap),1,inodeBitmapOffset*SECTOR_SIZE);
    for (j = 0; j < superBlock->availInodeNum / 8; j++) {
        if (inodeBitmap.byte[j] != 0xff) {
            break;
        }
    }
    for (k = 0; k < 8; k++) {
        if ((inodeBitmap.byte[j] >> (7-k)) % 2 == 0) {
            break;
        }
    }
    inodeBitmap.byte[j] = inodeBitmap.byte[j] | (1 << (7 - k));

    *inodeOffset = inodeTableOffset * SECTOR_SIZE + (j * 8 + k) * sizeof(Inode);

    diskWrite((void*)superBlock,sizeof(SuperBlock),1,0);
    diskWrite((void*)&inodeBitmap,sizeof(InodeBitmap),1,inodeBitmapOffset*SECTOR_SIZE);
    return 0;
}
/*
 *  Write buffer to the blockIndex-th block of inode.
 *  Return -1 when failed.
 */
int writeBlock (SuperBlock *superBlock, Inode *inode, int blockIndex, uint8_t *buffer) {
    int divider0 = superBlock->blockSize / 4;
    int bound0 = POINTER_NUM;
    int bound1 = bound0 + divider0;

    uint32_t singlyPointerBuffer[divider0];

    if (blockIndex < bound0) {
        diskWrite((void*)buffer,sizeof(uint8_t),superBlock->blockSize,
                    inode->pointer[blockIndex]*SECTOR_SIZE);
        return 0;
    }
    else if (blockIndex < bound1) {
        diskRead((void*)singlyPointerBuffer,sizeof(uint8_t),superBlock->blockSize,
                    inode->singlyPointer*SECTOR_SIZE);
        diskWrite((void*)buffer,sizeof(uint8_t),superBlock->blockSize,
                    singlyPointerBuffer[blockIndex - bound0] * SECTOR_SIZE);
        return 0;
    }
    else
        return -1;
}
/*
 *  Alloc a new Block for inode, inode.blockCount++ and write back inode.
 *  Input: file, superBlock, inode, inodeOffset(byte as unit), blockOffset (sector as unit).
 *  Output: inode
 *  Return -1 when failed.
 */
int allocLastBlock (SuperBlock *superBlock, Inode *inode, int inodeOffset, int blockOffset) {
    int divider0 = superBlock->blockSize / 4;
    int bound0 = POINTER_NUM;
    int bound1 = bound0 + divider0;

    uint32_t singlyPointerBuffer[divider0];
    int singlyPointerBufferOffset = 0;

    if (inode->blockCount < bound0) {
        inode->pointer[inode->blockCount] = blockOffset;
    }
    else if (inode->blockCount == bound0) {
        getAvailBlock(superBlock, &singlyPointerBufferOffset);
        singlyPointerBuffer[0] = blockOffset;
        inode->singlyPointer = singlyPointerBufferOffset;
        diskWrite((void*)singlyPointerBuffer,sizeof(uint8_t),superBlock->blockSize,
                    singlyPointerBufferOffset*SECTOR_SIZE);
    }
    else if (inode->blockCount < bound1) {
        diskRead((void*)singlyPointerBuffer,sizeof(uint8_t),superBlock->blockSize,
                    inode->singlyPointer*SECTOR_SIZE);
        singlyPointerBuffer[inode->blockCount - bound0] = blockOffset;
        diskWrite((void*)singlyPointerBuffer,sizeof(uint8_t),superBlock->blockSize,
                    inode->singlyPointer*SECTOR_SIZE);
    }
    else
        return -1;
    
    inode->blockCount++;
    diskWrite((void*)inode,sizeof(Inode),1,inodeOffset);
    return 0;
}
/*
 * return the number of pointerBlock should be allocated in addition if the blockCount-th block is added
 */
int calNeededPointerBlocks (SuperBlock *superBlock, int blockCount) {
    int divider0 = superBlock->blockSize / 4;
    int bound0 = POINTER_NUM;
    int bound1 = bound0 + divider0;

    if (blockCount == bound0)
        return 1;
    else if (blockCount >= bound1)
        return -1;
    else
        return 0;
}
/*
 *  Alloc a new block for inode by call getAvailBlock and allocLastBlock.
 *  Input: file, superBlock, inode, inodeOffset (byte as unit).
 *  Output: inode.
 *  Return -1 when failed.
 */
int allocBlock (SuperBlock *superBlock, Inode *inode, int inodeOffset) {
    int ret = 0;
    int blockOffset = 0;

    ret = calNeededPointerBlocks(superBlock, inode->blockCount);
    if (ret == -1)
        return -1;
    if (superBlock->availBlockNum < ret + 1)
        return -1;
    
    getAvailBlock(superBlock, &blockOffset);
    allocLastBlock(superBlock, inode, inodeOffset, blockOffset);
    return 0;
}

/*
 *  Read fatherInode blocks and find a empty DirEntry.
 *  Find a empty Inode and fill DirEntry.name and DirEntry.inode.
 *  New DirEntry should contrain '.' and '..'.
 */
int allocInode (SuperBlock *superBlock, Inode *fatherInode, int fatherInodeOffset,
        Inode *destInode, int *destInodeOffset, const char *destFilename, int destFiletype) {
    int i = 0;
    int j = 0;
    int ret = 0;
    DirEntry *dirEntry = NULL;
    uint8_t buffer[superBlock->blockSize];
    int length = stringLen(destFilename);

    if (destFilename == NULL || destFilename[0] == 0)
        return -1;

    if (superBlock->availInodeNum == 0)
        return -1;
    
    for (i = 0; i < fatherInode->blockCount; i++) {
        ret = readBlock(superBlock, fatherInode, i, buffer);
        if (ret == -1)
            return -1;
        dirEntry = (DirEntry *)buffer;
        for (j = 0; j < superBlock->blockSize / sizeof(DirEntry); j++) {
            if (dirEntry[j].inode == 0) // a valid empty dirEntry
                break;
            else if (stringCmp(dirEntry[j].name, destFilename, length) == 0)
                return -1; // file with filename = destFilename exist
        }
        if (j < superBlock->blockSize / sizeof(DirEntry))
            break;
    }
    if (i == fatherInode->blockCount) {
        ret = allocBlock(superBlock, fatherInode, fatherInodeOffset);
        if (ret == -1)
            return -1;
        fatherInode->size = fatherInode->blockCount * superBlock->blockSize;
        setBuffer(buffer, superBlock->blockSize, 0);
        dirEntry = (DirEntry *)buffer;
        j = 0;
    }
    // dirEntry[j] is the valid empty dirEntry, it is in the i-th block of fatherInode.
    ret = getAvailInode(superBlock, destInodeOffset);
    if (ret == -1)
        return -1;

    stringCpy(destFilename, dirEntry[j].name, NAME_LENGTH);
    dirEntry[j].inode = (*destInodeOffset - superBlock->inodeTable * SECTOR_SIZE) / sizeof(Inode) + 1;
    ret = writeBlock(superBlock, fatherInode, i, buffer);
    if (ret == -1)
        return -1;
    
    diskWrite((void*)fatherInode,sizeof(Inode),1,fatherInodeOffset);

    destInode->type = destFiletype;
    destInode->linkCount = 1;
    destInode->blockCount = 0;
    destInode->size = 0;

    diskWrite((void*)destInode,sizeof(Inode),1,*destInodeOffset);

    return 0;
}

int setAllocInode (SuperBlock *superBlock, int inodeOffset) {
    int j = 0;
    int k = 0;
    int inodeBitmapOffset = 0;
    InodeBitmap inodeBitmap;

    j = (inodeOffset-superBlock->inodeTable*SECTOR_SIZE)/sizeof(Inode)/8;
    k = (inodeOffset-superBlock->inodeTable*SECTOR_SIZE)/sizeof(Inode)%8;

    inodeBitmapOffset = superBlock->inodeBitmap;
    diskRead((void*)&inodeBitmap,sizeof(InodeBitmap),1,inodeBitmapOffset*SECTOR_SIZE);
    if((inodeBitmap.byte[j]>>(7-k))%2==0)
        return -1;
    
    superBlock->availBlockNum++;
    inodeBitmap.byte[j] = inodeBitmap.byte[j] ^ (1<<(7-k));

    diskWrite((void*)superBlock,sizeof(SuperBlock),1,0);
    diskWrite((void*)&inodeBitmap,sizeof(InodeBitmap),1,inodeBitmapOffset*SECTOR_SIZE);
    return 0;
}

int setAllocBlock (SuperBlock *superBlock, int blockOffset) {
    int j = 0;
    int k = 0;
    int blockBitmapOffset = 0;
    BlockBitmap blockBitmap;

    j = (blockOffset - superBlock->blocks)/(superBlock->blockSize/SECTOR_SIZE)/8;
    k = (blockOffset - superBlock->blocks)/(superBlock->blockSize/SECTOR_SIZE)%8;

    blockBitmapOffset = superBlock->blockBitmap;

    diskRead((void*)&blockBitmap,sizeof(BlockBitmap),1,blockBitmapOffset*SECTOR_SIZE);
    if((blockBitmap.byte[j] >> (7-k)) % 2 == 0)
        return -1;
    
    superBlock->availBlockNum++;
    blockBitmap.byte[j] = blockBitmap.byte[j] ^ (1<<(7-k));

    diskWrite((void*)superBlock,sizeof(SuperBlock),1,0);
    diskWrite((void*)&blockBitmap,sizeof(BlockBitmap),1,blockBitmapOffset*SECTOR_SIZE);  
    return 0;
}

int freeLastBlock (SuperBlock *superBlock, Inode *inode, int inodeOffset) {
    int divider0 = superBlock->blockSize / 4;
    int bound0 = POINTER_NUM;
    int bound1 = bound0 + divider0;

    uint32_t singlyPointerBuffer[divider0];
    inode->blockCount--;

    if (inode->blockCount < bound0) {
        setAllocBlock(superBlock,inode->pointer[inode->blockCount]);
    }
    else if (inode->blockCount == bound0) {

        diskRead((void*)singlyPointerBuffer,sizeof(uint8_t),superBlock->blockSize,inode->singlyPointer*SECTOR_SIZE);
        setAllocBlock(superBlock,singlyPointerBuffer[0]);
        setAllocBlock(superBlock,inode->singlyPointer);
    }
    else if (inode->blockCount < bound1) {
        diskRead((void*)singlyPointerBuffer,sizeof(uint8_t),superBlock->blockSize,inode->singlyPointer*SECTOR_SIZE);
        setAllocBlock(superBlock,singlyPointerBuffer[inode->blockCount - bound0]);
    }
    else
        return -1;
    
    diskWrite((void*)inode,sizeof(Inode),1,inodeOffset);
    return 0;
}

int freeBlock (SuperBlock *superBlock, Inode *inode, int inodeOffset) {
    int ret = 0;
    while(inode->blockCount>=0){
        ret = freeLastBlock(superBlock,inode,inodeOffset);
        if(ret == -1){
            return -1;
        }
    }
    return 0;
}

int getDirEntry (SuperBlock *superBlock, Inode *inode, int dirIndex, DirEntry *destDirEntry) {
    int i = 0;
    int j = 0;
    int ret = 0;
    int dirCount = 0;
    DirEntry *dirEntry = NULL;
    uint8_t buffer[superBlock->blockSize];

    for (i = 0; i < inode->blockCount; i++) {
        ret = readBlock(superBlock, inode, i, buffer);
        if (ret == -1)
            return -1;
        dirEntry = (DirEntry *)buffer;
        for (j = 0; j < superBlock->blockSize / sizeof(DirEntry); j ++) {
            if (dirEntry[j].inode != 0) {
                if (dirCount == dirIndex)
                    break;
                else
                    dirCount ++;
            }
        }
        if (j < superBlock->blockSize / sizeof(DirEntry))
            break;
    }
    if (i == inode->blockCount)
        return -1;
    else {
        destDirEntry->inode = dirEntry[j].inode;
        stringCpy(dirEntry[j].name, destDirEntry->name, NAME_LENGTH);
        return 0;
    }
} 

int freeInode (SuperBlock *superBlock, Inode *fatherInode, int fatherInodeOffset,
        Inode *destInode, int *destInodeOffset, const char *destFilename, int destFiletype) {
    int i = 0;
    int j = 0;
    int ret = 0;
    DirEntry *dirEntry = NULL;
    uint8_t buffer[superBlock->blockSize];
    int length = stringLen(destFilename);

    if (destFilename == NULL || destFilename[0] == 0)
        return -1;
    for (i = 0; i < fatherInode->blockCount; i++) {
        ret = readBlock(superBlock, fatherInode, i, buffer);
        if (ret == -1)
            return -1;
        dirEntry = (DirEntry *)buffer;
        for (j = 0; j < superBlock->blockSize / sizeof(DirEntry); j++) {
            if (dirEntry[j].inode == 0) // a valid empty dirEntry
                continue;
            else if (stringCmp(dirEntry[j].name, destFilename, length) == 0)
                break;
        }
        if (j < superBlock->blockSize / sizeof(DirEntry))
            break;
    }
    if (i == fatherInode->blockCount) {
        return -1;
    }

    // free destInode
    *destInodeOffset = superBlock->inodeTable * SECTOR_SIZE+(dirEntry[j].inode - 1)*sizeof(Inode);
    diskRead((void*)destInode,sizeof(Inode),1,*destInodeOffset);
    if(destInode->type != destFiletype)
        return -1;

    DirEntry tmpDirEntry;
    if(destFiletype == DIRECTORY_TYPE){
        ret = getDirEntry(superBlock,destInode,0,&tmpDirEntry);
        if(ret != 0){ // directory is not empty , fail
            return -1;
        }
    }

    --destInode->linkCount;
    if(destInode->linkCount == 0){
        freeBlock(superBlock,destInode,*destInodeOffset);
        setAllocInode(superBlock,*destInodeOffset);
    }
    else{
        diskWrite((void*)destInode,sizeof(Inode),1,*destInodeOffset);
    }
    dirEntry[j].inode = 0;
    ret = writeBlock(superBlock,fatherInode,i,buffer);
    if(ret == -1){
        return -1;
    }

    return 0;

} 