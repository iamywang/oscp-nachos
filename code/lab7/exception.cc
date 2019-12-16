// exception.cc
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include <stdlib.h>
#include <time.h>

#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "addrspace.h"

Thread *thread;
AddrSpace *space;

//----------------------------------------------------------------------
// StartProcess
//----------------------------------------------------------------------

void StartProcess(int n)
{
    currentThread->space = space;
    currentThread->space->InitRegisters();
    currentThread->space->RestoreState();

    machine->Run();
    ASSERT(false);
}

//----------------------------------------------------------------------
// AdvancePC
//----------------------------------------------------------------------

void AdvancePC()
{
    machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
    machine->WriteRegister(PCReg, machine->ReadRegister(PCReg) + 4);
    machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg) + 4);
}

//----------------------------------------------------------------------
// PageReplace
// 页面置换算法
//----------------------------------------------------------------------
enum PageReplaceType
{
    RANDOM,
    FIFO,           // 先入先出
    LIFO,           // 后入先出
    LRU,            // 最近最少使用
    CLOCK,          // 时钟
    ENHANCED_CLOCK, // 二次机会
};

unsigned int PageReplace(PageReplaceType type)
{
    unsigned int readySwap;
    extern unsigned int vpTable[MaxPages];

    if (type == RANDOM)
    {
        srand((unsigned)time(NULL));
        readySwap = rand() % currentThread->space->numPages;
        while (!currentThread->space->pageTable[readySwap].valid)
            readySwap = rand() % currentThread->space->numPages;
    }
    else if (type == FIFO)
    {
        int count = 0;
        readySwap = vpTable[0];
        // 找到第一个不为-1的页面
        while (vpTable[count] != -1)
            count++;
        readySwap = vpTable[count - 1];
        // 移动页面
        for (int i = 0; i < count; i++)
            vpTable[i] = vpTable[i + 1];
        vpTable[count - 1] = -1;
    }
    else if (type == LIFO)
    {
        int count = 0;
        while (vpTable[count] != -1)
            count++;
        readySwap = vpTable[count - 1];
        vpTable[count - 1] = -1;
    }
    else if (type == LRU)
    {
    }
    else if (type == CLOCK)
    {
        int count = 0;
        readySwap = vpTable[0];
        // 找到第一个不为-1的页面，访问位是0
        for (count = 0; count < MaxPages; count++)
        {
            if (vpTable[count] != -1)
                if (currentThread->space->pageTable[readySwap].use == TRUE)
                    currentThread->space->pageTable[readySwap].use == FALSE;
                else
                {
                    readySwap = vpTable[count - 1];
                    break;
                }
        }
        // 移动页面
        for (int i = 0; i < count; i++)
            vpTable[i] = vpTable[i + 1];
        vpTable[count - 1] = -1;
    }
    else if (type == ENHANCED_CLOCK)
    {
        int count = 0;
        readySwap = vpTable[0];
        // 找到第一个不为-1的页面，访问位 dirty位都是0
        for (count = 0; count < MaxPages; count++)
        {
            if (vpTable[count] != -1)
                if (currentThread->space->pageTable[readySwap].use == TRUE)
                    currentThread->space->pageTable[readySwap].use == FALSE;
                else if (currentThread->space->pageTable[readySwap].dirty == FALSE)
                {
                    readySwap = vpTable[count - 1];
                    break;
                }
        }
        // 移动页面
        for (int i = 0; i < count; i++)
            vpTable[i] = vpTable[i + 1];
        vpTable[count - 1] = -1;
    }
    return readySwap;
}

//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2.
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions
//	are in machine.h.
//----------------------------------------------------------------------

void ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);
    extern BitMap *bitmap; // 比特图

    // 系统调用异常
    if (which == SyscallException)
    {
        switch (type)
        {
        case SC_Halt:
        {
            DEBUG('a', "Shutdown, initiated by user program.\n");
            interrupt->Halt();
            break;
        }
        case SC_Exec:
        {
            printf("Execute system call of Exec()\n");
            //read argument
            char filename[50];
            int addr = machine->ReadRegister(4);
            int i = 0;
            do
            {
                machine->ReadMem(addr + i, 1, (int *)&filename[i]);
            } while (filename[i++] != '\0');

            printf("Exec(%s):\n", filename);

            // open file
            OpenFile *executable = fileSystem->Open(filename);

            if (executable == NULL)
            {
                printf("Unable to open file %s\n", filename);
                return;
            }

            // new address space
            space = new AddrSpace(executable);
            delete executable; // close file

            // new and fork thread
            thread = new Thread("forked thread");
            thread->Fork(StartProcess, 1);

            // run the new thread
            currentThread->Yield();

            // return spaceID
            machine->WriteRegister(2, space->getSpaceID());

            // advance PC
            AdvancePC();
            break;
        }
        case SC_Exit:
        {
            printf("Execute system call of Exit()\n");

            AdvancePC();
            currentThread->Finish();
            break;
        }
        default:
        {
            printf("Unexpected syscall %d %d\n", which, type);
            ASSERT(FALSE);
            break;
        }
        }
    }

    // 页错误异常
    else if (which == PageFaultException)
    {
        AddrSpace *pageSpace = currentThread->space;              // 地址空间
        OpenFile *swapFile = fileSystem->Open(pageSpace->vmName); // 交换文件

        unsigned int pageFaultAddress;         // 页错误地址
        unsigned int page;                     // 需要加载的页号
        extern unsigned int vpTable[MaxPages]; // 存放虚拟页号的数组

        pageFaultAddress = machine->registers[BadVAddrReg];
        page = pageFaultAddress / PageSize;

        if (bitmap->NumClear() >= 1 && pageSpace->count < MaxPages)
        {
            // 从交换文件换入物理内存
            pageSpace->count++;
            pageSpace->pageTable[page].physicalPage = bitmap->Find();
            pageSpace->pageTable[page].valid = TRUE;
            pageSpace->pageTable[page].use = TRUE;
            pageSpace->pageTable[page].dirty = FALSE;
            swapFile->ReadAt(&(machine->mainMemory[pageSpace->pageTable[page].physicalPage * PageSize]),
                             PageSize, pageSpace->pageTable[page].virtualPage * PageSize);
        }
        // 需要使用页面置换算法
        else
        {
            unsigned int readySwap = PageReplace(FIFO);
            // 是否被修改
            if (pageSpace->pageTable[readySwap].dirty == TRUE)
            {
                // 写回交换文件
                swapFile->WriteAt(&(machine->mainMemory[pageSpace->pageTable[readySwap].physicalPage * PageSize]),
                                  PageSize, pageSpace->pageTable[page].virtualPage * PageSize);
                pageSpace->pageTable[readySwap].dirty = FALSE;
            }
            // 释放页
            pageSpace->pageTable[readySwap].valid = FALSE;
            for (int j = 0; j < MaxPages; j++)
                if (vpTable[j] == readySwap)
                {
                    vpTable[j] = -1;
                    printf("Page Fault Handler: Successfully Release Page # %d.\n", readySwap);
                    break;
                }
            // 换入页
            pageSpace->pageTable[page].physicalPage = pageSpace->pageTable[readySwap].physicalPage;
            pageSpace->pageTable[page].valid = FALSE;
            pageSpace->pageTable[page].use = TRUE;
            pageSpace->pageTable[page].dirty = FALSE;
            swapFile->ReadAt(&(machine->mainMemory[pageSpace->pageTable[page].physicalPage * PageSize]),
                             PageSize, pageSpace->pageTable[page].virtualPage * PageSize);
        }
        pageSpace->pageTable[page].valid = TRUE;
        unsigned int vpn = pageSpace->pageTable[page].virtualPage;

        // 加载页
        for (int j = 0; j < MaxPages; j++)
            if (vpTable[j] == -1)
            {
                vpTable[j] = vpn;
                printf("Page Fault Handler: Successfully Load Page # %d.\n", vpn);
                pageSpace->Print();
                break;
            }
    }

    // 未定义异常
    else
    {
        printf("Unexpected user mode exception %d %d\n", which, type);
        ASSERT(FALSE);
    }
}