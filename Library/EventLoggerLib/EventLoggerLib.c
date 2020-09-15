#include <Uefi.h>
#include <Library/EventLoggerLib.h>
#include <Library/EventProviderLib.h>
#include <Library/CommonMacros.h>
#include <Library/UefiBootServicesTableLib.h>

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
 * @param This                      Указатель на структуру логгера.
 *
 * @retval EFI_SUCCESS              Операция завершена успешно.
 * @retval Любое другое значение    Произошла ошибка, объект не инициализирован.
 */
EFI_STATUS
Logger_Construct (
  LOGGER *This
  )
{
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
  Logger_Stop(This);

  Vector_Destruct (&This->LogData);
  EventProvider_Destruct (&This->EventProvider);
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
  EFI_STATUS Status;

  Status = EventProvider_Start (&This->EventProvider);
  RETURN_ON_ERR(Status)

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
  EventProvider_Stop (&This->EventProvider);
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
  LOGGER *This = (LOGGER *)Logger;

  EFI_TPL OldTpl = gBS->RaiseTPL (TPL_HIGH_LEVEL);
  Vector_PushBack (&This->LogData, Event);
  gBS->RestoreTPL (OldTpl);
}

// -----------------------------------------------------------------------------
