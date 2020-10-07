#include <Uefi.h>
#include <Library/CommonMacrosLib.h>
#include <Library/EventLoggerLib.h>
#include <Library/EventProviderLib.h>
#include <Library/ProtocolGuidDatabaseLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiLib.h>

#define DBG_STR_NO_NULL(Pointer) ((Pointer) ? (Pointer) : (L"<NONE>"))

// -----------------------------------------------------------------------------
/**
 * Функции обратного вызова, вызов происходит при поступлении события.
*/
VOID
AddEventToLog (
  IN OUT VOID           *Logger,
  IN     LOADING_EVENT  *Event
  );

// -----------------------------------------------------------------------------
/**
 * Инициализирует структуру LOGGER.
 * Функция должна быть обязательно однократно вызвана перед использованием объекта.
 *
 * @param This                      Указатель на структуру логгера, для которой выполняется инициализация.
 * @param EventIncomed              Функция. которая будет вызываться сразу при поступлении события.
 *                                  NULL, если не нужно.
 *
 * @retval EFI_SUCCESS              Операция завершена успешно.
 * @retval Любое другое значение    Произошла ошибка, объект не инициализирован.
 */
EFI_STATUS
Logger_Construct (
  IN OUT LOGGER              *This,
  IN     EVENT_INCOMED_FUNC  EventIncomed  OPTIONAL
  )
{
  DBG_ENTER();

  This->EventIncomedCallback = EventIncomed;

  EFI_STATUS Status;

  Status = Vector_Construct (
            &This->LogData,
            sizeof(LOADING_EVENT),
            1024
            );
  RETURN_ON_ERR(Status)

  Status = EventProvider_Construct (
            &This->EventProvider,
            &AddEventToLog,
            This
            );
  RETURN_ON_ERR(Status)

  DBG_EXIT_STATUS (EFI_SUCCESS);
  return EFI_SUCCESS;
}

// -----------------------------------------------------------------------------
/**
 * Освобождает память из-под структуры.
 * Функция должна быть обязательно однократно вызвана после завершения использования объекта.
 *
 * @param This                      Указатель на структуру логгера.
 */
VOID
Logger_Destruct (
  LOGGER *This
  )
{
  DBG_ENTER ();

  Logger_Stop(This);

  FOR_EACH_VCT (LOADING_EVENT, Event, This->LogData) {
    LoadingEvent_Destruct (Event);
  }
  Vector_Destruct (&This->LogData);

  EventProvider_Destruct (&This->EventProvider);

  DBG_EXIT ();
}

// -----------------------------------------------------------------------------
/**
 * Логгер коннектится к системе и начинает сбор событий.
 *
 * @param This                      Указатель на структуру логгера.
 *
 * @retval EFI_SUCCESS              Логгер успешно законнектился.
 * @retval Любое другое значение    Произошла ошибка, логгер в том же состоянии что и до вызова функции.
 */
EFI_STATUS
Logger_Start (
  LOGGER *This
  )
{
  DBG_ENTER ();

  EFI_STATUS Status;

  Status = EventProvider_Start (&This->EventProvider);
  RETURN_ON_ERR(Status)

  DBG_EXIT_STATUS (EFI_SUCCESS);
  return EFI_SUCCESS;
}

// -----------------------------------------------------------------------------
/**
 * Отсоединяется от системы и прекращает сбор событий.
 * Уже собранные события остаются доступны.
 *
 * @param This                      Указатель на структуру логгера.
 */
VOID
Logger_Stop (
  LOGGER *This
  )
{
  DBG_ENTER ();

  EventProvider_Stop (&This->EventProvider);

  DBG_EXIT ();
}

// -----------------------------------------------------------------------------
/**
 * Функции обратного вызова, вызов происходит при поступлении события.
*/
VOID
AddEventToLog (
  IN OUT VOID           *Logger,
  IN     LOADING_EVENT  *Event
  )
{
  DBG_ENTER ();
  EFI_TPL OldTpl = gBS->RaiseTPL (TPL_HIGH_LEVEL);

  LOGGER *This = (LOGGER *)Logger;

  Vector_PushBack (&This->LogData, Event);

  if (This->EventIncomedCallback != NULL) {
    This->EventIncomedCallback ();
  }

  if (FeaturePcdGet (PcdPrintEventNumbersToConsole)) {
    // Два данных типа событий происходят до нашей загрузки, поэтому нет смысла писать о них в консоль.
    // Ведь пишем мы в консоль только для того чтобы сопоставить момент возникновения события с выводом на экран.
    if (gST->ConOut
      && Event->Type != LOG_ENTRY_TYPE_PROTOCOL_EXISTS_ON_STARTUP
      && Event->Type != LOG_ENTRY_TYPE_IMAGE_EXISTS_ON_STARTUP) {
        Print(L"---- [event #%u] ----\n", (unsigned) Vector_Size (&This->LogData));
    }
  }

  DBG_INFO1 ("---- Event received: --------------------------------------\n");
  DEBUG_CODE_BEGIN ();

  CHAR16 *GuidName = NULL;

  switch (Event->Type)
  {
  case LOG_ENTRY_TYPE_PROTOCOL_INSTALLED:
    GuidName = GetProtocolName (&Event->ProtocolInstalled.Guid);
    DBG_INFO1 ("Type:             LOG_ENTRY_TYPE_PROTOCOL_INSTALLED\n");
    if (GuidName == NULL) {
      DBG_INFO  ("Guid:             %g\n", &Event->ProtocolInstalled.Guid);
    } else {
      DBG_INFO  ("Guid:             %s\n", GuidName);
    }
    DBG_INFO  ("Handle:   %s\n", DBG_STR_NO_NULL (Event->ProtocolInstalled.HandleDescription));
    if (Event->ProtocolInstalled.Successful) {
      DBG_INFO1 ("Successful:       TRUE\n");
    } else {
      DBG_INFO1 ("Successful:       FALSE\n");
    }
    break;

  case LOG_ENTRY_TYPE_PROTOCOL_EXISTS_ON_STARTUP:
    GuidName = GetProtocolName(&Event->ProtocolExistsOnStartup.Guid);
    DBG_INFO1 ("Type:             LOG_ENTRY_TYPE_PROTOCOL_EXISTS_ON_STARTUP\n");
    if (GuidName == NULL) {
      DBG_INFO  ("Guid:             %g\n", &Event->ProtocolExistsOnStartup.Guid);
    } else {
      DBG_INFO  ("Guid:             %s\n", GuidName);
    }
    DBG_INFO  ("Handles:     %s\n", DBG_STR_NO_NULL (Event->ProtocolExistsOnStartup.HandleDescription));
    break;

  case LOG_ENTRY_TYPE_PROTOCOL_REMOVED:
    GuidName = GetProtocolName(&Event->ProtocolRemoved.Guid);
    DBG_INFO1 ("Type:             LOG_ENTRY_TYPE_PROTOCOL_REMOVED\n");
    if (GuidName == NULL) {
      DBG_INFO  ("Guid:             %g\n", &Event->ProtocolRemoved.Guid);
    } else {
      DBG_INFO  ("Guid:             %s\n", GuidName);
    }
    DBG_INFO  ("Handle:   %s\n", DBG_STR_NO_NULL (Event->ProtocolInstalled.HandleDescription));
    if (Event->ProtocolRemoved.Successful) {
      DBG_INFO1 ("Successful:       TRUE\n");
    } else {
      DBG_INFO1 ("Successful:       FALSE\n");
    }
    break;

  case LOG_ENTRY_TYPE_IMAGE_LOADED:
    DBG_INFO1 ("Type:             LOG_ENTRY_TYPE_IMAGE_LOADED\n");
    DBG_INFO  ("ImageName:        %s\n", DBG_STR_NO_NULL (Event->ImageLoaded.ImageName));
    DBG_INFO  ("ParentImageName:  %s\n", DBG_STR_NO_NULL (Event->ImageLoaded.ParentImageName));
    break;

  case LOG_ENTRY_TYPE_IMAGE_EXISTS_ON_STARTUP:
    DBG_INFO1 ("Type:             LOG_ENTRY_TYPE_IMAGE_EXISTS_ON_STARTUP\n");
    DBG_INFO  ("ImageName:        %s\n", DBG_STR_NO_NULL (Event->ImageExistsOnStartup.ImageName));
    DBG_INFO  ("ParentImageName:  %s\n", DBG_STR_NO_NULL (Event->ImageExistsOnStartup.ParentImageName));
    break;

  case LOG_ENTRY_TYPE_BDS_STAGE_ENTERED:
    DBG_INFO1 ("Type:             LOG_ENTRY_TYPE_BDS_STAGE_ENTERED\n");
    switch (Event->BdsStageEntered.SubEvent) {
    case BDS_STAGE_EVENT_BEFORE_ENTRY_CALLING:
      DBG_INFO1 ("SubType:          BEFORE\n");
      break;
    case BDS_STAGE_EVENT_AFTER_ENTRY_CALLING:
      DBG_INFO1 ("SubType:          AFTER\n");
      break;
    default:
      DBG_INFO1 ("ERROR: Unknown SubEvent type\n");
      break;
    }
    break;

  case LOG_ENTRY_TYPE_ERROR:
    DBG_INFO1 ("Type:             LOG_ENTRY_TYPE_ERROR\n");
    DBG_INFO  ("Message:          %s\n", DBG_STR_NO_NULL (Event->Error.Message));
    break;

  default:
    DBG_INFO1 ("ERROR: Unknown event type\n");
    break;
  }

  DEBUG_CODE_END ();
  DBG_INFO1 ("-----------------------------------------------------------\n");

  gBS->RestoreTPL (OldTpl);
  DBG_EXIT ();
}

// -----------------------------------------------------------------------------
/**
 * Возвращает текущее количество находящихся в логе событий.
 */
UINTN
Logger_GetEventCount(
  IN LOGGER *This
  )
{
  DBG_ENTER ();
  EFI_TPL OldTpl = gBS->RaiseTPL (TPL_HIGH_LEVEL);
  UINTN EventCount = Vector_Size (&This->LogData);
  gBS->RestoreTPL (OldTpl);

  DBG_EXIT ();
  return EventCount;
}

// -----------------------------------------------------------------------------
/**
 * Возвращает находящееся в логе событие с индексом Index.
 *
 * @param  This    Структура лога, над которой выполняется действие.
 * @param  Index   Индекс события в логе, должен быть в пределах [0, Logger_GetEventCount() - 1]
 * @param  Event   Записывает туда указанное событие.
 *
 * @retval EFI_SUCCESS           Операция завершена успешно.
 * @return EFI_INVALID_PARAMETER Index >= Logger_GetEventCount()
 */
EFI_STATUS
Logger_GetEvent(
  IN  LOGGER        *This,
  IN  UINTN         Index,
  OUT LOADING_EVENT *ResultEvent
  )
{
  DBG_ENTER ();

  EFI_STATUS Status;
  EFI_TPL OldTpl = gBS->RaiseTPL (TPL_HIGH_LEVEL);

  LOADING_EVENT *Event = Vector_Get(&This->LogData, Index);
  if (Event == NULL) {
    Status = EFI_INVALID_PARAMETER;
  } else {
    *ResultEvent = *Event;
    Status = EFI_SUCCESS;
  }

  gBS->RestoreTPL (OldTpl);
  DBG_EXIT_STATUS (Status);
  return Status;
}

// -----------------------------------------------------------------------------
