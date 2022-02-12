#pragma once
#include "Global.h"
#include "Structs.h"

#define USER_VMMCALL 0xAAAA
#define KERNEL_VMMCALL 0xBBBB

void	HandleCommands();

EXTERN_C int NTAPI svm_vmmcall(UINT64 Rcx, UINT64	Rdx, UINT64	R8, UINT64	R9);