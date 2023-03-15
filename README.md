# AetherVisor: AMD-V memory hacking library

AetherVisor is a minimalistic type-1 AMD hypervisor that provides a memory hacking interface.  

Here's how AetherVisor's features are implemented: https://mellownight.github.io/2023/01/19/AetherVisor.html. 

In the writeup, I mentioned multiple potential bugs that I fixed with band-aid solutions. If you experience any of these bugs, feel free to open an issue and/or propose a better fix.

## Features

<br>

### Syscall hooks via MSR_LSTAR
```Aether::SyscallHook::Enable()``` - Enables process-wide system call hooks.

```Aether::SyscallHook::Disable()``` - Disables system call hooks.


<br>

### NPT inline hooks

<code>
Aether::NptHook::Set(uintptr_t address, uint8_t* patch, size_t patch_len, NCR3_DIRECTORIES ncr3_id = NCR3_DIRECTORIES::primary, bool global_page = false);</code> - Sets an NPT hook. 
<br>

`patch` - Hook shellcode 

`patch_len` - Hook shellcode 

`length address` - Target address 

`ncr3_id` - The nested paging context that the hook is active in. 

`global_page` - Indicates that a global copy-on-write page (e.g. kernel32.dll, ntdll.dll, etc.) is being hooked.


<br>

### Branch tracing
```Aether::BranchTracer::Init() ``` - Initializes Branch tracer.

```Aether::BranchTracer::Trace(uint8_t* start_addr, uintptr_t range_base, uintptr_t range_size, uint8_t* stop_addr = NULL) ``` - Logs every branch executed until either the return address or the `stop_addr` is reached.

`start_addr` - Where to start tracing

`range_base` - Branches that occur below `range_base` are excluded from the trace

`range_base` - Branches that occur above `range_base` + `range_size` are excluded from the trace

`stop_address` - Where to stop tracing branches; if this value is NULL, the return address on the stack is used. 
 
<br>

#### Sandboxing and Read/Write/Execute instrumentation

```Aether::Sandbox::SandboxRegion(uintptr_t base, uintptr_t size);``` - Puts a region of memory/code into the no-execute region.

```Aether::Sandbox::DenyRegionAccess(void* base, size_t range, bool allow_reads);``` - Deny read/write access to pages outside of the sandbox for code inside of the sandbox.

```Aether::Sandbox::UnboxRegion(uintptr_t base, uintptr_t size);``` - Removes pages from the sandbox.

#### vmmcall interface
```svm_vmmcall(VMMCALL_ID, ...)``` -

```
// callback IDs and function prototypes:

enum CALLBACK_ID
{
    // 
    sandbox_readwrite = 0, 

    // 
    sandbox_execute = 1,

    //
    branch = 2,

    //
    branch_trace_finished = 3,

    //
    syscall = 4,

    max_id
};

Aether::SetCallback(CALLBACK_ID handler_id, void* address);
```

## Components ##

**AetherVisor-lib -** Static library that contains wrappers for interfacing with AetherVisor via vmmcall.

**AetherVisor-lib-kernel -** A version of AetherVisor-api designed for compilation with Windows kernel drivers.

**AetherVisor-example -** EXE demonstrating AetherVisor's features.

## Supported hardware ##
 Intel processors with VT-x and EPT support

## Supported platforms ##
 Windows 7 - Windows 10, x64 only
 
