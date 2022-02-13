#pragma once
#include "Global.h"

void HandleCommands();

EXTERN_C int NTAPI svm_vmmcall(UINT64 Rcx, UINT64	Rdx, UINT64	R8, UINT64	R9);