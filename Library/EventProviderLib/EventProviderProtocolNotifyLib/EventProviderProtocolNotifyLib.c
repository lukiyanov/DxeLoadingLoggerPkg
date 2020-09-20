#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiLib.h>
#include <Library/PrintLib.h>
#include <Library/DevicePathLib.h>
#include <Pi/PiFirmwareFile.h>
#include <Pi/PiFirmwareVolume.h>

#include <Protocol/FirmwareVolume2.h>
#include <Protocol/LoadedImage.h>

#include <Library/EventProviderLib.h>
#include <Library/VectorLib.h>
#include <Library/ProtocolGuidDatabaseLib.h>
#include <Library/CommonMacrosLib.h>
#include <Library/HandleDatabaseDumpLib.h>

// -----------------------------------------------------------------------------
#define CHECK_PROTOCOL_EXISTENCE_BUFFER_SIZE 128
#define GET_HANDLE_NAME_BUFFER_SIZE          1024

// -----------------------------------------------------------------------------
/// EVENT_PROVIDER -> Data
typedef struct
{
  // Хэндлы создаваемых нами событий и, соответственно, которые мы должны удалить.
  VECTOR TYPE (EFI_EVENT) EventHandles;

  // Структуры, передающиеся в ProtocolInstalledCallback().
  VECTOR TYPE (NOTIFY_FUNCTION_CONTEXT) Contexts;
} EVENT_PROVIDER_DATA_STRUCT;

// -----------------------------------------------------------------------------
/// VOID *Context
typedef struct
{
  EVENT_PROVIDER *This;
  EFI_GUID       *Guid;
  VOID           *Registration;
} NOTIFY_FUNCTION_CONTEXT;


// -----------------------------------------------------------------------------
/**
 * Создаёт по LOG_ENTRY_TYPE_IMAGE_EXISTS_ON_STARTUP для каждого образа, уже загруженного в момент нашего запуска.
*/
EFI_STATUS
DetectImagesLoadedOnStartup (
  IN OUT EVENT_PROVIDER *This
  );

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
 * Начинает отслеживание установки новых экземпляров протокола.
*/
EFI_STATUS
SubscribeToProtocolInstallation (
  IN OUT EVENT_PROVIDER  *This,
  IN     EFI_GUID        *ProtocolGuid
  );

// -----------------------------------------------------------------------------
/**
 * Вызывается при установке протокола в БД.
*/
VOID
EFIAPI
ProtocolInstalledCallback (
  IN  EFI_EVENT  Event,
  IN  VOID       *Context
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
/**
 * Инициализирует структуру EVENT_PROVIDER.
 * Функция должна быть обязательно однократно вызвана перед использованием объекта.
 *
 * @param This                      Указатель на структуру EVENT_PROVIDER, для которой выполняется инициализация.
 * @param AddEvent                  Функция обратного вызова, EVENT_PROVIDER вызывает её при поступлении новых событий.
 * @param ExternalData              Передаётся в AddEvent при каждом вызове.
 *
 * @retval EFI_SUCCESS              Операция завершена успешно.
 * @retval Любое другое значение    Произошла ошибка, объект не инициализирован.
 */
EFI_STATUS
EventProvider_Construct(
  IN OUT EVENT_PROVIDER  *This,
  IN     ADD_EVENT       AddEvent,
  IN     VOID            *ExternalData
  )
{
  DBG_ENTER ();

  This->AddEvent     = AddEvent;
  This->ExternalData = ExternalData;

  EFI_STATUS Status;
  Status = gBS->AllocatePool (
                  EfiBootServicesData,
                  sizeof(EVENT_PROVIDER_DATA_STRUCT),
                  &This->Data
                  );
  RETURN_ON_ERR (Status)

  EVENT_PROVIDER_DATA_STRUCT *DataStruct = (EVENT_PROVIDER_DATA_STRUCT *)This->Data;

  UINTN KnownProtocolCount = GetProtocolGuidCount();

  Status = Vector_Construct (
            &DataStruct->EventHandles,
            sizeof(EFI_EVENT),
            KnownProtocolCount
            );
  RETURN_ON_ERR (Status)

  Status = Vector_Construct (
            &DataStruct->Contexts,
            sizeof(NOTIFY_FUNCTION_CONTEXT),
            KnownProtocolCount
            );
  RETURN_ON_ERR (Status)

  DBG_EXIT_STATUS (EFI_SUCCESS);
  return EFI_SUCCESS;
}

// -----------------------------------------------------------------------------
/**
 * Освобождает память из-под структуры.
 * Функция должна быть обязательно однократно вызвана после завершения использования объекта.
 */
VOID
EventProvider_Destruct (
  IN OUT  EVENT_PROVIDER  *This
  )
{
  DBG_ENTER ();

  EventProvider_Stop (This);

  if (This && This->Data) {
    EVENT_PROVIDER_DATA_STRUCT *DataStruct = (EVENT_PROVIDER_DATA_STRUCT *)This->Data;
    Vector_Destruct (&DataStruct->EventHandles);
    Vector_Destruct (&DataStruct->Contexts);

    gBS->FreePool (This->Data);
    This->Data = 0;
  }

  DBG_EXIT ();
}

// -----------------------------------------------------------------------------
/**
 * EVENT_PROVIDER коннектится к системе и начинает сбор событий.
 *
 * @retval EFI_SUCCESS              EVENT_PROVIDER успешно законнектился.
 * @retval Любое другое значение    Произошла ошибка, объект в том же состоянии что и до вызова функции.
 */
EFI_STATUS
EventProvider_Start (
  IN OUT EVENT_PROVIDER  *This
  )
{
  DBG_ENTER ();

  if (This == NULL || This->Data == NULL) {
    DBG_EXIT_STATUS (EFI_INVALID_PARAMETER);
    return EFI_INVALID_PARAMETER;
  }

  EFI_STATUS Status;
 Status = DetectImagesLoadedOnStartup (This);
 RETURN_ON_ERR (Status)

  UINTN KnownProtocolGuidCount = GetProtocolGuidCount();

  for (UINTN Index = 0; Index < KnownProtocolGuidCount; ++Index) {
    EFI_GUID *Guid = GetProtocolGuid(Index);
    CheckProtocolExistenceOnStartup (This, Guid);
  }

  for (UINTN Index = 0; Index < KnownProtocolGuidCount; ++Index) {
    EFI_GUID *Guid = GetProtocolGuid(Index);
    Status = SubscribeToProtocolInstallation (This, Guid);
    RETURN_ON_ERR (Status)
  }

  DBG_EXIT_STATUS (EFI_SUCCESS);
  return EFI_SUCCESS;
}

// -----------------------------------------------------------------------------
/**
 * EVENT_PROVIDER отсоединяется от системы и прекращает сбор событий.
 */
VOID
EventProvider_Stop (
  IN OUT EVENT_PROVIDER  *This
  )
{
  DBG_ENTER ();

  if (This == NULL || This->Data == NULL) {
    DBG_EXIT_STATUS (EFI_INVALID_PARAMETER);
    return;
  }

  // Просто уничтожаем все события.
  EVENT_PROVIDER_DATA_STRUCT *DataStruct = (EVENT_PROVIDER_DATA_STRUCT *)This->Data;

  FOR_EACH_VCT (EFI_EVENT, Event, DataStruct->EventHandles) {
    gBS->CloseEvent (*Event);
  }
  Vector_Clear (&DataStruct->EventHandles);

  DBG_EXIT ();
}

// -----------------------------------------------------------------------------
/**
 * Создаёт по LOG_ENTRY_TYPE_IMAGE_EXISTS_ON_STARTUP для каждого образа, уже загруженного в момент нашего запуска.
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

    This->AddEvent(This->ExternalData, &Event);
  }

  gBS->FreePool (Handles);
  DBG_EXIT_STATUS (EFI_SUCCESS);
  return EFI_SUCCESS;
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

  CHAR16 *ImageNames = NULL;

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
    ImageNames = StrAllocCopy (L"<ERROR: can\t get handle buffer for protocol>");
  } else {
    ImageNames = StrAllocCopy (L"{ ");

    UINTN NotImageHandleCount = 0;

    for (UINTN Index = 0; Index < HandleCount; ++Index) {
      CHAR16 *CurrentImageName = NULL;

      if (IsHandleImage (Handles[Index])) {
        GetHandleImageName (
          Handles[Index],
          &CurrentImageName
          );

        StrAllocAppend(&ImageNames, CurrentImageName);

        if (NotImageHandleCount || Index < HandleCount - 1) {
          StrAllocAppend(&ImageNames, L", ");
        }
      } else {
        NotImageHandleCount++;
      }

      SHELL_FREE_NON_NULL (CurrentImageName);
    }

    if (NotImageHandleCount) {
      static CHAR16 Buffer[CHECK_PROTOCOL_EXISTENCE_BUFFER_SIZE];
      UnicodeSPrint(Buffer, CHECK_PROTOCOL_EXISTENCE_BUFFER_SIZE * sizeof(CHAR16), L"[not images: %u]", (unsigned)NotImageHandleCount);

      StrAllocAppend(&ImageNames, Buffer);
    }

    StrAllocAppend(&ImageNames, L" }");
    SHELL_FREE_NON_NULL (Handles);
  }

  LOADING_EVENT  Event;
  Event.Type                               = LOG_ENTRY_TYPE_PROTOCOL_EXISTS_ON_STARTUP;
  Event.ProtocolExistsOnStartup.Guid       = *Protocol;
  Event.ProtocolExistsOnStartup.ImageNames = ImageNames;

  This->AddEvent(This->ExternalData, &Event);

  DBG_EXIT ();
}

// -----------------------------------------------------------------------------
/**
 * Начинает отслеживание установки новых экземпляров протокола.
*/
EFI_STATUS
SubscribeToProtocolInstallation (
  IN OUT EVENT_PROVIDER  *This,
  IN     EFI_GUID        *ProtocolGuid
  )
{
  DBG_ENTER ();
  DBG_INFO ("-- Subscribing for %g\n", ProtocolGuid);

  EFI_STATUS   Status;
  EFI_EVENT    EventProtocolAppeared;

  EVENT_PROVIDER_DATA_STRUCT *DataStruct = (EVENT_PROVIDER_DATA_STRUCT *)This->Data;
  {
    NOTIFY_FUNCTION_CONTEXT Context;
    Context.This         = This;
    Context.Guid         = ProtocolGuid;

    Status = Vector_PushBack (&DataStruct->Contexts, &Context);
    RETURN_ON_ERR (Status)
  }

  NOTIFY_FUNCTION_CONTEXT *ContextPointer = (NOTIFY_FUNCTION_CONTEXT *)Vector_GetLast (&DataStruct->Contexts);

  Status = gBS->CreateEvent (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  ProtocolInstalledCallback,
                  ContextPointer,
                  &EventProtocolAppeared
                  );
  RETURN_ON_ERR (Status)

  Status = gBS->RegisterProtocolNotify (
                  ProtocolGuid,
                  EventProtocolAppeared,
                  &ContextPointer->Registration
                  );
  RETURN_ON_ERR (Status)

  Status = Vector_PushBack (&DataStruct->EventHandles, &EventProtocolAppeared);
  RETURN_ON_ERR (Status)

  DBG_EXIT_STATUS (EFI_SUCCESS);
  return EFI_SUCCESS;
}

// -----------------------------------------------------------------------------
/**
 * Вызывается при установке протокола в БД.
*/
VOID
EFIAPI
ProtocolInstalledCallback (
  IN  EFI_EVENT  EfiEvent,
  IN  VOID       *Context
  )
{
  DBG_ENTER ();

  NOTIFY_FUNCTION_CONTEXT *ContextData = (NOTIFY_FUNCTION_CONTEXT *)Context;
  EVENT_PROVIDER *This = ContextData->This;
  EFI_GUID       *Guid = ContextData->Guid;
  VOID           *Reg  = ContextData->Registration;

  // Получаем хэндл, на который установлен протокол.
  // LocateHandleBuffer () возвращает по одному хэндлу за раз,
  // при этом нам нужен последний из них.
  EFI_STATUS  Status;
  UINTN       HandleCount     = 0;
  EFI_HANDLE  *Handles        = NULL;
  EFI_HANDLE  *ProtocolHandle = NULL;

  while (TRUE) {
    ProtocolHandle = Handles;

    Status = gBS->LocateHandleBuffer (
                    ByRegisterNotify,
                    NULL,
                    Reg,
                    &HandleCount,
                    &Handles
                    );
    if (EFI_ERROR (Status)) {
      break;
    }
  }

  LOADING_EVENT Event;
  STATIC CHAR16 *Failed = L"<ERROR: can\'t get protocol handle>";

  if (CompareGuid (Guid, &gEfiLoadedImageProtocolGuid)) {
    Event.Type = LOG_ENTRY_TYPE_IMAGE_LOADED;

    if (ProtocolHandle == NULL) {
      // Ошибка во время детекта новых образов.
      Event.ImageLoaded.ImageName       = StrAllocCopy (Failed);
      Event.ImageLoaded.ParentImageName = NULL;
    } else {
      // Ок.
      GetHandleImageNameAndParentImageName (
        *ProtocolHandle,
        &Event.ImageLoaded.ImageName,
        &Event.ImageLoaded.ParentImageName
        );
    }
  } else {  // <- if (CompareGuid (Guid, &gEfiLoadedImageProtocolGuid)) { ... }
    Event.Type                                      = LOG_ENTRY_TYPE_PROTOCOL_INSTALLED;
    Event.ProtocolInstalled.Guid                    = *Guid;
    Event.ProtocolInstalled.Successful              = TRUE; // Если нас вызвали, то протокол успешно установлен.
    Event.ProtocolInstalled.ImageNameWhoInstalled   = NULL; // Данная реализация EventProvider не детектит это.

    if (ProtocolHandle == NULL) {
      Event.ProtocolInstalled.ImageNameWhereInstalled = StrAllocCopy (Failed);
    } else {
      Event.ProtocolInstalled.ImageNameWhereInstalled = GetHandleName (*ProtocolHandle);
    }
  }

  This->AddEvent(This->ExternalData, &Event);
  SHELL_FREE_NON_NULL (Handles);
  DBG_EXIT ();
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

    UnicodeSPrint(Buffer, GET_HANDLE_NAME_BUFFER_SIZE * sizeof(CHAR16), L"[img: %s]", Str);
    gBS->FreePool (Str);
    return StrAllocCopy (Buffer);
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
