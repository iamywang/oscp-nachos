// openfile.cc
//	Routines to manage an open Nachos file.  As in UNIX, a
//	file must be open before we can read or write to it.
//	Once we're all done, we can close it (in Nachos, by deleting
//	the OpenFile data structure).
//
//	Also as in UNIX, for convenience, we keep the file header in
//	memory while the file is open.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "filehdr.h"
#include "openfile.h"
#include "system.h"

//----------------------------------------------------------------------
// OpenFile::OpenFile
// 	Open a Nachos file for reading and writing.  Bring the file header
//	into memory while the file is open.
//
//	"sector" -- the location on disk of the file header for this file
//----------------------------------------------------------------------

OpenFile::OpenFile(int sector)
{
    hdr = new FileHeader;
    hdr->FetchFrom(sector);
    seekPosition = 0;
    headSector = sector; // 文件头部头部扇区
}

//----------------------------------------------------------------------
// OpenFile::~OpenFile
// 	Close a Nachos file, de-allocating any in-memory data structures.
//----------------------------------------------------------------------

OpenFile::~OpenFile()
{
    delete hdr;
}

//----------------------------------------------------------------------
// OpenFile::Seek
// 	Change the current location within the open file -- the point at
//	which the next Read or Write will start from.
//
//	"position" -- the location within the file for the next Read/Write
//----------------------------------------------------------------------

void OpenFile::Seek(int position)
{
    seekPosition = position;
}

//----------------------------------------------------------------------
// OpenFile::Read/Write
// 	Read/write a portion of a file, starting from seekPosition.
//	Return the number of bytes actually written or read, and as a
//	side effect, increment the current position within the file.
//
//	Implemented using the more primitive ReadAt/WriteAt.
//
//	"into" -- the buffer to contain the data to be read from disk
//	"from" -- the buffer containing the data to be written to disk
//	"numBytes" -- the number of bytes to transfer
//----------------------------------------------------------------------

int OpenFile::Read(char *into, int numBytes)
{
    int result = ReadAt(into, numBytes, seekPosition);
    seekPosition += result;
    return result;
}

int OpenFile::Write(char *into, int numBytes)
{
    int result = WriteAt(into, numBytes, seekPosition);
    seekPosition += result;
    return result;
}

//----------------------------------------------------------------------
// OpenFile::ReadAt/WriteAt
// 	Read/write a portion of a file, starting at "position".
//	Return the number of bytes actually written or read, but has
//	no side effects (except that Write modifies the file, of course).
//
//	There is no guarantee the request starts or ends on an even disk sector
//	boundary; however the disk only knows how to read/write a whole disk
//	sector at a time.  Thus:
//
//	For ReadAt:
//	   We read in all of the full or partial sectors that are part of the
//	   request, but we only copy the part we are interested in.
//	For WriteAt:
//	   We must first read in any sectors that will be partially written,
//	   so that we don't overwrite the unmodified portion.  We then copy
//	   in the data that will be modified, and write back all the full
//	   or partial sectors that are part of the request.
//
//	"into" -- the buffer to contain the data to be read from disk
//	"from" -- the buffer containing the data to be written to disk
//	"numBytes" -- the number of bytes to transfer
//	"position" -- the offset within the file of the first byte to be
//			read/written
//----------------------------------------------------------------------

int OpenFile::ReadAt(char *into, int numBytes, int position)
{
    int fileLength = hdr->FileLength();
    int i, firstSector, lastSector, numSectors;
    char *buf;

    if ((numBytes <= 0) || (position >= fileLength))
        return 0; // check request
    if ((position + numBytes) > fileLength)
        numBytes = fileLength - position;
    DEBUG('f', "Reading %d bytes at %d, from file of length %d.\n",
          numBytes, position, fileLength);

    firstSector = divRoundDown(position, SectorSize);
    lastSector = divRoundDown(position + numBytes - 1, SectorSize);
    numSectors = 1 + lastSector - firstSector;

    // read in all the full and partial sectors that we need
    buf = new char[numSectors * SectorSize];
    for (i = firstSector; i <= lastSector; i++)
        synchDisk->ReadSector(hdr->ByteToSector(i * SectorSize),
                              &buf[(i - firstSector) * SectorSize]);

    // copy the part we want
    bcopy(&buf[position - (firstSector * SectorSize)], into, numBytes);
    delete[] buf;
    return numBytes;
}

int OpenFile::WriteAt(char *from, int numBytes, int position)
{
    int fileLength = hdr->FileLength();
    int i, firstSector, lastSector, numSectors;
    bool firstAligned, lastAligned;
    char *buf;
    //    printf("seekPosition is %d,fileLength is %d\n",seekPosition,fileLength);
    if (numBytes < 0)
        return 0;                   // check request
    if (seekPosition >= fileLength) //如果文件指针已经超过了文件大小
    {
        numSectors = divRoundUp(fileLength, SectorSize); //计算当前需要的扇区
        int numPos = seekPosition + numBytes;            //计算添加新字节之后的指针位置
        if (numPos > numSectors * SectorSize)            //如果文件为空或者超过了已有扇区空间
        {
            AllocateSpace(numPos - numSectors * SectorSize); //申请新空间
            fileLength += numBytes;                          //增大文件空间
        }
        hdr->setLength(numPos); //根据新指针位置设置新的文件大小
    }
    //写入的部分已经考虑了从中间写入的情况

    if (fileLength == 0)
        return 0;
    DEBUG('f', "Writing %d bytes at %d, from file of length %d.\n",
          numBytes, position, fileLength);

    firstSector = divRoundDown(position, SectorSize);
    lastSector = divRoundDown(position + numBytes - 1, SectorSize);
    numSectors = 1 + lastSector - firstSector;
    //    printf("firstSector is %d,lastSector is %d\n",firstSector,lastSector);
    buf = new char[numSectors * SectorSize];

    firstAligned = (bool)(position == (firstSector * SectorSize));
    lastAligned = (bool)((position + numBytes) == ((lastSector + 1) * SectorSize));

    // read in first and last sector, if they are to be partially modified
    if (!firstAligned)
        ReadAt(buf, SectorSize, firstSector * SectorSize);
    if (!lastAligned && ((firstSector != lastSector) || firstAligned))
        ReadAt(&buf[(lastSector - firstSector) * SectorSize],
               SectorSize, lastSector * SectorSize);

    // copy in the bytes we want to change
    bcopy(from, &buf[position - (firstSector * SectorSize)], numBytes);

    // write modified sectors back
    for (i = firstSector; i <= lastSector; i++)
        synchDisk->WriteSector(hdr->ByteToSector(i * SectorSize),
                               &buf[(i - firstSector) * SectorSize]);
    delete[] buf;
    return numBytes;
}

//----------------------------------------------------------------------
// OpenFile::Length
// 	Return the number of bytes in the file.
//----------------------------------------------------------------------

int OpenFile::Length()
{
    return hdr->FileLength();
}

//----------------------------------------------------------------------
// OpenFile::AllocateSpace
// 	AllocateSpace
//----------------------------------------------------------------------

void OpenFile::AllocateSpace(int size)
{
    BitMap *freeMap;
    freeMap = new BitMap(NumSectors); //新建一个BitMap对象
    OpenFile *freeMapFile;
    freeMapFile = new OpenFile(0);   //新建一个比特图对应的OpenFile对象
    freeMap->FetchFrom(freeMapFile); //从磁盘中取出比特图的信息

    hdr->extendFile(freeMap, size);  //实际的扩展操作
    freeMap->WriteBack(freeMapFile); //写回比特图的信息
    delete freeMap;
}

//----------------------------------------------------------------------
// OpenFile::WriteBack
// 	WriteBack to the file.
//----------------------------------------------------------------------

void OpenFile::WriteBack()
{
    hdr->WriteBack(headSector);
}