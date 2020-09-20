#include <Uefi.h>
#include <Library/VectorLib.h>
#include <Library/CommonMacrosLib.h>

#ifndef HANDLE_DATABASE_DUMP_H_
#define HANDLE_DATABASE_DUMP_H_


//------------------------------------------------------------------------------
typedef struct
{
  EFI_HANDLE              Handle;
  VECTOR TYPE (EFI_GUID)  InstalledProtocolGuids;
} HANDLE_DATABASE_ENTRY;

typedef VECTOR TYPE (HANDLE_DATABASE_ENTRY)  HANDLE_DATABASE_DUMP;


//------------------------------------------------------------------------------
/**
 * Генерит HANDLE_DATABASE_DUMP из текущего состояния системы.
*/
EFI_STATUS
GetHandleDatabaseDump (
  OUT HANDLE_DATABASE_DUMP *Dump
  );

//------------------------------------------------------------------------------
/**
 * Корректно уничтожает HANDLE_DATABASE_DUMP.
*/
VOID
HandleDatabaseDump_Destruct (
  IN OUT HANDLE_DATABASE_DUMP *Dump
  );

//------------------------------------------------------------------------------
/**
 * Возвращает хэндлы из Dump, на которые установлен протокол ProtocolGuid.
 *
 * @param Dump              Дамп, в котором мы ищем хэндлы.
 * @param ProtocolGuid      GUID искомого протокола.
 * @param Handles           Вектор с хэндлами, на которые установлен протокол.
 *                          Его НЕ нужно заранее инициализировать функцией Vector_Create ().
 *                          Не забыть корректно уничтожить его.
 *
 * @param EFI_SUCCESS       Всё ок, результат в Handles.
 * @param Что-то другое     Операция не удалась.
*/
EFI_STATUS
HandleDatabaseDump_PeekHandlesWithProtocol (
  IN  HANDLE_DATABASE_DUMP     *Dump,
  IN  EFI_GUID                 *ProtocolGuid,
  OUT VECTOR TYPE (EFI_HANDLE) *Handles
  );

//------------------------------------------------------------------------------
/**
 * Возвращает те хэндлы из DumpNew, на которые установлен протокол ProtocolGuid
 * и которых при этом не был установлен протокол ProtocolGuid в DumpOld.
 *
 * @param HandlesOld        Старый набор хэндлов.
 * @param HandlesNew        Новый набор хэндлов.
 * @param HandlesAdded      Вектор с хэндлами, добавленными в HandlesNew и отсутствующими в HandlesOld.
 *                          Его НЕ нужно заранее инициализировать функцией Vector_Create ().
 *                          Не забыть корректно уничтожить его.
 *
 * @param EFI_SUCCESS       Всё ок, результат в HandlesAdded.
 * @param Что-то другое     Операция не удалась.
*/
EFI_STATUS
HandleDatabaseDump_GetAddedHandles (
  IN  VECTOR TYPE (EFI_HANDLE) *HandlesOld,
  IN  VECTOR TYPE (EFI_HANDLE) *HandlesNew,
  OUT VECTOR TYPE (EFI_HANDLE) *HandlesAdded
  );

//------------------------------------------------------------------------------


#endif  // HANDLE_DATABASE_DUMP_H_