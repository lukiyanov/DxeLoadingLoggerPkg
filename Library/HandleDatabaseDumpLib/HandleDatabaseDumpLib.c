#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>

#include <Library/HandleDatabaseDumpLib.h>
#include <Library/CommonMacrosLib.h>

//------------------------------------------------------------------------------
/**
 * Генерит HANDLE_DATABASE_DUMP из текущего состояния системы.
*/
EFI_STATUS
GetHandleDatabaseDump (
  OUT HANDLE_DATABASE_DUMP *Dump
  )
{
  DBG_ENTER ();

  if (Dump == NULL) {
    DBG_EXIT_STATUS (EFI_INVALID_PARAMETER);
    return EFI_INVALID_PARAMETER;
  }

  EFI_STATUS Status;
  EFI_HANDLE *Handles;
  UINTN      HandleCount;

  Status = gBS->LocateHandleBuffer (
                  AllHandles,
                  NULL,
                  NULL,
                  &HandleCount,
                  &Handles
                  );
  RETURN_ON_ERR (Status);

  Status = Vector_Construct (
            Dump,
            sizeof(HANDLE_DATABASE_ENTRY),
            HandleCount
            );
  RETURN_ON_ERR (Status);

  for (UINTN Index = 0; Index < HandleCount; ++Index) {
    EFI_GUID **ProtocolGuidArray = NULL;
    UINTN    ArrayCount;

    Status = gBS->ProtocolsPerHandle (
                    Handles[Index],
                    &ProtocolGuidArray,
                    &ArrayCount
                    );
    if (EFI_ERROR (Status)) {
      DBG_ERROR ("Error in ProtocolsPerHandle(): %r", Status);
      continue;
    }

    HANDLE_DATABASE_ENTRY HandleInfo;
    HandleInfo.Handle = Handles[Index];
    Status = Vector_Construct (
              &HandleInfo.InstalledProtocolGuids,
              sizeof(EFI_GUID),
              ArrayCount
              );
    if (EFI_ERROR (Status)) {
      DBG_ERROR ("Error in Vector_Construct(): %r", Status);
      SHELL_FREE_NON_NULL(ProtocolGuidArray);
      continue;
    }

    for (UINTN ProtocolIndex = 0; ProtocolIndex < ArrayCount; ProtocolIndex++) {
      EFI_GUID *ProtocolGuid = ProtocolGuidArray[ProtocolIndex];

      Vector_PushBack (&HandleInfo.InstalledProtocolGuids, ProtocolGuid);
    }

    SHELL_FREE_NON_NULL(ProtocolGuidArray);
  }

  SHELL_FREE_NON_NULL (Handles);
  DBG_EXIT_STATUS (EFI_SUCCESS);
  return EFI_SUCCESS;
}

//------------------------------------------------------------------------------
/**
 * Корректно уничтожает HANDLE_DATABASE_DUMP.
*/
VOID
HandleDatabaseDump_Destruct (
  IN OUT HANDLE_DATABASE_DUMP *Dump
  )
{
  DBG_ENTER ();

  if (Dump == NULL) {
    DBG_EXIT_STATUS (EFI_INVALID_PARAMETER);
    return;
  }

  FOR_EACH_VCT (HANDLE_DATABASE_ENTRY, DbEntry, *Dump) {
    Vector_Destruct (&DbEntry->InstalledProtocolGuids);
  }

  Vector_Destruct (Dump);

  DBG_EXIT ();
}

//------------------------------------------------------------------------------
/**
 * Возвращает список всех протоколов из Dump.
 * Каждый протокол встречается в списке ровно один раз.
 *
 * @param Dump              Дамп, в котором мы ищем хэндлы.
 * @param Protocols         Вектор с GUID'ами протоколов.
 *                          Его НЕ нужно заранее инициализировать функцией Vector_Create ().
 *                          Не забыть корректно уничтожить его.
 *
 * @param EFI_SUCCESS       Всё ок, результат в Protocols.
 * @param Что-то другое     Операция не удалась.
*/
EFI_STATUS
HandleDatabaseDump_PeekAllProtocols (
  IN  HANDLE_DATABASE_DUMP    *Dump,
  OUT VECTOR TYPE (EFI_GUID)  *Protocols
  )
{
  DBG_ENTER ();

  if (Dump == NULL || Protocols == NULL) {
    DBG_EXIT_STATUS (EFI_INVALID_PARAMETER);
    return EFI_INVALID_PARAMETER;
  }

  EFI_STATUS Status;
  Status = Vector_Construct (Protocols, sizeof(EFI_GUID), 64);
  RETURN_ON_ERR (Status);

  FOR_EACH_VCT (HANDLE_DATABASE_ENTRY, HandleWithProtocols, *Dump) {
    // Для каждого протокола...
    FOR_EACH_VCT (EFI_GUID, Guid, HandleWithProtocols->InstalledProtocolGuids) {
      BOOLEAN AlreadyStored = FALSE;

      // Ищем, занесли ли мы его в список ранее...
      FOR_EACH_VCT (EFI_GUID, StoredGuid, *Protocols) {
        if (CompareGuid (Guid, StoredGuid)) {
          AlreadyStored = TRUE;
          break;
        }
      }

      // И если мы его ещё не занесли в Protocols...
      if (!AlreadyStored) {
        // То заносим.
        Status = Vector_PushBack (Protocols, Guid);
        if (EFI_ERROR (Status)) {
          Vector_Destruct (Protocols);
          DBG_EXIT_STATUS (Status);
          return Status;
        }
      }
    }
  }

  DBG_EXIT_STATUS (EFI_SUCCESS);
  return EFI_SUCCESS;
}

//------------------------------------------------------------------------------
/**
 * Возвращает хэндлы из Dump, на которые установлен протокол ProtocolGuid.
 *
 * @param Dump              Дамп, в котором мы ищем хэндлы.
 * @param ProtocolGuid      GUID искомого протокола.
 * @param Handles           Вектор с хэндлами, на которые установлен протокол. Не забыть корректно уничтожить его.
 *
 * @param EFI_SUCCESS       Всё ок, результат в Handles.
 * @param Что-то другое     Операция не удалась.
*/
EFI_STATUS
HandleDatabaseDump_PeekHandlesWithProtocol (
  IN  HANDLE_DATABASE_DUMP     *Dump,
  IN  EFI_GUID                 *ProtocolGuid,
  OUT VECTOR TYPE (EFI_HANDLE) *Handles
  )
{
  DBG_ENTER ();

  if (Dump == NULL || ProtocolGuid == NULL || Handles == NULL) {
    DBG_EXIT_STATUS (EFI_INVALID_PARAMETER);
    return EFI_INVALID_PARAMETER;
  }

  EFI_STATUS Status;
  Status = Vector_Construct (Handles, sizeof(EFI_HANDLE), 2);
  RETURN_ON_ERR (Status);

  FOR_EACH_VCT (HANDLE_DATABASE_ENTRY, DbEntry, *Dump) {
    FOR_EACH_VCT (EFI_GUID, Guid, DbEntry->InstalledProtocolGuids) {
      if (CompareGuid (Guid, ProtocolGuid)) {
        Status = Vector_PushBack (Handles, &DbEntry->Handle);
        if (EFI_ERROR (Status)) {
          Vector_Destruct (Handles);
          DBG_EXIT_STATUS (Status);
          return Status;
        }
        break;
      }
    }
  }

  DBG_EXIT_STATUS (EFI_SUCCESS);
  return EFI_SUCCESS;
}

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
  )
{
  DBG_ENTER ();

  if (HandlesOld == NULL || HandlesNew == NULL || HandlesAdded == NULL) {
    DBG_EXIT_STATUS (EFI_INVALID_PARAMETER);
    return EFI_INVALID_PARAMETER;
  }

  EFI_STATUS Status;
  Status = Vector_Construct (HandlesAdded, sizeof(EFI_HANDLE), 2);
  RETURN_ON_ERR (Status);

  FOR_EACH_VCT (EFI_HANDLE, HandleNew, *HandlesNew) {
    BOOLEAN Found = FALSE;

    FOR_EACH_VCT (EFI_HANDLE, HandleOld, *HandlesOld) {
      if (*HandleNew == *HandleOld) {
        Found = TRUE;
        continue;
      }
    }

    if (!Found) {
      Status = Vector_PushBack (HandlesAdded, HandleNew);
        if (EFI_ERROR (Status)) {
          Vector_Destruct (HandlesAdded);
          DBG_EXIT_STATUS (Status);
          return Status;
        }
    }
  }

  DBG_EXIT_STATUS (EFI_SUCCESS);
  return EFI_SUCCESS;
}

//------------------------------------------------------------------------------