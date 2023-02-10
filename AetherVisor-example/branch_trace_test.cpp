#include "aethervisor_test.h"

using namespace AetherVisor;
using namespace AetherVisor::BranchTracer;

/*  branch_trace_test.cpp:  ???  */


std::vector<BranchLog::LogEntry> traced_branches;

void BranchLogFullHook()
{
	Utils::Log(
		"BranchLogFullHook(), AetherVisor::log_buffer->info.buffer 0x%p, AetherVisor::log_buffer->info.buffer_idx %i \n",
		BranchTracer::log_buffer->info.buffer, BranchTracer::log_buffer->info.buffer_idx
	);

	traced_branches.insert(traced_branches.end(), BranchTracer::log_buffer->info.buffer,
		BranchTracer::log_buffer->info.buffer + BranchTracer::log_buffer->info.buffer_idx);
}

void BranchTraceFinished()
{
	Utils::Log("BranchTraceFinished(), dumping branch trace to file! \n");

	for (auto entry : traced_branches)
	{
		Utils::LogToFile(LOG_FILE, "branch %s -> %s", 
			AddressInfo{ (void*)entry.branch_address }.Format().c_str(),
			AddressInfo{ (void*)entry.branch_target }.Format().c_str()
		);

		Utils::LogToFile(LOG_FILE, "\n");
	}
}

/*	trace BE shellcode report	*/

void BranchTraceTest()
{
	auto beclient = (uintptr_t)GetModuleHandle(L"BEClient.dll");

	AetherVisor::SetCallback(AetherVisor::branch_log_full, BranchLogFullHook);
	AetherVisor::SetCallback(AetherVisor::branch_trace_finished, BranchTraceFinished);

	BranchTracer::Trace(
		(uint8_t*)GetModuleHandleA(NULL) + 0x1100, beclient, PeHeader(beclient)->OptionalHeader.SizeOfImage);
}
