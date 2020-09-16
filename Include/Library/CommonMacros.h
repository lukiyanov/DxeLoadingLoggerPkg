#include <Uefi.h>
#include <Library/DebugMacros.h>

#define RETURN_ON_ERR(Status) \
  if (EFI_ERROR (Status)) {   \
    DBG_EXIT_STATUS (Status); \
    return Status;            \
  }

#define SHELL_FREE_NON_NULL(Pointer)  \
  if ((Pointer) != NULL) {            \
    gBS->FreePool (Pointer);          \
    (Pointer) = NULL;                 \
  }
