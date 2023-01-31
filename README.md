AetherVisor: AMD-V memory hacking library
=========

AetherVisor is a minimalistic type-2 AMD hypervisor that provides a memory hacking interface. Add  

Here is an overview of how AetherVisor's features are implemented, along with a description with : https://mellownight.github.io/2023/01/19/AetherVisor.html. 

I wrote about multiple potential bugs that I wrote band-aid fixes for in AetherVisor. If you experience any of these bugs, feel free to open an issue and propose a fix.

## Features
### Syscall hooks via MSR_LSTAR
**function definition**


### NPT inline hooks
**function definition**

### Branch tracing
**function definition**

### Sandboxing and Read/Write/Execute instrumentation
**SandboxPage(VMMCALL_ID, ...) - **

### vmmcall interface
**svm_vmmcall(VMMCALL_ID, ...) - **

## Components ##

**aethervisor-api -** Static library containing wrappers for using vmmcall to interface with AetherVisor.


**aethervisor-api-kernel -** A version of AetherVisor-api for compilation with Windows kernel drivers.

**aethervisor-example -** DLL demonstrating the features of AetherVisor against Battleye on Unturned. In this example, I log BEClient's API calls, trace a VMProtected function, and set a hidden hook on IDXGISwapChain::Present().

## Supported hardware ##
 Intel processors with VT-x and EPT support

## Supported platforms ##
 Windows 7 - Windows 10, x64 only
 
