/** @file
 * Содержит общие функции для всех типов EventProvider.
 */
#include <Uefi.h>
#include <Library/EventProviderLib.h>
#include <Protocol/LoadedImage.h>

#ifndef EVENT_PROVIDER_UTILITY_LIB_H_
#define EVENT_PROVIDER_UTILITY_LIB_H_

// -----------------------------------------------------------------------------
/**
 * Создаёт по LOG_ENTRY_TYPE_IMAGE_EXISTS_ON_STARTUP для каждого образа,
 * уже загруженного в момент нашего запуска.
*/
EFI_STATUS
DetectImagesLoadedOnStartup (
  IN OUT EVENT_PROVIDER *This
  );

// -----------------------------------------------------------------------------
/**
 * По хэндлу образа находит его имя и имя образа-родителя.
 * Указатели, возвращённые через OUT-параметры впоследствии требуется освободить.
*/
VOID
GetHandleImageNameAndParentImageName (
  IN  EFI_HANDLE Handle,
  OUT CHAR16     **ImageName,
  OUT CHAR16     **ParentImageName
  );

// -----------------------------------------------------------------------------
/**
 * Возвращает TRUE если это хэндл исполняемого образа.
*/
BOOLEAN
IsHandleImage (
  IN  EFI_HANDLE Handle
  );

// -----------------------------------------------------------------------------
/**
 * По хэндлу образа находит его имя.
 * Указатель, возвращённый через OUT-параметр впоследствии требуется освободить.
*/
VOID
GetHandleImageName (
  IN  EFI_HANDLE Handle,
  OUT CHAR16     **ImageName
  );

// -----------------------------------------------------------------------------
/**
 * Выделяет память, копирует в неё аргумент, и возвращает указатель на память.
 * В случае неудачи возвращает NULL.
*/
CHAR16 *
StrAllocCopy (
  IN CHAR16 *Str
  );

// -----------------------------------------------------------------------------
/**
 * Добавляет к первой строке вторую, выделяя память под результат и освобождая предыдущую память под StrHead.
 * В случае неудачи оставляет без изменений.
*/
VOID
StrAllocAppend (
  IN OUT CHAR16 **StrHead,
  IN     CHAR16 *StrTail
  );

// -----------------------------------------------------------------------------
/**
 * Возвращает по хэндлу его более информативное описание.
 * Не забыть освободить память из-под возвращаемого значения!
*/
CHAR16 *GetHandleName (
  EFI_HANDLE Handle
  );

// -----------------------------------------------------------------------------
/**
  Взято из ShellPkg, возвращает имя модуля для тех из них что входят в состав образа.

  Function to find the file name associated with a LoadedImageProtocol.

  @param[in] LoadedImage     An instance of LoadedImageProtocol.

  @retval                    A string representation of the file name associated
                             with LoadedImage, or NULL if no name can be found.
**/
CHAR16 *
FindLoadedImageFileName (
  IN EFI_LOADED_IMAGE_PROTOCOL *LoadedImage
  );

// -----------------------------------------------------------------------------

#endif // EVENT_PROVIDER_UTILITY_LIB_H_