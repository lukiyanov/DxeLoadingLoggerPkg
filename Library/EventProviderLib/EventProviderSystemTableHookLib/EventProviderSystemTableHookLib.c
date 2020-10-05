#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PcdLib.h>
#include <Library/EventProviderLib.h>
#include <Library/CommonMacrosLib.h>
#include <Protocol/Bds.h>

// Более 35 интерфейсов за раз будут устанавливать только совсем отбитые разрабы, большее количество не поддерживаем.
// Возможно, в далёком и светлом будущем способ форвардинга аргументов будет заменён на более универсальный.
#define ARRAY_ARG_LENGTH 70

#define ARRAY_ARGS(Pairs) \
  Pairs[0],  \
  Pairs[1],  \
  Pairs[2],  \
  Pairs[3],  \
  Pairs[4],  \
  Pairs[5],  \
  Pairs[6],  \
  Pairs[7],  \
  Pairs[8],  \
  Pairs[9],  \
  Pairs[10], \
  Pairs[11], \
  Pairs[12], \
  Pairs[13], \
  Pairs[14], \
  Pairs[15], \
  Pairs[16], \
  Pairs[17], \
  Pairs[18], \
  Pairs[19], \
  Pairs[20], \
  Pairs[21], \
  Pairs[22], \
  Pairs[23], \
  Pairs[24], \
  Pairs[25], \
  Pairs[26], \
  Pairs[27], \
  Pairs[28], \
  Pairs[29], \
  Pairs[30], \
  Pairs[31], \
  Pairs[32], \
  Pairs[33], \
  Pairs[34], \
  Pairs[35], \
  Pairs[36], \
  Pairs[37], \
  Pairs[38], \
  Pairs[39], \
  Pairs[40], \
  Pairs[41], \
  Pairs[42], \
  Pairs[43], \
  Pairs[44], \
  Pairs[45], \
  Pairs[46], \
  Pairs[47], \
  Pairs[48], \
  Pairs[49], \
  Pairs[50], \
  Pairs[51], \
  Pairs[52], \
  Pairs[53], \
  Pairs[54], \
  Pairs[55], \
  Pairs[56], \
  Pairs[57], \
  Pairs[58], \
  Pairs[59], \
  Pairs[60], \
  Pairs[61], \
  Pairs[62], \
  Pairs[63], \
  Pairs[64], \
  Pairs[65], \
  Pairs[66], \
  Pairs[67], \
  Pairs[68], \
  Pairs[69]

// -----------------------------------------------------------------------------
// Хук на переход в BDS.
VOID
EFIAPI MyBdsArchProtocolEntry (
  IN EFI_BDS_ARCH_PROTOCOL  *This
  );

static GLOBAL_REMOVE_IF_UNREFERENCED EFI_BDS_ARCH_PROTOCOL gMyBdsArchProtocol = { &MyBdsArchProtocolEntry };
static GLOBAL_REMOVE_IF_UNREFERENCED EFI_BDS_ARCH_PROTOCOL *gOriginalBdsArchProtocol = NULL;


// -----------------------------------------------------------------------------
// Указатели на оригинальные системные сервисы.
// -----------------------------------------------------------------------------
static EFI_INSTALL_PROTOCOL_INTERFACE              gOriginalInstallProtocolInterface;
static EFI_REINSTALL_PROTOCOL_INTERFACE            gOriginalReinstallProtocolInterface;
static EFI_UNINSTALL_PROTOCOL_INTERFACE            gOriginalUninstallProtocolInterface;
static EFI_INSTALL_MULTIPLE_PROTOCOL_INTERFACES    gOriginalInstallMultipleProtocolInterfaces;
static EFI_UNINSTALL_MULTIPLE_PROTOCOL_INTERFACES  gOriginalUninstallMultipleProtocolInterfaces;

// -----------------------------------------------------------------------------
// То, чем мы заменяем системные сервисы.
// -----------------------------------------------------------------------------
EFI_STATUS
EFIAPI MyInstallProtocolInterface (
  IN OUT EFI_HANDLE               *Handle,
  IN     EFI_GUID                 *Protocol,
  IN     EFI_INTERFACE_TYPE       InterfaceType,
  IN     VOID                     *Interface
  );

EFI_STATUS
EFIAPI MyReinstallProtocolInterface (
  IN EFI_HANDLE               Handle,
  IN EFI_GUID                 *Protocol,
  IN VOID                     *OldInterface,
  IN VOID                     *NewInterface
  );

EFI_STATUS
EFIAPI MyUninstallProtocolInterface (
  IN EFI_HANDLE               Handle,
  IN EFI_GUID                 *Protocol,
  IN VOID                     *Interface
  );

EFI_STATUS
EFIAPI MyInstallMultipleProtocolInterfaces (
  IN OUT EFI_HANDLE           *Handle,
  ...
  );

EFI_STATUS
EFIAPI MyUninstallMultipleProtocolInterfaces (
  IN EFI_HANDLE           Handle,
  ...
  );

// -----------------------------------------------------------------------------
/**
 * TRUE, если это EFI_BDS_ARCH_PROTOCOL_GUID.
*/
BOOLEAN IsBdsArchProtocolGuidAndWeMustSubstituteIt(VOID *Guid);

// -----------------------------------------------------------------------------
/**
  Calcualte the 32-bit CRC in a EFI table using the service provided by the
  gRuntime service.
  (выдрано из стандартного DxeMain.c)

  @param  Hdr                    Pointer to an EFI standard header

**/
VOID
CalculateEfiHdrCrc (
  IN  OUT EFI_TABLE_HEADER    *Hdr
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
  This->Data         = NULL;

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
  IN OUT  EVENT_PROVIDER  *This
  )
{
  if (This == NULL || This->Data == NULL) {
    DBG_EXIT_STATUS (EFI_INVALID_PARAMETER);
    return EFI_INVALID_PARAMETER;
  }

  EFI_TPL PreviousTpl = gBS->RaiseTPL (TPL_HIGH_LEVEL);
  {
    gOriginalInstallProtocolInterface            = gST->BootServices->InstallProtocolInterface;
    gOriginalReinstallProtocolInterface          = gST->BootServices->ReinstallProtocolInterface;
    gOriginalUninstallProtocolInterface          = gST->BootServices->UninstallProtocolInterface;
    gOriginalInstallMultipleProtocolInterfaces   = gST->BootServices->InstallMultipleProtocolInterfaces;
    gOriginalUninstallMultipleProtocolInterfaces = gST->BootServices->UninstallMultipleProtocolInterfaces;

    gST->BootServices->InstallProtocolInterface            = &MyInstallProtocolInterface;
    gST->BootServices->ReinstallProtocolInterface          = &MyReinstallProtocolInterface;
    gST->BootServices->UninstallProtocolInterface          = &MyUninstallProtocolInterface;
    gST->BootServices->InstallMultipleProtocolInterfaces   = &MyInstallMultipleProtocolInterfaces;
    gST->BootServices->UninstallMultipleProtocolInterfaces = &MyUninstallMultipleProtocolInterfaces;

    CalculateEfiHdrCrc (&gST->BootServices->Hdr);
  }
  gBS->RestoreTPL (PreviousTpl);

  DBG_EXIT_STATUS (EFI_SUCCESS);
  return EFI_SUCCESS;
}

// -----------------------------------------------------------------------------
/**
 * EVENT_PROVIDER отсоединяется от системы и прекращает сбор событий.
 */
VOID
EventProvider_Stop (
  IN OUT  EVENT_PROVIDER  *This
  )
{
  DBG_ENTER ();

  if (This == NULL || This->Data == NULL) {
    DBG_EXIT_STATUS (EFI_INVALID_PARAMETER);
    return;
  }

  EFI_TPL PreviousTpl = gBS->RaiseTPL (TPL_HIGH_LEVEL);
  {
    gST->BootServices->InstallProtocolInterface            = gOriginalInstallProtocolInterface;
    gST->BootServices->ReinstallProtocolInterface          = gOriginalReinstallProtocolInterface;
    gST->BootServices->UninstallProtocolInterface          = gOriginalUninstallProtocolInterface;
    gST->BootServices->InstallMultipleProtocolInterfaces   = gOriginalInstallMultipleProtocolInterfaces;
    gST->BootServices->UninstallMultipleProtocolInterfaces = gOriginalUninstallMultipleProtocolInterfaces;

    CalculateEfiHdrCrc (&gST->BootServices->Hdr);
  }
  gBS->RestoreTPL (PreviousTpl);

  DBG_EXIT ();
}

// -----------------------------------------------------------------------------
VOID
EFIAPI MyBdsArchProtocolEntry (
  IN EFI_BDS_ARCH_PROTOCOL  *This
  )
{
  // TODO: BEFORE

  gOriginalBdsArchProtocol->Entry(This);

  // TODO: AFTER
}

// -----------------------------------------------------------------------------
EFI_STATUS
EFIAPI MyInstallProtocolInterface (
  IN OUT EFI_HANDLE               *Handle,
  IN     EFI_GUID                 *Protocol,
  IN     EFI_INTERFACE_TYPE       InterfaceType,
  IN     VOID                     *Interface
  )
{
  if (IsBdsArchProtocolGuidAndWeMustSubstituteIt (Protocol)) {
    gOriginalBdsArchProtocol = Interface;
    Interface = &gMyBdsArchProtocol;
  }

  EFI_STATUS Status = gOriginalInstallProtocolInterface (Handle, Protocol, InterfaceType, Interface);
  // TODO: ...
  return Status;
}

// -----------------------------------------------------------------------------
EFI_STATUS
EFIAPI MyReinstallProtocolInterface (
  IN EFI_HANDLE               Handle,
  IN EFI_GUID                 *Protocol,
  IN VOID                     *OldInterface,
  IN VOID                     *NewInterface
  )
{
  if (IsBdsArchProtocolGuidAndWeMustSubstituteIt (Protocol)) {
    gOriginalBdsArchProtocol = NewInterface;
    OldInterface = &gMyBdsArchProtocol;
    NewInterface = &gMyBdsArchProtocol;
  }

  EFI_STATUS Status = gOriginalReinstallProtocolInterface (Handle, Protocol, OldInterface, NewInterface);
  // TODO: ...
  return Status;
}

// -----------------------------------------------------------------------------
EFI_STATUS
EFIAPI MyUninstallProtocolInterface (
  IN EFI_HANDLE               Handle,
  IN EFI_GUID                 *Protocol,
  IN VOID                     *Interface
  )
{
  if (IsBdsArchProtocolGuidAndWeMustSubstituteIt (Protocol)) {
    Interface = &gMyBdsArchProtocol;
  }

  EFI_STATUS Status = gOriginalUninstallProtocolInterface (Handle, Protocol, Interface);
  // TODO: ...
  return Status;
}

// -----------------------------------------------------------------------------
EFI_STATUS
EFIAPI MyInstallMultipleProtocolInterfaces (
  IN OUT EFI_HANDLE           *Handle,
  ...
  )
{
  VOID* FunctionArgList[ARRAY_ARG_LENGTH];
  UINTN FunctionArgCount = 0;
  ZeroMem (FunctionArgList, sizeof (FunctionArgList));

  VA_LIST VaList;
  VA_START (VaList, Handle);
  {
    // Аргументы идут по 2, если первый из них NULL - конец.
    while (TRUE) {
      if (FunctionArgCount == ARRAY_ARG_LENGTH) {
        // Большее количество не поддерживем, это бессмысленно.

        // TODO: добавить событие "Ошибка" в этом случае.
        break;
      }

      VOID *ArgInterfaceGuid = VA_ARG (VaList, VOID*);
      if (ArgInterfaceGuid == NULL) {
        break;
      }

      VOID *ArgInterfaceStruct = VA_ARG (VaList, VOID*);

      if (IsBdsArchProtocolGuidAndWeMustSubstituteIt (ArgInterfaceGuid)) {
        gOriginalBdsArchProtocol = ArgInterfaceStruct;
        ArgInterfaceStruct = &gMyBdsArchProtocol;
      }

      FunctionArgList[FunctionArgCount++] = ArgInterfaceGuid;
      FunctionArgList[FunctionArgCount++] = ArgInterfaceStruct;
    }
  }
  VA_END (VaList);

  EFI_STATUS Status = gOriginalInstallMultipleProtocolInterfaces(Handle, ARRAY_ARGS(FunctionArgList), NULL);
  // TODO: ...
  return Status;
}

// -----------------------------------------------------------------------------
EFI_STATUS
EFIAPI MyUninstallMultipleProtocolInterfaces (
  IN EFI_HANDLE           Handle,
  ...
  )
{
  VOID* FunctionArgList[ARRAY_ARG_LENGTH];
  UINTN FunctionArgCount = 0;
  ZeroMem (FunctionArgList, sizeof (FunctionArgList));

  VA_LIST VaList;
  VA_START (VaList, Handle);
  {
    // Аргументы идут по 2, если первый из них NULL - конец.
    while (TRUE) {
      if (FunctionArgCount == ARRAY_ARG_LENGTH) {
        // Большее количество не поддерживем, это бессмысленно.

        // TODO: добавить событие "Ошибка" в этом случае.
        break;
      }

      VOID *ArgInterfaceGuid = VA_ARG (VaList, VOID*);
      if (ArgInterfaceGuid == NULL) {
        break;
      }

      VOID *ArgInterfaceStruct = VA_ARG (VaList, VOID*);

      if (IsBdsArchProtocolGuidAndWeMustSubstituteIt (ArgInterfaceGuid)) {
        ArgInterfaceStruct = &gMyBdsArchProtocol;
      }

      FunctionArgList[FunctionArgCount++] = ArgInterfaceGuid;
      FunctionArgList[FunctionArgCount++] = ArgInterfaceStruct;
    }
  }
  VA_END (VaList);

  EFI_STATUS Status = gOriginalUninstallMultipleProtocolInterfaces(Handle, ARRAY_ARGS(FunctionArgList), NULL);
  // TODO: ...
  return Status;
}

// -----------------------------------------------------------------------------
BOOLEAN IsBdsArchProtocolGuidAndWeMustSubstituteIt(VOID *Guid)
{
  if (FeaturePcdGet (PcdBdsEntryHookEnabled)) {
    return CompareGuid ((EFI_GUID *)Guid, &gEfiBdsArchProtocolGuid);
  } else {
    return FALSE;
  }
}

// -----------------------------------------------------------------------------
/**
  Calcualte the 32-bit CRC in a EFI table using the service provided by the
  gRuntime service.

  @param  Hdr                    Pointer to an EFI standard header

**/
VOID
CalculateEfiHdrCrc (
  IN  OUT EFI_TABLE_HEADER    *Hdr
  )
{
  UINT32 Crc = 0;

  Hdr->CRC32 = 0;
  gBS->CalculateCrc32 ((UINT8 *)Hdr, Hdr->HeaderSize, &Crc);
  Hdr->CRC32 = Crc;
}

// -----------------------------------------------------------------------------
