#include <Library/ProtocolGuidDatabaseLib.h>
#include <Library/BaseMemoryLib.h>
#include "GeneratedProtocolGuidDatabase.h"

typedef struct
{
  EFI_GUID *Guid;
  CHAR16   *Name;
} KNOWN_PROTOCOL_DB_ENTRY;

static KNOWN_PROTOCOL_DB_ENTRY gProtocolDatabase[] = {
  GUID_DB
};

#define KNOWN_GUID_COUNT (sizeof(gProtocolDatabase) / sizeof(gProtocolDatabase[0]))
_Static_assert (KNOWN_GUID_COUNT > 0, "Protocol database is empty");

// -----------------------------------------------------------------------------
/**
 * Возвращает осмысленное имя протокола по его GUID'у.
 *
 * @param Guid                      Указатель на GUID протокола.
 *
 * @retval NULL                     Протокол не найден в БД.
 * @return Указатель на строку, содержащую имя протокола.
 *         Данная строка существует на протяжении всей жизни программы,
 *         перезаписывать её не следует.
 */
CHAR16 *
GetProtocolName (
  IN EFI_GUID *Guid
  )
{
  if (Guid == NULL) {
    return NULL;
  }

  // По-хорошему, следовало бы сделать это в виде поиска по дереву, например.
  // Но потери в скорости из-за линейного поиска не критичны.
  for (UINTN Index = 0; Index < KNOWN_GUID_COUNT; ++Index) {
    if (CompareGuid (Guid, gProtocolDatabase[Index].Guid)) {
      return gProtocolDatabase[Index].Name;
    }
  }

  return NULL;
}

// -----------------------------------------------------------------------------
/**
 * Позволяет перечислить все занесённые в БД GUIDы протоколов.
 *
 * @param Guid  В этот параметр функция заносит:
 *              - GUID первого известного протокола, если *Guid == NULL
 *              - GUID следующего протокола, если *Guid указывает на один из протоколов БД
 *              - NULL, если все протоколы были перечислены, либо если *Guid не указывает на протокол БД.
 */
VOID
GetProtocolGuid (
  IN OUT EFI_GUID **Guid
  )
{
  if (Guid == NULL) {
    *Guid = NULL;
    return;
  }

  if (Guid < &gProtocolDatabase[0].Guid || Guid > &gProtocolDatabase[KNOWN_GUID_COUNT - 1].Guid) {
    *Guid = NULL;
    return;
  }

  if (*Guid == NULL) {
    *Guid = gProtocolDatabase[0].Guid;
  } else if (*Guid == gProtocolDatabase[KNOWN_GUID_COUNT - 1].Guid) {
    *Guid = NULL;
  } else {
    *Guid = (EFI_GUID *)(((UINT8 *)(*Guid)) + sizeof(KNOWN_PROTOCOL_DB_ENTRY));
  }
}

// -----------------------------------------------------------------------------
