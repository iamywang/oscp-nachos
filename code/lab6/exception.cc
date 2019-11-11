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

    if ((which == SyscallException))
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
                machine->ReadMem(addr + i, 1, (int *)&filename[i]); //read filename from mainMemory
            } while (filename[i++] != '\0');

            printf("Exec(%s):\n", filename);

            //open file
            OpenFile *executable = fileSystem->Open(filename);

            if (executable == NULL)
            {
                printf("Unable to open file %s\n", filename);
                return;
            }

            //new address space
            space = new AddrSpace(executable);
            delete executable; // close file

            //new and fork thread
            thread = new Thread("forked thread");
            thread->Fork(StartProcess, 1);

            //run the new thread
            currentThread->Yield();

            //return spaceID
            machine->WriteRegister(2, space->getSpaceID());

            //advance PC
            AdvancePC();
            break;
        }
        case SC_Exit:
        {
            printf("Execute system call of Exit()\n");

            //machine->clear();
            AdvancePC();
            currentThread->Finish();
            break;
        }
        default:
        {
            printf("Unexpected syscall %d %d\n", which, type);
            ASSERT(FALSE);
        }
        }
    }
    else
    {
        printf("Unexpected user mode exception %d %d\n", which, type);
        ASSERT(FALSE);
    }
}
