#include <Uefi.h>
#include <Library/DebugMacros.h>

#ifndef COMMON_MACROS_H_
#define COMMON_MACROS_H_

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

// Таким образом указываем тип элементов контейнера.
// Никакого смысла кроме документирования не несёт.
#define TYPE(ContainerElementTypeName)

#endif  // COMMON_MACROS_H_