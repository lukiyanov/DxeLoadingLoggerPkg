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
#include <Library/HandleDatabaseDumpLib.h>

// -----------------------------------------------------------------------------
#define GET_HANDLE_NAME_BUFFER_SIZE          1024
#define CHECK_PROTOCOL_EXISTENCE_BUFFER_SIZE 128


// -----------------------------------------------------------------------------
/**
 * Если Protocol присутствует в БД, то функция генерит событие LOG_ENTRY_TYPE_PROTOCOL_EXISTS_ON_STARTUP.
*/
VOID
CheckProtocolExistenceOnStartup (
  IN  EVENT_PROVIDER  *This,
  IN  EFI_GUID        *Protocol
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
 * Создаёт по LOG_ENTRY_TYPE_PROTOCOL_EXISTS_ON_STARTUP для каждого протокола,
 * уже установленного в момент нашего запуска.
*/
EFI_STATUS
DetectProtocolsInstalledOnStartup (
  IN OUT EVENT_PROVIDER *This
  )
{
  DBG_ENTER ();
  EFI_STATUS  Status;

  HANDLE_DATABASE_DUMP HandleDbDump;
  Status = GetHandleDatabaseDump (&HandleDbDump);
  RETURN_ON_ERR (Status);

  DBG_INFO ("HANDLE count: %u\n", (unsigned)Vector_Size(&HandleDbDump));

  VECTOR TYPE (EFI_GUID) Protocols;
  Status = HandleDatabaseDump_PeekAllProtocols (&HandleDbDump, &Protocols);
  if (EFI_ERROR (Status)) {
    HandleDatabaseDump_Destruct (&HandleDbDump);
    DBG_EXIT_STATUS (Status);
    return Status;
  }

  DBG_INFO ("PROTOCOL count: %u\n", (unsigned)Vector_Size(&Protocols));

  FOR_EACH_VCT (EFI_GUID, ProtocolGuid, Protocols) {
    CheckProtocolExistenceOnStartup (This, ProtocolGuid);
  }

  Vector_Destruct (&Protocols);
  HandleDatabaseDump_Destruct (&HandleDbDump);

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
 * Если Protocol присутствует в БД, то функция генерит событие LOG_ENTRY_TYPE_PROTOCOL_EXISTS_ON_STARTUP.
*/
VOID
CheckProtocolExistenceOnStartup (
  IN EVENT_PROVIDER  *This,
  IN EFI_GUID        *Protocol
  )
{
  DBG_ENTER ();

  EFI_STATUS     Status;
  VOID           *Iface;

  Status = gBS->LocateProtocol (Protocol, NULL, &Iface);
  DBG_INFO ("-- Protocol %g exists: %u\n", Protocol, (unsigned)(!EFI_ERROR (Status)));

  if (EFI_ERROR (Status)) {
    DBG_EXIT_STATUS (EFI_NOT_FOUND);
    return;
  }

  CHAR16 *HandleDescription = NULL;

  EFI_HANDLE *Handles;
  UINTN      HandleCount;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  Protocol,
                  NULL,
                  &HandleCount,
                  &Handles
                  );
  if (EFI_ERROR (Status)) {
    HandleDescription = StrAllocCopy (L"<ERROR: can\t get handle buffer for protocol>");
  } else {
    static CHAR16 Buffer[CHECK_PROTOCOL_EXISTENCE_BUFFER_SIZE];
    UnicodeSPrint(Buffer, CHECK_PROTOCOL_EXISTENCE_BUFFER_SIZE * sizeof(CHAR16), L"[%3u] { ", (unsigned)HandleCount);
    HandleDescription = StrAllocCopy (L"");
    StrAllocAppend(&HandleDescription, Buffer);

    BOOLEAN First = TRUE;

    // Добавляем сначала образы.
    // Попутно считаем, сколько у нас хэндлов не-образов.
    UINTN NotImageHandleCount = 0;
    for (UINTN Index = 0; Index < HandleCount; ++Index) {
      if (IsHandleImage (Handles[Index])) {
        if (First) {
          First = FALSE;
        } else {
          StrAllocAppend(&HandleDescription, L", ");
        }

        CHAR16 *CurrentImageName = GetHandleName (Handles[Index]);
        StrAllocAppend(&HandleDescription, CurrentImageName);
        SHELL_FREE_NON_NULL (CurrentImageName);
      } else
      {
         ++NotImageHandleCount;
      }
    }

    // Затем всё остальное:
    if (NotImageHandleCount > 0) {
      // Если были образы, то после них переводим на новую строку.
      if (!First) {
          StrAllocAppend(&HandleDescription, L"; ");
      }

      UnicodeSPrint(Buffer, CHECK_PROTOCOL_EXISTENCE_BUFFER_SIZE * sizeof(CHAR16), L"not images (%u): ", (unsigned)NotImageHandleCount);
      StrAllocAppend(&HandleDescription, Buffer);

      First = TRUE;
      for (UINTN Index = 0; Index < HandleCount; ++Index) {
        if (!IsHandleImage (Handles[Index])) {
          if (First) {
            First = FALSE;
          } else {
            StrAllocAppend(&HandleDescription, L", ");
          }

          CHAR16 *CurrentImageName = GetHandleName (Handles[Index]);
          StrAllocAppend(&HandleDescription, CurrentImageName);
          SHELL_FREE_NON_NULL (CurrentImageName);
        }
      }
    }

    StrAllocAppend(&HandleDescription, L" }");
    SHELL_FREE_NON_NULL (Handles);
  }

  LOADING_EVENT  Event;
  Event.Type                               = LOG_ENTRY_TYPE_PROTOCOL_EXISTS_ON_STARTUP;
  Event.ProtocolExistsOnStartup.Guid       = *Protocol;
  Event.ProtocolExistsOnStartup.HandleDescription = HandleDescription;

  This->AddEvent (This->ExternalData, &Event);

  DBG_EXIT ();
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