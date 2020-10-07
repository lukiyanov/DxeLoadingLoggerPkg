#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/PrintLib.h>
#include <Library/DevicePathLib.h>
#include <Pi/PiFirmwareFile.h>
#include <Pi/PiFirmwareVolume.h>

#include <Protocol/FirmwareVolume2.h>
#include <Protocol/LoadedImage.h>

#include <Library/EventProviderUtilityLib.h>
#include <Library/CommonMacrosLib.h>


// -----------------------------------------------------------------------------
#define GET_HANDLE_NAME_BUFFER_SIZE          1024

// -----------------------------------------------------------------------------
/**
 * Создаёт по LOG_ENTRY_TYPE_IMAGE_EXISTS_ON_STARTUP для каждого образа,
 * уже загруженного в момент нашего запуска.
*/
EFI_STATUS
DetectImagesLoadedOnStartup (
  IN OUT EVENT_PROVIDER *This
  )
{
  DBG_ENTER ();

  EFI_STATUS  Status;
  UINTN       HandleCount;
  EFI_HANDLE  *Handles = NULL;

  // Ищем все протоколы gEfiLoadedImageProtocolGuid и логируем их.
  // Мы рассчитываем что в дальнейшем образы не будут выгружаться.
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiLoadedImageProtocolGuid,
                  NULL,
                  &HandleCount,
                  &Handles
                  );
  RETURN_ON_ERR (Status)

  for (UINTN Index = 0; Index < HandleCount; ++Index) {
    DBG_INFO ("-- Detecting image %u/%u name...\n", (unsigned)(Index + 1), (unsigned)HandleCount);

    // Генерим событие.
    LOADING_EVENT Event;
    Event.Type = LOG_ENTRY_TYPE_IMAGE_EXISTS_ON_STARTUP;
    GetHandleImageNameAndParentImageName (
      Handles[Index],
      &Event.ImageExistsOnStartup.ImageName,
      &Event.ImageExistsOnStartup.ParentImageName
      );

    This->AddEvent (This->ExternalData, &Event);
  }

  gBS->FreePool (Handles);
  DBG_EXIT_STATUS (EFI_SUCCESS);
  return EFI_SUCCESS;
}

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
  )
{
  DBG_ENTER ();

  GetHandleImageName(Handle, ImageName);

  EFI_STATUS Status;

  EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
  Status = gBS->OpenProtocol (
                  Handle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID **)&LoadedImage,
                  gImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    *ParentImageName = StrAllocCopy (L"<ERROR: can\'t open the EFI_LOADED_IMAGE_PROTOCOL for the image>");
    DBG_EXIT_STATUS (Status);
    return;
  }

  GetHandleImageName(LoadedImage->ParentHandle, ParentImageName);
  DBG_EXIT ();
}

// -----------------------------------------------------------------------------
/**
 * Возвращает TRUE если это хэндл исполняемого образа.
*/
BOOLEAN
IsHandleImage (
  IN  EFI_HANDLE Handle
  )
{
  DBG_ENTER ();

  EFI_STATUS Status;
  Status = gBS->OpenProtocol (
                  Handle,
                  &gEfiLoadedImageProtocolGuid,
                  NULL,
                  gImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_TEST_PROTOCOL
                  );

  DBG_EXIT ();
  return !EFI_ERROR (Status);
}

// -----------------------------------------------------------------------------
/**
 * По хэндлу образа находит его имя.
 * Указатель, возвращённый через OUT-параметр впоследствии требуется освободить.
*/
VOID
GetHandleImageName (
  IN  EFI_HANDLE Handle,
  OUT CHAR16     **ImageName
  )
{
  DBG_ENTER ();

  EFI_STATUS Status;

  EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
  Status = gBS->OpenProtocol (
                  Handle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID **)&LoadedImage,
                  gImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    *ImageName = StrAllocCopy (L"<ERROR: can\'t open the EFI_LOADED_IMAGE_PROTOCOL for the image>");
    DBG_EXIT_STATUS (Status);
    return;
  }

  *ImageName = FindLoadedImageFileName(LoadedImage);
  if (*ImageName == NULL) {
    *ImageName = ConvertDevicePathToText(LoadedImage->FilePath, TRUE, TRUE);
  }
  if (*ImageName == NULL) {
    *ImageName = StrAllocCopy (L"<ERROR: can\'t find the image name>");
    DBG_EXIT_STATUS (EFI_UNSUPPORTED);
    return;
  }

  DBG_EXIT ();
}

// -----------------------------------------------------------------------------
/**
 * Выделяет память, копирует в неё аргумент, и возвращает указатель на память.
 * В случае неудачи возвращает NULL.
*/
CHAR16 *
StrAllocCopy (
  IN CHAR16 *Str
  )
{
  DBG_ENTER ();

  UINTN Len = StrLen (Str);
  UINTN BufferLen = Len + 1; // + 1 под L'\0'
  CHAR16 *StrCopy = NULL;

  EFI_STATUS Status;
  Status = gBS->AllocatePool (
                  EfiBootServicesData,
                  BufferLen * sizeof(CHAR16),
                  (VOID **)&StrCopy
                  );
  if (EFI_ERROR (Status)) {
    DBG_EXIT_STATUS (Status);
    return NULL;
  }

  Status = StrCpyS (StrCopy, BufferLen, Str);
  if (EFI_ERROR (Status)) {
    SHELL_FREE_NON_NULL (StrCopy)
    DBG_EXIT_STATUS (Status);
    return NULL;
  }

  DBG_EXIT_STATUS (EFI_SUCCESS);
  return StrCopy;
}

// -----------------------------------------------------------------------------
/**
 * Добавляет к первой строке вторую, выделяя память под результат и освобождая предыдущую память под StrHead.
 * В случае неудачи оставляет без изменений.
*/
VOID
StrAllocAppend (
  IN OUT CHAR16 **StrHead,
  IN     CHAR16 *StrTail
  )
{
  DBG_ENTER ();

  if (StrHead == NULL || *StrHead == NULL || StrTail == NULL) {
    DBG_EXIT_STATUS (EFI_INVALID_PARAMETER);
    return;
  }

  UINTN HeadLen   = StrLen (*StrHead);
  UINTN TailLen   = StrLen (StrTail);
  UINTN TotalLen  = HeadLen + TailLen;
  UINTN BufferLen = TotalLen + 1; // +1 под L'\0'
  CHAR16 *NewStr = NULL;

  EFI_STATUS Status;
  Status = gBS->AllocatePool (
                  EfiBootServicesData,
                  BufferLen * sizeof(CHAR16),
                  (VOID **)&NewStr
                  );
  if (EFI_ERROR (Status)) {
    DBG_EXIT_STATUS (Status);
    return;
  }

  Status = StrCpyS (NewStr, BufferLen, *StrHead);
  if (EFI_ERROR (Status)) {
    SHELL_FREE_NON_NULL (NewStr)
    DBG_EXIT_STATUS (Status);
    return;
  }

  Status = StrCpyS (NewStr + HeadLen, BufferLen - HeadLen, StrTail);
  if (EFI_ERROR (Status)) {
    SHELL_FREE_NON_NULL (NewStr)
    DBG_EXIT_STATUS (Status);
    return;
  }

  gBS->FreePool (*StrHead);
  *StrHead = NewStr;
  DBG_EXIT ();
}

// -----------------------------------------------------------------------------
/**
 * Возвращает по хэндлу его более информативное описание.
 * Не забыть освободить память из-под возвращаемого значения!
*/
CHAR16 *GetHandleName (
  EFI_HANDLE Handle
  )
{
  static CHAR16 Buffer[GET_HANDLE_NAME_BUFFER_SIZE];

  // Образ?
  if (IsHandleImage (Handle)) {
    CHAR16 *Str = NULL;
    GetHandleImageName (
      Handle,
      &Str
      );

    return Str;
  }

  // Контроллер физического устройства?
  EFI_STATUS Status;
  EFI_DEVICE_PATH_PROTOCOL *DevPath;
  Status = gBS->OpenProtocol (
                  Handle,
                  &gEfiDevicePathProtocolGuid,
                  (VOID**)&DevPath,
                  gImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (!EFI_ERROR (Status)) {
    CHAR16 *DevPathStr = ConvertDevicePathToText (DevPath, FALSE, FALSE);
    UnicodeSPrint(Buffer, GET_HANDLE_NAME_BUFFER_SIZE * sizeof(CHAR16), L"[dev: %s]", DevPathStr);
    gBS->FreePool (DevPathStr);
    return StrAllocCopy (Buffer);
  }

  // Сервисный/етц.
  UnicodeSPrint(Buffer, GET_HANDLE_NAME_BUFFER_SIZE * sizeof(CHAR16), L"[unk: %p]", Handle);
  return StrAllocCopy (Buffer);
}

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
  )
{
  DBG_ENTER ();

  EFI_GUID                       *NameGuid;
  EFI_STATUS                     Status;
  EFI_FIRMWARE_VOLUME2_PROTOCOL  *Fv;
  VOID                           *Buffer;
  UINTN                          BufferSize;
  UINT32                         AuthenticationStatus;

  if ((LoadedImage == NULL) || (LoadedImage->FilePath == NULL)) {
    DBG_EXIT_STATUS (EFI_INVALID_PARAMETER);
    return NULL;
  }

  NameGuid = EfiGetNameGuidFromFwVolDevicePathNode((MEDIA_FW_VOL_FILEPATH_DEVICE_PATH *)LoadedImage->FilePath);

  if (NameGuid == NULL) {
    DBG_EXIT_STATUS (EFI_VOLUME_CORRUPTED);
    return NULL;
  }

  //
  // Get the FirmwareVolume2Protocol of the device handle that this image was loaded from.
  //
  Status = gBS->HandleProtocol (LoadedImage->DeviceHandle, &gEfiFirmwareVolume2ProtocolGuid, (VOID**) &Fv);

  //
  // FirmwareVolume2Protocol is PI, and is not required to be available.
  //
  if (EFI_ERROR (Status)) {
    DBG_EXIT_STATUS (Status);
    return NULL;
  }

  //
  // Read the user interface section of the image.
  //
  Buffer = NULL;
  Status = Fv->ReadSection(Fv, NameGuid, EFI_SECTION_USER_INTERFACE, 0, &Buffer, &BufferSize, &AuthenticationStatus);

  if (EFI_ERROR (Status)) {
    DBG_EXIT_STATUS (Status);
    return NULL;
  }

  //
  // ReadSection returns just the section data, without any section header. For
  // a user interface section, the only data is the file name.
  //
  DBG_EXIT_STATUS (EFI_SUCCESS);
  return Buffer;
}

// -----------------------------------------------------------------------------