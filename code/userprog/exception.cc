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
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "main.h"
#include "syscall.h"
#include "ksyscall.h"

#include "synchconsole.h"
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
// If you are handling a system call, don't forget to increment the pc
// before returning. (Or else you'll loop making the same system call forever!)
//
//	"which" is the kind of exception.  The list of possible exceptions
//	is in machine.h.
//----------------------------------------------------------------------
void IncrementPC()
{
	/* Modify return point */
	{
		/* set previous programm counter (debugging only)*/
		kernel->machine->WriteRegister(PrevPCReg, kernel->machine->ReadRegister(PCReg));

		/* set programm counter to next instruction (all Instructions are 4 byte wide)*/
		kernel->machine->WriteRegister(PCReg, kernel->machine->ReadRegister(PCReg) + 4);

		/* set next programm counter for brach execution */
		kernel->machine->WriteRegister(NextPCReg, kernel->machine->ReadRegister(PCReg) + 4);
	}
}

char *User2System(int virtAddr, int limit)
{
	int i; // chi so index
	int oneChar;
	char *kernelBuf = NULL;
	kernelBuf = new char[limit + 1]; // can cho chuoi terminal
	if (kernelBuf == NULL)
		return kernelBuf;

	memset(kernelBuf, 0, limit + 1);

	for (i = 0; i < limit; i++)
	{
		kernel->machine->ReadMem(virtAddr + i, 1, &oneChar);
		kernelBuf[i] = (char)oneChar;
		if (oneChar == 0)
			break;
	}
	return kernelBuf;
}

int System2User(int virtAddr, int len, char *buffer)
{
	if (len < 0)
		return -1;
	if (len == 0)
		return len;
	int i = 0;
	int oneChar = 0;
	do
	{
		oneChar = (int)buffer[i];
		kernel->machine->WriteMem(virtAddr + i, 1, oneChar);
		i++;
	} while (i < len && oneChar != 0);
	return i;
}

void ExceptionHandler(ExceptionType which)
{
	int type = kernel->machine->ReadRegister(2);

	DEBUG(dbgSys, "Received Exception " << which << " type: " << type << "\n");

	switch (which)
	{
	case SyscallException:
		switch (type)
		{
		case SC_Halt:
			DEBUG(dbgSys, "Shutdown, initiated by user program.\n");

			SysHalt();

			ASSERTNOTREACHED();
			break;

		case SC_Add:
			DEBUG(dbgSys, "Add " << kernel->machine->ReadRegister(4) << " + " << kernel->machine->ReadRegister(5) << "\n");

			/* Process SysAdd Systemcall*/
			int result;
			result = SysAdd(/* int op1 */ (int)kernel->machine->ReadRegister(4),
							/* int op2 */ (int)kernel->machine->ReadRegister(5));

			DEBUG(dbgSys, "Add returning with " << result << "\n");
			/* Prepare Result */
			kernel->machine->WriteRegister(2, (int)result);

			IncrementPC();
			return;
			ASSERTNOTREACHED();

			break;

		case SC_ReadNum:
		{
			int MAX_LENGTH_INT = 11;
			int num = 0;
			bool valid = true;
			char* buffer = new char [MAX_LENGTH_INT+1];
			int index = 0;
			buffer[index] = kernel->synchConsoleIn->GetChar();
			int currentIndex = 0;
			if (buffer[index] == '-') {
				currentIndex = 1;
			}

			while (buffer[index] != '\n') {
				if (buffer[index] < '0' || buffer[index] > '9') {
					if (!(index == 0 && currentIndex == 1)){
						DEBUG(dbgSys, "Invalid character" << "\n");
						valid = false;
						break;						
					}
				}
				index++;
				buffer[index] = kernel->synchConsoleIn->GetChar();
			}
			if (index > MAX_LENGTH_INT) {
				valid = false;
				DEBUG(dbgSys, "Over size of integer");
			}
			if (valid) {
				for (int i = currentIndex; i < index; i++) {
					num = num * 10 + (int)(buffer[i] - 48);
				}
			}
			if (currentIndex == 1) num*=-1;

			DEBUG(dbgSys, "Result: " << num << "\n");


			kernel->machine->WriteRegister(2, num);
			IncrementPC();
			delete buffer;
			return;
			ASSERTNOTREACHED();
			break;
		}

		case SC_PrintNum:
		{
			int res = kernel->machine->ReadRegister(4);

			if (res == 0) {
				kernel->synchConsoleOut->PutChar('0');
				IncrementPC();
				return;
			}
			
			char * buffer = new char [12];
			
			int currentIndex = 0;
			
			int tmp = res;
			int countDigit = 0;

			while (tmp) {
				tmp /= 10;
				countDigit++;
			}
			if (res < 0) {
				buffer[countDigit] = '-';
				tmp = -res;
			}else{
				countDigit--;
				tmp = res;
			}
			
			while (tmp) {
				buffer[currentIndex] = (char)((tmp % 10) + 48);	
				currentIndex++;
				tmp/=10;
			}
			for (int i = countDigit; i >=0; i--) {
				kernel->synchConsoleOut->PutChar(buffer[i]);
			}
			
			delete buffer;
			IncrementPC();
			return;
			ASSERTNOTREACHED();
			break;
		}

		case SC_ReadChar:
		{
			int MAX_BUFFER = 256;
			char * input  = new char [256+1];
			int index = -1;

			do {
				input[++index] = kernel->synchConsoleIn->GetChar();	
			}
			while (input[index] != '\n');

			if (index > 0) {
				DEBUG(dbgSys, "Not a single character \n");
			}

			kernel->machine->WriteRegister(2, input[0]);

			IncrementPC();
			return;
			ASSERTNOTREACHED();
			break;
		}

		case SC_PrintChar:
		{
			char res = (char)(kernel->machine->ReadRegister(4));

			kernel->synchConsoleOut->PutChar(res);

			// kernel->synchConsoleOut->PutChar('\n');

			IncrementPC();
			return;
			ASSERTNOTREACHED();
			break;
		}

		case SC_RandomNum:
		{
			int res = rand();

			kernel->machine->WriteRegister(2, res);

			IncrementPC();
			return;
			ASSERTNOTREACHED();
			break;
		}

		case SC_ReadString:
		{
			int addressBuffer = kernel->machine->ReadRegister(4);
			int length = kernel->machine->ReadRegister(5);

			int index = 0;
			int tmp = length;
			char * buffer = User2System(addressBuffer, length);

			while (tmp) {
				buffer[index] = kernel->synchConsoleIn->GetChar();
				if (buffer[index] == '\n') break;
				index++;
				tmp--;
			}
			
			System2User(addressBuffer, length, buffer);
			
			delete buffer;

			IncrementPC();
			return;
			ASSERTNOTREACHED();
			break;
		}

		case SC_PrintString:
		{
			int MAX_LENGTH_STRING = 1000;
			int addressBuffer = kernel->machine->ReadRegister(4);

			char * buffer = User2System(addressBuffer, MAX_LENGTH_STRING);

			int index = 0;
			while (buffer[index] != '\0') {
				index++;
			}

			for (int i = 0; i < index; i++) {
				kernel->synchConsoleOut->PutChar(buffer[i]);
			}

			delete buffer;

			IncrementPC();
			return;
			ASSERTNOTREACHED();
			break;
		}

		default:
			cerr << "Unexpected system call " << type << "\n";
			break;
		}

		break;
	case NoException:
		DEBUG(dbgSys, "No Exception.\n");
		SysHalt();
		break;
	case PageFaultException:
		DEBUG(dbgSys, "Page Fault Exception.\n");
		SysHalt();
		break;
	case ReadOnlyException:
		DEBUG(dbgSys, "Read Only Exception.\n");
		SysHalt();
		break;
	case BusErrorException:
		DEBUG(dbgSys, "Bus Error Exception.\n");
		SysHalt();
		break;
	case AddressErrorException:
		DEBUG(dbgSys, "Address Error Exception.\n");
		SysHalt();
		break;
	case OverflowException:
		DEBUG(dbgSys, "Overflow Exception.\n");
		SysHalt();
		break;
	case IllegalInstrException:
		DEBUG(dbgSys, "Illegal Instr Exception.\n");
		SysHalt();
		break;
	case NumExceptionTypes:
		DEBUG(dbgSys, "Num ExceptionType Exception.\n");
		SysHalt();
		break;
	default:
		cerr << "Unexpected user mode exception" << (int)which << "\n";
		break;
	}
	ASSERTNOTREACHED();
}
