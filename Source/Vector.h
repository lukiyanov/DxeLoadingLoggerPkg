#pragma once

#include <Uefi.h>


struct Vector;

// ----------------------------------------------------------------------------
/// Инициализирует структуру.
VOID Vector_Construct  (struct Vector *This, UINTN SizeOfType, UINTN InitialCount);

/// Освобождает память из-под структуры.
VOID Vector_Destruct   (struct Vector *This);

/// Копирует объект в конец вектора.
VOID Vector_PushBack   (struct Vector *This, VOID *Object);

/// Удаляет объект с конца вектора и возвращает значение удалённого элемента.
/// Возвращает NULL если вектор пуст.
VOID *Vector_PopBack   (struct Vector *This);

/// Возвращает указатель на первый элемент вектора.
VOID *Vector_GetBegin  (struct Vector *This);

/// Возвращает указатель на элемент вектора, следующий за последним.
VOID *Vector_GetEnd    (struct Vector *This);

/// Возвращает количество занятых элементов.
UINTN Vector_Size      (struct Vector *This);

/// Сбрасывает число занятых элементов до нуля, но не освобождает память.
VOID Vector_Clear      (struct Vector *This);

// ----------------------------------------------------------------------------
struct Vector
{
  UINTN CountAllocated;
  UINTN CountUsed;
  UINTN ObjectSize;
  VOID  *AllocatedMemory;
};

// ----------------------------------------------------------------------------
#define VECTOR(Type, Size, VarName) \
  struct Vector VarName; \
  Vector_Construct(&VarName, sizeof(Type), Size)

#define FOR_EACH_VCT(ElementType, IteratorVarName, VectorName) \
  ElementType *IteratorVarName = Vector_GetBegin(&(VectorName)); \
  ElementType *EndIterator     = Vector_GetEnd(&(VectorName)); \
  for (; IteratorVarName != EndIterator; ++IteratorVarName)
