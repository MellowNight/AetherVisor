AetherVisor: AMD-V memory hacking library
=========

AetherVisor is a minimalistic type-2 AMD hypervisor that provides a memory hacking interface. 

Here is an overview of how AetherVisor's features are implemented : https://mellownight.github.io/2023/01/19/AetherVisor.html

## Features
### Syscall hooks via MSR_LSTAR
**function definition**


### NPT inline hooks
**function definition**

### Branch tracing
**function definition**

### Sandboxing and Read/Write/Execute instrumentation
**function definition**

### vmmcall interface
**function definition**

## Components ##

**aethervisor-api -** Static library containing wrappers for using vmmcall to interface with AetherVisor.


**aethervisor-api-kernel -** Static library containing vmmcall wrappers, intended for compilation with Windows kernel drivers.

**aethervisor-example -** DLL demonstrating the features of AetherVisor against Battleye on Unturned. In this example, I log BEClient's API calls, trace a VMProtected function, and set a hidden hook on IDXGISwapChain::Present().

## Supported hardware ##
 Intel processors with VT-x and EPT support

## Supported platforms ##
 Windows 7 - Windows 10, x64 only
