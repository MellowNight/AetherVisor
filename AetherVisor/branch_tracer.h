#include "logging.h"
#include "disassembly.h"
#include "portable_executable.h"
#include "vmexit.h"
#include "utils.h"
#include "npt_hook.h"
#include "instrumentation_hook.h"

namespace BranchTracer
{
	extern CR3 process_cr3;

	extern bool active;
	extern bool initialized;

	extern uintptr_t range_base;
	extern uintptr_t range_size;

	extern uintptr_t stop_address;
	extern uintptr_t start_address;

	extern uintptr_t resume_address;

	extern HANDLE thread_id;

	struct TlsParams
	{
		bool callback_pending;
		void* last_branch_from;
	};

	extern TlsParams* tls_params;

	void Start(VcpuData* vcpu);
	void Stop(VcpuData* vcpu);

	void Pause(VcpuData* vcpu);
	void Resume(VcpuData* vcpu);

	void Init(
		VcpuData* vcpu, 
		uintptr_t start_addr,
		uintptr_t stop_addr, uintptr_t trace_range_base, uintptr_t trace_range_size, TlsParams* tracer_params);

	void UpdateState(VcpuData* vcpu, GuestRegisters* guest_ctx);

};
