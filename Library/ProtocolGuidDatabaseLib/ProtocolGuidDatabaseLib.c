#include <Library/CommonMacrosLib.h>
#include <Library/ProtocolGuidDatabaseLib.h>
#include <Library/BaseMemoryLib.h>
#include "GeneratedProtocolGuidDatabase.h"

typedef struct
{
  EFI_GUID *Guid;
  CHAR16   *Name;
} KNOWN_PROTOCOL_DB_ENTRY;

STATIC KNOWN_PROTOCOL_DB_ENTRY gProtocolDatabase[] = {
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
DBG_ENTER ();

  if (Guid == NULL) {
    DBG_EXIT_STATUS (EFI_INVALID_PARAMETER);
    return NULL;
  }

  // По-хорошему, следовало бы сделать это в виде поиска по дереву, например.
  // Но потери в скорости из-за линейного поиска не критичны.
  for (UINTN Index = 0; Index < KNOWN_GUID_COUNT; ++Index) {
    if (CompareGuid (Guid, gProtocolDatabase[Index].Guid)) {
      DBG_EXIT_STATUS (EFI_SUCCESS);
      return gProtocolDatabase[Index].Name;
    }
  }

  DBG_EXIT_STATUS (EFI_NOT_FOUND);
  return NULL;
}

// -----------------------------------------------------------------------------
/**
 * Возвращает количество известных GUID.
 *
 * @retval Количество известных GUID.
 */
UINTN
GetProtocolGuidCount ()
{
  return KNOWN_GUID_COUNT;
}

// -----------------------------------------------------------------------------
/**
 * Возвращает указатель на GUID протокола с индексом Index.
 *
 * @param Index  Индекс протокола в БД.
 *
 * @retval NULL  Индекс выходит за пределы [0 .. GetProtocolGuidCount()]
 * @retval Указатель на GUID протокола с указанным индексом.
 */
EFI_GUID *
GetProtocolGuid (
  IN UINTN Index
  )
{
  DBG_ENTER ();

  if (Index >= KNOWN_GUID_COUNT) {
    DBG_EXIT_STATUS (EFI_INVALID_PARAMETER);
    return NULL;
  }

  DBG_INFO ("-- Protocol GUID index: %u/%u\n", (unsigned)Index + 1, KNOWN_GUID_COUNT);
  return gProtocolDatabase[Index].Guid;

  DBG_EXIT ();
}

// -----------------------------------------------------------------------------
