#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/EventProviderLib.h>
#include <Library/CommonMacrosLib.h>
#include <Protocol/Bds.h>

// -----------------------------------------------------------------------------
// Хук на переход в BDS.
VOID
EFIAPI MyBdsArchProtocolEntry (
  IN EFI_BDS_ARCH_PROTOCOL  *This
  );

static EFI_BDS_ARCH_PROTOCOL gMyBdsArchProtocol = { &MyBdsArchProtocolEntry };
static EFI_BDS_ARCH_PROTOCOL *gOriginalBdsArchProtocol = NULL;


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
BOOLEAN IsBdsArchProtocolGuid(VOID *Guid);

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
  This->Data         = NULL;         // Не используем.

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
  ;  // TODO
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
  ;  // TODO
}

// -----------------------------------------------------------------------------
EFI_STATUS
EFIAPI MyUninstallProtocolInterface (
  IN EFI_HANDLE               Handle,
  IN EFI_GUID                 *Protocol,
  IN VOID                     *Interface
  )
{
  ;  // TODO
}

// -----------------------------------------------------------------------------
EFI_STATUS
EFIAPI MyInstallMultipleProtocolInterfaces (
  IN OUT EFI_HANDLE           *Handle,
  ...
  )
{
  ;  // TODO
}

// -----------------------------------------------------------------------------
EFI_STATUS
EFIAPI MyUninstallMultipleProtocolInterfaces (
  IN EFI_HANDLE           Handle,
  ...
  )
{
  ;  // TODO
}

// -----------------------------------------------------------------------------
BOOLEAN IsBdsArchProtocolGuid(VOID *Guid)
{
  return CompareGuid ((EFI_GUID *)Guid, &gEfiBdsArchProtocolGuid);
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
