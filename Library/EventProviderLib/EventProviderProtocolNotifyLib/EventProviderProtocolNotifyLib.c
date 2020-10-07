#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>

#include <Library/EventProviderUtilityLib.h>
#include <Library/EventProviderLib.h>
#include <Library/VectorLib.h>
#include <Library/ProtocolGuidDatabaseLib.h>
#include <Library/CommonMacrosLib.h>

// -----------------------------------------------------------------------------
#define CHECK_PROTOCOL_EXISTENCE_BUFFER_SIZE 128

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

  if (This == NULL) {
    DBG_EXIT_STATUS (EFI_INVALID_PARAMETER);
    return EFI_INVALID_PARAMETER;
  }

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

    This->AddEvent (This->ExternalData, &Event);
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
  UINTN       HandleCount    = 0;
  EFI_HANDLE  *Handles       = NULL;
  EFI_HANDLE  ProtocolHandle = NULL;

  while (TRUE) {
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

    ProtocolHandle = *Handles;
    SHELL_FREE_NON_NULL (Handles);
  }

  LOADING_EVENT Event;
  STATIC CHAR16 *Failed = L"<ERROR: can\'t get protocol handle>";

  if (CompareGuid (Guid, &gEfiLoadedImageProtocolGuid)) {
    Event.Type = LOG_ENTRY_TYPE_IMAGE_LOADED;

    if (ProtocolHandle == NULL) {
      // Ошибка во время детекта новых образов.
      Event.ImageLoaded.ImageName       = StrAllocCopy (Failed);
      Event.ImageLoaded.ParentImageName = NULL;
    } else if (ProtocolHandle == gImageHandle) {
      // Не логируем событие загрузки собственного образа.
      DBG_EXIT ();
      return;
    } else {
      // Ок.
      GetHandleImageNameAndParentImageName (
        ProtocolHandle,
        &Event.ImageLoaded.ImageName,
        &Event.ImageLoaded.ParentImageName
        );
    }
  } else {  // <- if (CompareGuid (Guid, &gEfiLoadedImageProtocolGuid)) { ... }
    Event.Type                                      = LOG_ENTRY_TYPE_PROTOCOL_INSTALLED;
    Event.ProtocolInstalled.Guid                    = *Guid;
    Event.ProtocolInstalled.Successful              = TRUE; // Если нас вызвали, то протокол успешно установлен.

    if (ProtocolHandle == NULL) {
      Event.ProtocolInstalled.HandleDescription = StrAllocCopy (Failed);
    } else {
      Event.ProtocolInstalled.HandleDescription = GetHandleName (ProtocolHandle);
    }
  }

  This->AddEvent (This->ExternalData, &Event);
  DBG_EXIT ();
}

// -----------------------------------------------------------------------------
