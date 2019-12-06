// addrspace.h
//	Data structures to keep track of executing user programs
//	(address spaces).
//
//	For now, we don't keep any information about address spaces.
//	The user level CPU state is saved and restored in the thread
//	executing the user program (see thread.h).
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#ifndef ADDRSPACE_H
#define ADDRSPACE_H

#include "copyright.h"
#include "filesys.h"
#include "bitmap.h"

#define UserStackSize 1024 // 用户栈大小
#define MaxPages 32        // 可装入内存最大页数
#define MinPages 16        // 可装入内存最小页数
#define CodePages 8        // 代码区最大页数
#define DataPages 24       // 数据区最大页数

class AddrSpace
{
public:
  AddrSpace(OpenFile *executable); // Create an address space,
                                   // initializing it with the program stored in the file "executable"
  ~AddrSpace();                    // De-allocate an address space

  void InitRegisters(); // Initialize user-level CPU registers, before jumping to user code

  void SaveState();    // Save/restore address space-specific
  void RestoreState(); // info on a context switch

  void Print();

  int getSpaceID() { return spaceID; }

  unsigned int count; // 计数器
  char *vmName;       // 交换文件文件名

  TranslationEntry *pageTable;

private:
  unsigned int numPages; // Number of pages in the virtual address space
  int spaceID;           // Current Space ID
};

#endif // ADDRSPACE_H
