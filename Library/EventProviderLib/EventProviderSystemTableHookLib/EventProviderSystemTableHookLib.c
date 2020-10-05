#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PcdLib.h>
#include <Library/EventProviderLib.h>
#include <Library/CommonMacrosLib.h>
#include <Protocol/Bds.h>

// Более 35 интерфейсов за раз будут устанавливать только совсем отбитые разрабы, большее количество не поддерживаем.
// Возможно, в далёком и светлом будущем способ форвардинга аргументов будет заменён на более универсальный.
#define ARG_ARRAY_ELEMENT_COUNT 70

#define ARG_ARRAY_ALL_ELEMENTS(Array) \
  Array[0],  \
  Array[1],  \
  Array[2],  \
  Array[3],  \
  Array[4],  \
  Array[5],  \
  Array[6],  \
  Array[7],  \
  Array[8],  \
  Array[9],  \
  Array[10], \
  Array[11], \
  Array[12], \
  Array[13], \
  Array[14], \
  Array[15], \
  Array[16], \
  Array[17], \
  Array[18], \
  Array[19], \
  Array[20], \
  Array[21], \
  Array[22], \
  Array[23], \
  Array[24], \
  Array[25], \
  Array[26], \
  Array[27], \
  Array[28], \
  Array[29], \
  Array[30], \
  Array[31], \
  Array[32], \
  Array[33], \
  Array[34], \
  Array[35], \
  Array[36], \
  Array[37], \
  Array[38], \
  Array[39], \
  Array[40], \
  Array[41], \
  Array[42], \
  Array[43], \
  Array[44], \
  Array[45], \
  Array[46], \
  Array[47], \
  Array[48], \
  Array[49], \
  Array[50], \
  Array[51], \
  Array[52], \
  Array[53], \
  Array[54], \
  Array[55], \
  Array[56], \
  Array[57], \
  Array[58], \
  Array[59], \
  Array[60], \
  Array[61], \
  Array[62], \
  Array[63], \
  Array[64], \
  Array[65], \
  Array[66], \
  Array[67], \
  Array[68], \
  Array[69]

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
BOOLEAN
IsBdsArchProtocolGuidAndWeMustSubstituteIt (
  VOID *Guid
  );

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
  DBG_ENTER ();

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
  DBG_ENTER ();

  // TODO: BEFORE

  gOriginalBdsArchProtocol->Entry(This);

  // TODO: AFTER

  DBG_EXIT ();
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
  DBG_ENTER ();

  if (IsBdsArchProtocolGuidAndWeMustSubstituteIt (Protocol)) {
    gOriginalBdsArchProtocol = Interface;
    Interface = &gMyBdsArchProtocol;
  }

  EFI_STATUS Status = gOriginalInstallProtocolInterface (Handle, Protocol, InterfaceType, Interface);
  // TODO: ...

  DBG_EXIT_STATUS (Status);
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
  DBG_ENTER ();

  if (IsBdsArchProtocolGuidAndWeMustSubstituteIt (Protocol)) {
    gOriginalBdsArchProtocol = NewInterface;
    OldInterface = &gMyBdsArchProtocol;
    NewInterface = &gMyBdsArchProtocol;
  }

  EFI_STATUS Status = gOriginalReinstallProtocolInterface (Handle, Protocol, OldInterface, NewInterface);
  // TODO: ...

  DBG_EXIT_STATUS (Status);
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
  DBG_ENTER ();

  if (IsBdsArchProtocolGuidAndWeMustSubstituteIt (Protocol)) {
    Interface = &gMyBdsArchProtocol;
  }

  EFI_STATUS Status = gOriginalUninstallProtocolInterface (Handle, Protocol, Interface);
  // TODO: ...

  DBG_EXIT_STATUS (Status);
  return Status;
}

// -----------------------------------------------------------------------------
EFI_STATUS
EFIAPI MyInstallMultipleProtocolInterfaces (
  IN OUT EFI_HANDLE           *Handle,
  ...
  )
{
  DBG_ENTER ();

  VOID* FunctionArgList[ARG_ARRAY_ELEMENT_COUNT];
  UINTN FunctionArgCount = 0;
  ZeroMem (FunctionArgList, sizeof (FunctionArgList));

  VA_LIST VaList;
  VA_START (VaList, Handle);
  {
    // Аргументы идут по 2, если первый из них NULL - конец.
    while (TRUE) {
      if (FunctionArgCount == ARG_ARRAY_ELEMENT_COUNT) {
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

  EFI_STATUS Status = gOriginalInstallMultipleProtocolInterfaces (Handle, ARG_ARRAY_ALL_ELEMENTS(FunctionArgList), NULL);
  // TODO: ...

  DBG_EXIT_STATUS (Status);
  return Status;
}

// -----------------------------------------------------------------------------
EFI_STATUS
EFIAPI MyUninstallMultipleProtocolInterfaces (
  IN EFI_HANDLE           Handle,
  ...
  )
{
  DBG_ENTER ();

  VOID* FunctionArgList[ARG_ARRAY_ELEMENT_COUNT];
  UINTN FunctionArgCount = 0;
  ZeroMem (FunctionArgList, sizeof (FunctionArgList));

  VA_LIST VaList;
  VA_START (VaList, Handle);
  {
    // Аргументы идут по 2, если первый из них NULL - конец.
    while (TRUE) {
      if (FunctionArgCount == ARG_ARRAY_ELEMENT_COUNT) {
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

  EFI_STATUS Status = gOriginalUninstallMultipleProtocolInterfaces (Handle, ARG_ARRAY_ALL_ELEMENTS(FunctionArgList), NULL);
  // TODO: ...

  DBG_EXIT_STATUS (Status);
  return Status;
}

// -----------------------------------------------------------------------------
BOOLEAN
IsBdsArchProtocolGuidAndWeMustSubstituteIt (
  VOID *Guid
  )
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
