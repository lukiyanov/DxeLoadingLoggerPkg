#include "Vector.h"
#include <Library/UefiBootServicesTableLib.h>

#define PTR_OFFSET(Ptr, OffsetInBytes) \
  (VOID*)(((UINT8*) This->AllocatedMemory) + OffsetInBytes)

// ----------------------------------------------------------------------------
/// Инициализирует структуру.
VOID Vector_Construct  (struct Vector *This, UINTN SizeOfType, UINTN InitialCount)
{
  This->ObjectSize      = SizeOfType;
  This->CountAllocated  = InitialCount;
  This->CountUsed       = 0;
  This->AllocatedMemory = NULL;

  gBS->AllocatePool (
    EfiBootServicesData,
    InitialCount * SizeOfType,
    &This->AllocatedMemory
    );
}

// ----------------------------------------------------------------------------
/// Освобождает память из-под структуры.
VOID Vector_Destruct   (struct Vector *This)
{
  if (This->AllocatedMemory) {
    gBS->FreePool (This->AllocatedMemory);
    This->AllocatedMemory = NULL;
  }
}

// ----------------------------------------------------------------------------
/// Копирует объект в конец вектора.
VOID Vector_PushBack   (struct Vector *This, VOID *Object)
{
  if (This->CountUsed == This->CountAllocated) {
    // Увеличиваем количество выделенной памяти вдвое.
    VOID *NewAllocatedMemory;
    UINTN NewCountAllocated = This->CountAllocated * 2;
    UINTN NewSizeInBytes = NewCountAllocated    * This->ObjectSize;
    UINTN OldSizeInBytes = This->CountAllocated * This->ObjectSize;

    gBS->AllocatePool (
      EfiBootServicesData,
      NewSizeInBytes,
      &NewAllocatedMemory
      );

    gBS->CopyMem (
      NewAllocatedMemory,
      This->AllocatedMemory,
      OldSizeInBytes
      );

    gBS->FreePool (This->AllocatedMemory);

    This->CountAllocated  = NewCountAllocated;
    This->AllocatedMemory = NewAllocatedMemory;
  }

  gBS->CopyMem (
    PTR_OFFSET(This->AllocatedMemory, This->ObjectSize * This->CountUsed),
    Object,
    This->ObjectSize);

  This->CountUsed++;
}

// ----------------------------------------------------------------------------
/// Удаляет объект с конца вектора и возвращает значение удалённого элемента.
/// Возвращает NULL если вектор пуст.
VOID *Vector_PopBack (struct Vector *This)
{
  VOID *Last;

  if (This->CountUsed == 0) {
    return NULL;
  }

  Last = PTR_OFFSET(This->AllocatedMemory, This->ObjectSize * (This->CountUsed - 1));
  This->CountUsed--;
  return Last;
}

// ----------------------------------------------------------------------------
/// Возвращает указатель на первый элемент вектора.
VOID *Vector_GetBegin  (struct Vector *This)
{
  return This->AllocatedMemory;
}

// ----------------------------------------------------------------------------
/// Возвращает указатель на элемент вектора, следующий за последним.
VOID *Vector_GetEnd    (struct Vector *This)
{
  return PTR_OFFSET(This->AllocatedMemory, This->ObjectSize * This->CountUsed);
}

// ----------------------------------------------------------------------------
UINTN Vector_Size   (struct Vector *This)
{
  return This->CountUsed;
}

// ----------------------------------------------------------------------------
VOID Vector_Clear (struct Vector *This)
{
  This->CountUsed = 0;
}

// ----------------------------------------------------------------------------

