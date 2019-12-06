// addrspace.cc
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "noff.h"

//-----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void
SwapHeader(NoffHeader *noffH)
{
    noffH->noffMagic = WordToHost(noffH->noffMagic);
    noffH->code.size = WordToHost(noffH->code.size);
    noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
    noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
    noffH->initData.size = WordToHost(noffH->initData.size);
    noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
    noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
    noffH->uninitData.size = WordToHost(noffH->uninitData.size);
    noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
    noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------

AddrSpace::AddrSpace(OpenFile *executable)
{
    // extended addrSpace
    bool flag = FALSE;
    for (int i = 0; i < 128; i++)
        if (!ThreadMap[i])
        {
            ThreadMap[i] = 1;
            flag = TRUE;
            spaceID = i;
            break;
        }
    ASSERT(flag);

    NoffHeader noffH;

    unsigned int size; // 需要的内存大小

    // 初始化记录页号的表
    extern unsigned int vpTable[MaxPages];
    for (int i = 0; i < MaxPages; i++)
        vpTable[i] = -1;

    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && (WordToHost(noffH.noffMagic) == NOFFMAGIC))
        SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

    // Address Space
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size + UserStackSize;
    numPages = divRoundUp(size, PageSize);
    size = numPages * PageSize;

    // Virtual Memory
    OpenFile *swapFile;   // 分页文件
    unsigned int phPages; // 已经分配的物理页
    unsigned int phSize;  // 分配的物理内存大小
    unsigned int coPages; // 代码区分配的物理页
    char *buffer;

    if (numPages <= MinPages)
        // 需要分配的页并不多，因此无需虚拟内存
        phPages = numPages;
    else
    {
        // 需要分配的页足够多，因此需要虚拟内存
        phPages = MinPages;
        // 创建分页文件
        vmName = "SwapFile\0";
        fileSystem->Create(vmName, size);
        swapFile = fileSystem->Open(vmName);
    }

    // 实际待分配的物理内存
    phSize = phPages * PageSize;
    ASSERT(phPages <= bitmap->NumClear());
    DEBUG('a', "Initializing address space, num pages %d, size %d\n", phPages, phSize);

    // 首先，初始化页表
    pageTable = new TranslationEntry[numPages];

    // 计算代码区需要页数，并且分配物理页
    count = divRoundUp(noffH.code.size, PageSize);
    if (count > CodePages)
        count = CodePages;
    coPages = count;

    for (int i = 0; i < count; i++)
    {
        pageTable[i].virtualPage = i;
        pageTable[i].physicalPage = bitmap->Find();
        pageTable[i].valid = FALSE;
        pageTable[i].use = FALSE;
        pageTable[i].dirty = FALSE;
        pageTable[i].readOnly = FALSE;
    }
    // 不能进入物理内存的部分分配页表，其中物理页为-1
    for (int i = count; i < numPages - 1; i++)
    {
        pageTable[i].virtualPage = i;
        pageTable[i].physicalPage = -1;
        pageTable[i].valid = FALSE;
        pageTable[i].use = FALSE;
        pageTable[i].dirty = FALSE;
        pageTable[i].readOnly = FALSE;
    }
    // 为最后一个虚拟页分配物理页
    for (int i = numPages - 1; i < numPages; i++)
    {
        pageTable[i].virtualPage = i;
        pageTable[i].physicalPage = bitmap->Find();
        pageTable[i].valid = TRUE;
        pageTable[i].use = FALSE;
        pageTable[i].dirty = FALSE;
        pageTable[i].readOnly = FALSE;
        // 使用的物理页个数加1
        count++;
        for (int vpc = 0; vpc < MaxPages; vpc++)
            if (vpTable[vpc] == -1)
            {
                vpTable[vpc] = i;
                printf("AddrSpace: Successfully Load Page # %d.\n", i);
                break;
            }
    }

    // 代码区部分
    if (noffH.code.size > 0)
    {
        DEBUG('a', "Initializing code segment, at 0x%x, size %d\n", noffH.code.virtualAddr, noffH.code.size);
        // 物理内存
        unsigned int code_start, code_end;
        // 计算
        code_start = divRoundUp(noffH.code.virtualAddr, PageSize);
        code_end = code_start + coPages;
        // 分配
        for (int i = code_start; i < code_end; i++)
        {
            // 计算物理地址
            int code_phy_addr = pageTable[i].physicalPage * PageSize;
            // 读内存
            executable->ReadAt(&(machine->mainMemory[code_phy_addr]), PageSize, noffH.code.inFileAddr + i * PageSize);
            pageTable[i].valid = TRUE;
            for (int vpc = 0; vpc < MaxPages; vpc++)
                if (vpTable[vpc] == -1)
                {
                    vpTable[vpc] = i;
                    printf("AddrSpace: Successfully Code Load Page # %d.\n", i);
                    break;
                }
        }
        // 交换文件
        if (numPages > MinPages)
        {
            buffer = new char[PageSize];
            int j, k = divRoundUp(noffH.code.size, PageSize) - 1;
            for (j = 0; j < k; j++)
            {
                executable->ReadAt(buffer, PageSize, noffH.code.inFileAddr + j * PageSize);
                swapFile->WriteAt(buffer, PageSize, noffH.code.virtualAddr + j * PageSize);
            }
            executable->ReadAt(buffer, noffH.code.size - k * PageSize, noffH.code.inFileAddr + j * PageSize);
            swapFile->WriteAt(buffer, noffH.code.size - k * PageSize, noffH.code.virtualAddr + j * PageSize);
            // 是否有数据
            if (noffH.initData.size == 0)
            {
                delete swapFile;
                delete[] buffer;
            }
        }
    }

    // 数据区部分
    if (noffH.initData.size > 0)
    {
        DEBUG('a', "Initializing data segment, at 0x%x, size %d\n", noffH.initData.virtualAddr, noffH.initData.size);
        // 物理内存
        unsigned int data_start, data_end;
        unsigned int daPages;
        // 计算
        data_start = divRoundUp(noffH.initData.virtualAddr, PageSize);
        daPages = divRoundUp(noffH.initData.size, PageSize);
        if (daPages <= DataPages)
            data_end = data_start + daPages;
        else
            data_end = data_start + DataPages;
        // 分配
        for (int i = data_start; i < data_end; i++)
        {
            if (pageTable[i].valid == FALSE)
                pageTable[i].physicalPage = bitmap->Find();
            // 计算地址
            int data_phy_addr = pageTable[i].physicalPage * PageSize;
            // 读内存
            executable->ReadAt(&(machine->mainMemory[data_phy_addr]), PageSize, noffH.initData.inFileAddr + i * PageSize);
            pageTable[i].valid = TRUE;
            for (int vpc = 0; vpc < MaxPages; vpc++)
                if (vpTable[vpc] == -1)
                {
                    vpTable[vpc] = i;
                    printf("AddrSpace: Successfully Load Data Page # %d.\n", i);
                    break;
                }
        }
        count += data_end - data_start;
        // 交换文件
        if (numPages > MinPages)
        {
            int j, k = divRoundUp(noffH.initData.size, PageSize) - 1 + data_start;
            for (j = data_start; j < k; j++)
            {
                executable->ReadAt(buffer, PageSize, noffH.initData.inFileAddr + (j - data_start) * PageSize);
                swapFile->WriteAt(buffer, PageSize, noffH.initData.virtualAddr + (j - data_start) * PageSize);
            }
            executable->ReadAt(buffer, noffH.initData.size - (k - data_start) * PageSize, noffH.initData.inFileAddr + j * PageSize);
            swapFile->ReadAt(buffer, noffH.initData.size - (k - data_start) * PageSize, noffH.initData.virtualAddr + j * PageSize);
            // release
            delete swapFile;
            delete[] buffer;
        }
    }

    Print();
}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
    ThreadMap[spaceID] = 0;

    // Clear pageTable
    for (int i = 0; i < numPages; i++)
        bitmap->Clear(pageTable[i].physicalPage);
    delete[] pageTable;
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void AddrSpace::InitRegisters()
{
    for (int i = 0; i < NumTotalRegs; i++)
        machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister(PCReg, 0);

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister(NextPCReg, 4);

    // Set the stack register to the end of the address space, where we
    // allocated the stack; but subtract off a bit, to make sure we don't
    // accidentally reference off the end!
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//----------------------------------------------------------------------

void AddrSpace::SaveState()
{
}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//----------------------------------------------------------------------

void AddrSpace::RestoreState()
{
    machine->pageTable = pageTable;
    machine->pageTableSize = numPages;
}

//----------------------------------------------------------------------
// AddrSpace::Print
// Print virtual memory and physical memory page and table infomation.
//----------------------------------------------------------------------

void AddrSpace::Print()
{
    printf("Page table dump: %d pages in total\n", count);
    printf("=================================================\n");
    printf("\tvPage\tpPage\tValid\t Use\tDirty\n");
    for (int i = 0; i < numPages; i++)
        printf("\t  %d \t  %d \t  %d \t  %d \t  %d\n", pageTable[i].virtualPage, pageTable[i].physicalPage,
               pageTable[i].valid, pageTable[i].use, pageTable[i].dirty);
    printf("=================================================\n");
}