#include <Uefi.h>
#include <Library/VectorLib.h>

#ifndef HANDLE_DATABASE_DUMP_H_
#define HANDLE_DATABASE_DUMP_H_


//------------------------------------------------------------------------------
typedef struct
{
  EFI_HANDLE Handle;
  VECTOR     Protocols;   // Vector <EFI_GUID>
} HANDLE_DATABASE_ENTRY;

typedef struct
{
  VECTOR Entries;         // Vector <HANDLE_DATABASE_ENTRY>
} HANDLE_DATABASE_DUMP;


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
 * Возвращает те хэндлы из DumpNew, на которые установлен протокол ProtocolGuid
 * и которых или не было в DumpOld или на них не был установлен протокол ProtocolGuid.
 *
 * @param DumpOld           Старый дамп HandleDB.
 * @param DumpNew           Новый дамп HandleDB.
 * @param ProtocolGuid      GUID искомого протокола.
 * @param VectorNewHandles  Вектор с новыми хэндлами. Не забыть корректно уничтожить его.
 *
 * @param EFI_SUCCESS       Всё ок, результат в VectorNewHandles.
 * @param Что-то другое     Операция не удалась.
*/
EFI_STATUS
HandleDatabaseDump_GetNewHandlesForProtocol (
  IN  HANDLE_DATABASE_DUMP *DumpOld,
  IN  HANDLE_DATABASE_DUMP *DumpNew,
  IN  EFI_GUID             *ProtocolGuid,
  OUT VECTOR               *VectorNewHandles
  );

//------------------------------------------------------------------------------


#endif  // HANDLE_DATABASE_DUMP_H_