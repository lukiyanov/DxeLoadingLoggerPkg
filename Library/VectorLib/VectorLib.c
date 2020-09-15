#include <Library/VectorLib.h>
#include <Library/UefiBootServicesTableLib.h>

// -----------------------------------------------------------------------------
#define PTR_OFFSET(Ptr, OffsetInBytes) \
  (VOID*)(((UINT8*) This->AllocatedMemory) + OffsetInBytes)

// -----------------------------------------------------------------------------
/**
 * Инициализирует структуру VECTOR, выделяет память под InitialCount элементов размера SizeOfElement.
 * Функция должна быть обязательно однократно вызвана перед использованием объекта.
 *
 * @param This                      Указатель на инициализируемую структуру вектора.
 * @param SizeOfElement             Размер одного элемента в байтах.
 * @param InitialCount              Начальный размер массива, измеряется в элементах (не байтах).
 *
 * @retval EFI_SUCCESS              Операция завершена успешно, массивом можно пользоваться.
 * @retval EFI_OUT_OF_RESOURCES     Не удалось выделить память.
 * @retval EFI_INVALID_PARAMETER    Один из параметров равен нулю.
 */
EFI_STATUS
Vector_Construct (
  IN OUT  VECTOR          *This,
  IN      UINTN           SizeOfElement,
  IN      UINTN           InitialCount
  )
{
  This->AllocatedMemory = NULL;

  if (This == NULL || SizeOfElement == 0 || InitialCount == 0) {
    return EFI_INVALID_PARAMETER;
  }

  EFI_STATUS Status;
  Status = gBS->AllocatePool (
                  EfiBootServicesData,
                  InitialCount * SizeOfElement,
                  &This->AllocatedMemory
                  );
  if (EFI_ERROR(Status)) {
    return EFI_OUT_OF_RESOURCES;
  }

  This->ObjectSize      = SizeOfElement;
  This->CountAllocated  = InitialCount;
  This->CountUsed       = 0;

  return EFI_SUCCESS;
}

// -----------------------------------------------------------------------------
/**
 * Освобождает память из-под структуры.
 * Функция должна быть обязательно однократно вызвана после завершения использования объекта.
 *
 * @param This                      Указатель на структуру вектора.
 */
VOID
Vector_Destruct (
  IN OUT  VECTOR          *This
  )
{
  if (This->AllocatedMemory) {
    gBS->FreePool (This->AllocatedMemory);
    This->AllocatedMemory = NULL;
  }
}

// -----------------------------------------------------------------------------
/**
 * Добавляет копию объекта в конец вектора.
 *
 * @param This                      Указатель на структуру вектора.
 * @param Object                    Указатель на элемент, который требуется добавить.
 *
 * @retval EFI_SUCCESS              Операция завершена успешно.
 * @retval EFI_OUT_OF_RESOURCES     Не удалось выделить память.
 * @retval EFI_INVALID_PARAMETER    Один из параметров равен NULL.
 */
EFI_STATUS
Vector_PushBack (
  IN OUT  VECTOR          *This,
  IN      VOID            *Object
  )
{
  EFI_STATUS Status;

  if (This == NULL || Object == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (This->CountUsed == This->CountAllocated) {
    // Увеличиваем количество выделенной памяти вдвое.
    VOID *NewAllocatedMemory = NULL;
    UINTN NewCountAllocated  = This->CountAllocated * 2;
    UINTN OldSizeInBytes     = This->CountAllocated * This->ObjectSize;
    UINTN NewSizeInBytes     = NewCountAllocated    * This->ObjectSize;

    Status = gBS->AllocatePool (
                    EfiBootServicesData,
                    NewSizeInBytes,
                    &NewAllocatedMemory
                    );
    if (EFI_ERROR(Status)) {
      return EFI_OUT_OF_RESOURCES;
    }

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
    This->ObjectSize
    );

  This->CountUsed++;

  return EFI_SUCCESS;
}

// -----------------------------------------------------------------------------
/**
 * Удаляет объект с конца вектора и возвращает значение удалённого элемента,
 * но не освобождает выделенной памяти.
 *
 * @param This                      Указатель на структуру вектора.
 *
 * @retval NULL                     Вектор пуст, либо This == NULL.
 * @return Указатель на удалённое из вектора значение.
 *         При первом же добавлении элемента в вектор это значение будет утеряно.
 */
VOID *
Vector_PopBack (
  IN OUT  VECTOR          *This
  )
{
  if (This == NULL || This->CountUsed == 0) {
    return NULL;
  }

  VOID *Last = PTR_OFFSET(This->AllocatedMemory, This->ObjectSize * (This->CountUsed - 1));
  This->CountUsed--;
  return Last;
}

// -----------------------------------------------------------------------------
/**
 * Возвращает указатель на элемент вектора с индексом Index, нумерация с 0.
 *
 * @param This                      Указатель на структуру вектора.
 * @param Index                     Индекс элемента в векторе.
 *
 * @retval NULL                     Выход за границы вектора, либо This == NULL.
 * @return Указатель на элемент с индексом Index.
 */
VOID *
Vector_Get (
  IN      VECTOR          *This,
  IN      UINTN           Index
  )
{
  if (This == NULL || Index >= This->CountUsed) {
    return NULL;
  }

  return PTR_OFFSET(This->AllocatedMemory, This->ObjectSize * Index);
}

// -----------------------------------------------------------------------------
/**
 * Возвращает указатель на первый элемент вектора.
 *
 * @param This                      Указатель на структуру вектора.
 *
 * @retval NULL                     Вектор пуст, либо This == NULL.
 * @return Указатель на первый элемент вектора.
 */
VOID *
Vector_GetBegin (
  IN      VECTOR          *This
  )
{
  if (This == NULL || This->CountUsed == 0) {
    return NULL;
  }

  return This->AllocatedMemory;
}

// -----------------------------------------------------------------------------
/**
 * Возвращает указатель на элемент вектора, следующий за последним.
 *
 * @param This                      Указатель на структуру вектора.
 *
 * @retval NULL                     Вектор пуст, либо This == NULL.
 * @return Указатель на элемент вектора, следующий за последним.
 */
VOID *
Vector_GetEnd (
  IN      VECTOR          *This
  )
{
  if (This == NULL || This->CountUsed == 0) {
    return NULL;
  }

  return PTR_OFFSET(This->AllocatedMemory, This->ObjectSize * This->CountUsed);
}

// -----------------------------------------------------------------------------
/**
 * Возвращает указатель на последний элемент вектора.
 *
 * @param This                      Указатель на структуру вектора.
 *
 * @retval NULL                     Вектор пуст, либо This == NULL.
 * @return Указатель на последний элемент вектора.
 */
VOID *
Vector_GetLast (
  IN      VECTOR          *This
  )
{
  if (This == NULL || This->CountUsed == 0) {
    return NULL;
  }

  return PTR_OFFSET(This->AllocatedMemory, This->ObjectSize * (This->CountUsed - 1));
}

// -----------------------------------------------------------------------------
/**
 * Возвращает количество хранимых элементов в векторе.
 *
 * @param This                      Указатель на структуру вектора.
 *
 * @return Количество хранимых элементов.
 */
UINTN
Vector_Size (
  IN      VECTOR          *This
  )
{
  if (This == NULL) {
    return 0;
  }

  return This->CountUsed;
}

// -----------------------------------------------------------------------------
/**
 * Сбрасывает число занятых элементов до нуля, но не освобождает память.
 *
 * @param This                      Указатель на структуру вектора.
 */
VOID
Vector_Clear (
  IN OUT  VECTOR          *This
  )
{
  if (This == NULL) {
    return;
  }

  This->CountUsed = 0;
}

// ----------------------------------------------------------------------------

