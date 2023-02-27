#include "utils.h"

#define PE_HEADER(image) ((IMAGE_NT_HEADERS64*)((uint64_t)image + ((IMAGE_DOS_HEADER*)image)->e_lfanew))
