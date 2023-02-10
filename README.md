# AetherVisor: AMD-V memory hacking library

AetherVisor is a minimalistic type-1 AMD hypervisor that provides a memory hacking interface.  

Here's how AetherVisor's features are implemented: https://mellownight.github.io/2023/01/19/AetherVisor.html. 

In the writeup, I mentioned multiple potential bugs that I fixed with band-aid solutions. If you experience any of these bugs, feel free to open an issue and/or propose a better fix.

## Features
### Syscall hooks via MSR_LSTAR
```function definition``` - 


### NPT inline hooks
```function definition``` - 

### Branch tracing
```function definition``` - 

### Sandboxing and Read/Write/Execute instrumentation
```SandboxPage(VMMCALL_ID, ...)``` - 

### vmmcall interface
```svm_vmmcall(VMMCALL_ID, ...)``` -

## Components ##

**AetherVisor-api -** Static library that contains wrappers for interfacing with AetherVisor via vmmcall.

**AetherVisor-api-kernel -** A version of AetherVisor-api designed for compilation with Windows kernel drivers.

**AetherVisor-example -** DLL demonstrating AetherVisor's features against Battleye on Unturned. In this example, I log beservice's API calls, trace a VMProtected function, and set a hidden hook on IDXGISwapChain::Present().

## Supported hardware ##
 Intel processors with VT-x and EPT support

## Supported platforms ##
 Windows 7 - Windows 10, x64 only
 
