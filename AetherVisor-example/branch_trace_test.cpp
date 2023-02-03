#include "aethervisor_test.h"

std::vector<BranchLog::LogEntry> traced_branches;

void BranchLogFullHook()
{
	Logger::Get()->Print(COLOR_ID::blue, "BranchLogFullHook(), AetherVisor::log_buffer->info.buffer 0x%p, AetherVisor::log_buffer->info.buffer_idx %i \n", AetherVisor::log_buffer->info.buffer, AetherVisor::log_buffer->info.buffer_idx);

	traced_branches.insert(traced_branches.end(),
		AetherVisor::log_buffer->info.buffer, AetherVisor::log_buffer->info.buffer + AetherVisor::log_buffer->info.buffer_idx);
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

void BranchTraceTest()
{
	AetherVisor::InstrumentationHook(AetherVisor::branch_log_full, BranchLogFullHook);
	AetherVisor::InstrumentationHook(AetherVisor::branch_trace_finished, BranchTraceFinished);

	AetherVisor::TraceFunction(
		(uint8_t*)GetModuleHandleA(NULL) + 0x1100, beclient, PeHeader(beclient)->OptionalHeader.SizeOfImage);
}
