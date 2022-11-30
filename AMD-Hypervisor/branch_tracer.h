#include "logging.h"
#include "paging_utils.h"
#include "disassembly.h"
#include "portable_executable.h"
#include "vmexit.h"
#include "utils.h"

namespace BranchTracer
{
	extern bool active;

	extern int lbr_stack_size;

	struct BasicBlock
	{
		uintptr_t start;
		uintptr_t end;
	};

	struct LogBuffer
	{
		BasicBlock*	cur_block;
		BasicBlock	records[1];
	};
	
	extern uintptr_t log_buffer
	extern void* log_buffer;
};
