#include <Uefi.h>

#define RETURN_ON_ERR(Status) if (EFI_ERROR (Status)) { return Status; }
