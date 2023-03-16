#include "portable_executable.h"

#include "test_branch_trace.h"
#include "test_npt_hook.h"
#include "test_syscall_hook.h"
#include "test_sandbox_hook.h"

int main()
{
	SandboxTest();
	BranchTraceTest();
	EferSyscallHookTest();
	NptHookTest();
}