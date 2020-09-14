#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiLib.h>
#include <Library/DevicePathLib.h>

#include <Protocol/FirmwareVolume2.h>
#include <Protocol/LoadedImage.h>

#include "DxeLoadingLog.h"

#define RETURN_ON_ERR(Status) if (EFI_ERROR (Status)) { return Status; }


// ----------------------------------------------------------------------------
VOID
CheckProtocolExistence (
  DxeLoadingLog *This,
  ProtocolEntry *Protocol
  );

EFI_STATUS
SubscribeToProtocolInstallation (
  DxeLoadingLog *This,
  ProtocolEntry *Protocol
  );

VOID
EFIAPI
ProtocolInstalledCallback (
  IN  EFI_EVENT  Event,
  IN  VOID       *Context
  );

VOID
GetHandleName (
  IN EFI_HANDLE Handle,
  OUT CHAR8 *Name
  );

VOID
SetEntryImageNames (
  IN EFI_HANDLE Handle,
  OUT LOG_ENTRY *LogEntry
  );

VOID
DetectEntryImageNames (
  IN  DxeLoadingLog *This,
  OUT LOG_ENTRY *Entry
  );

VOID
ScanHandleDB (
  DxeLoadingLog *This
  );

CHAR16*
FindLoadedImageFileName (
  IN EFI_LOADED_IMAGE_PROTOCOL *LoadedImage
  );

// ----------------------------------------------------------------------------
VOID
DxeLoadingLog_Construct (
  DxeLoadingLog *This
  )
{
  Vector_Construct (&This->LogData,            sizeof(LOG_ENTRY),  1024);
  Vector_Construct (&This->EventHandles,       sizeof(EFI_EVENT),  1024);
  Vector_Construct (&This->LoadedImageHandles, sizeof(EFI_HANDLE), 64);
}

// ----------------------------------------------------------------------------
VOID
DxeLoadingLog_Destruct (
  DxeLoadingLog *This
  )
{
  DxeLoadingLog_QuitObserving(This);

  Vector_Destruct (&This->LogData);
  Vector_Destruct (&This->EventHandles);
  Vector_Destruct (&This->LoadedImageHandles);
}

// ----------------------------------------------------------------------------
EFI_STATUS
DxeLoadingLog_SetObservingProtocols (
  DxeLoadingLog *This,
  ProtocolEntry *Protocols,
  UINTN         ProtocolCount
  )
{
  EFI_STATUS  Status;

  ScanHandleDB (This);

  for (UINTN I = 0; I < ProtocolCount; I++) {
    CheckProtocolExistence (This, &Protocols[I]);
  }

  for (UINTN I = 0; I < ProtocolCount; I++) {
    Protocols[I].Logger = This;
    Status = SubscribeToProtocolInstallation (This, &Protocols[I]);
    RETURN_ON_ERR (Status)
  }

  return EFI_SUCCESS;
}

// ----------------------------------------------------------------------------
VOID
DxeLoadingLog_QuitObserving (
  DxeLoadingLog *This
  )
{
  FOR_EACH_VCT(EFI_EVENT, Event, This->EventHandles)
  {
    gBS->CloseEvent (*Event);
  }

  Vector_Clear (&This->EventHandles);
}

// ----------------------------------------------------------------------------
VOID
CheckProtocolExistence (
  DxeLoadingLog *This,
  ProtocolEntry *Protocol
  )
{
  EFI_STATUS   Status;
  VOID         *Iface;
  LOG_ENTRY    LogEntry;

  Status = gBS->LocateProtocol (Protocol->Guid, NULL, &Iface);
  if (! EFI_ERROR (Status)) {
    LogEntry.Type                           = LOG_ENTRY_TYPE_PROTOCOL_EXISTANCE_ON_STARTUP;
    LogEntry.ProtocolExistence.ProtocolName = Protocol->Name;
    Vector_PushBack (&This->LogData, &LogEntry);
  }
}

// ----------------------------------------------------------------------------
EFI_STATUS
SubscribeToProtocolInstallation (
  DxeLoadingLog *This,
  ProtocolEntry *Protocol
  )
{
  EFI_STATUS   Status;
  EFI_EVENT    CallbackEvent;
  VOID         *Registration;

  Status = gBS->CreateEvent (
    EVT_NOTIFY_SIGNAL,
    TPL_CALLBACK,
    ProtocolInstalledCallback,
    Protocol,
    &CallbackEvent
    );
  RETURN_ON_ERR (Status)

  Status = gBS->RegisterProtocolNotify (
    Protocol->Guid,
    CallbackEvent,
    &Registration
    );
  RETURN_ON_ERR (Status)

  Vector_PushBack (&This->EventHandles, &CallbackEvent);

  return EFI_SUCCESS;
}

// ----------------------------------------------------------------------------

VOID
EFIAPI
ProtocolInstalledCallback (
  IN  EFI_EVENT  Event,
  IN  VOID       *Context
  )
{
  ProtocolEntry *Protocol = (ProtocolEntry *)Context;
  DxeLoadingLog *This     = (DxeLoadingLog *)Protocol->Logger;
  LOG_ENTRY LogEntry;

  EFI_TPL OldTpl = gBS->RaiseTPL (TPL_HIGH_LEVEL);

  if (CompareGuid (Protocol->Guid, &gEfiLoadedImageProtocolGuid)) {
    LogEntry.Type = LOG_ENTRY_TYPE_IMAGE_LOADED;
    DetectEntryImageNames (This, &LogEntry);
  } else {
    LogEntry.Type                           = LOG_ENTRY_TYPE_PROTOCOL_INSTALLED;
    LogEntry.ProtocolInstalled.ProtocolName = Protocol->Name;
  }

  Vector_PushBack (&This->LogData, &LogEntry);
  gBS->RestoreTPL (OldTpl);
}

// ----------------------------------------------------------------------------
VOID
ScanHandleDB (
  DxeLoadingLog *This
  )
{
  EFI_STATUS  Status;
  UINTN       HandleCount;
  EFI_HANDLE  *Handles = 0;

  // Ищем все протоколы gEfiLoadedImageProtocolGuid и логируем их.
  // Мы рассчитываем что в дальнейшем образы не будут выгружаться.
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiLoadedImageProtocolGuid,
                  NULL,
                  &HandleCount,
                  &Handles
                  );
  if (EFI_ERROR (Status)) {
    return;
  }

  for (UINTN Index = 0; Index < HandleCount; Index++) {
    Vector_PushBack (&This->LoadedImageHandles, &Handles[Index]);

    LOG_ENTRY LogEntry;
    LogEntry.Type = LOG_ENTRY_TYPE_IMAGE_LOADED;
    SetEntryImageNames (Handles[Index], &LogEntry);

    Vector_PushBack (&This->LogData, &LogEntry);
  }

  gBS->FreePool (Handles);
}

// ----------------------------------------------------------------------------
VOID
SetEntryImageNames(
  IN EFI_HANDLE Handle,
  OUT LOG_ENTRY *LogEntry
  )
{
  GetHandleName(Handle, LogEntry->ImageLoaded.ImageName);

  EFI_STATUS Status;

  EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
  Status = gBS->OpenProtocol (
              Handle,
              &gEfiLoadedImageProtocolGuid,
              (VOID **)&LoadedImage,
              gImageHandle,
              NULL,
              EFI_OPEN_PROTOCOL_GET_PROTOCOL
              );
  if (EFI_ERROR (Status)) {
    AsciiStrCpyS(
      LogEntry->ImageLoaded.ParentImageName,
      LOG_ENTRY_IMAGE_NAME_LENGTH,
      "<ERR: can\'t open protocol>"
      );

    return;
  }

  GetHandleName(LoadedImage->ParentHandle, LogEntry->ImageLoaded.ParentImageName);
}

// ----------------------------------------------------------------------------
VOID
DetectEntryImageNames(
  IN  DxeLoadingLog *This,
  OUT LOG_ENTRY     *LogEntry
  )
{
  EFI_STATUS  Status;
  UINTN       HandleCount;
  EFI_HANDLE  *Handles = 0;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiLoadedImageProtocolGuid,
                  NULL,
                  &HandleCount,
                  &Handles
                  );
  if (EFI_ERROR (Status)) {
    AsciiStrCpyS(
      LogEntry->ImageLoaded.ImageName,
      LOG_ENTRY_IMAGE_NAME_LENGTH,
      "<ERR: can\'t get handle buffer>"
      );
    return;
  }

  // Ищем новые хэндлы с LoadedImageProtocol:
  EFI_HANDLE NewImageHandle = NULL;
  BOOLEAN MultipleCandidates = FALSE;

  for (UINTN Index = 0; Index < HandleCount; Index++) {

    BOOLEAN HandleStored = FALSE;
    FOR_EACH_VCT (EFI_HANDLE, StoredHandle, This->LoadedImageHandles) {
      if (Handles[Index] == *StoredHandle) {
        HandleStored = TRUE;
        break;
      }
    }

    if (HandleStored) {
      continue;
    }

    Vector_PushBack (&This->LoadedImageHandles, &Handles[Index]);

    if (NewImageHandle == NULL) {
      // Первый кандидат.
      NewImageHandle = Handles[Index];
    } else {
      // Минимум два кандидата.
      MultipleCandidates = TRUE;
    }
  }

  if (MultipleCandidates) {
    // Несколько кандидатов.
    AsciiStrCpyS(
      LogEntry->ImageLoaded.ImageName,
      LOG_ENTRY_IMAGE_NAME_LENGTH,
      "<ERR: multiple candidates>"
      );
  }
  else if (NewImageHandle == NULL) {
    // Не нашли кандидатов вовсе.
    AsciiStrCpyS(
      LogEntry->ImageLoaded.ImageName,
      LOG_ENTRY_IMAGE_NAME_LENGTH,
      "<ERR: no one candidate>"
      );
  } else {
    // Всё ок, кандидат только один.
    SetEntryImageNames (NewImageHandle, LogEntry);
  }

  gBS->FreePool (Handles);
}

// ----------------------------------------------------------------------------
VOID
GetHandleName (
  IN EFI_HANDLE Handle,
  OUT CHAR8 *Name
  )
{
  EFI_STATUS Status;

  EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
  Status = gBS->OpenProtocol (
              Handle,
              &gEfiLoadedImageProtocolGuid,
              (VOID **)&LoadedImage,
              gImageHandle,
              NULL,
              EFI_OPEN_PROTOCOL_GET_PROTOCOL
              );
  if (EFI_ERROR (Status)) {
    AsciiStrCpyS(
      Name,
      LOG_ENTRY_IMAGE_NAME_LENGTH,
      "<ERR: can\'t open protocol>"
      );

    return;
  }

  CHAR16 *ImageName = FindLoadedImageFileName(LoadedImage);
  if (ImageName == NULL) {
    ImageName = ConvertDevicePathToText(LoadedImage->FilePath, TRUE, TRUE);
  }
  if (ImageName == NULL) {
    AsciiStrCpyS(
      Name,
      LOG_ENTRY_IMAGE_NAME_LENGTH,
      "<ERR: can\'t find name>"
      );

    return;
  }

  static CHAR8 AsciiBuffer[256];
  Status = UnicodeStrToAsciiStrS (
    ImageName,
    AsciiBuffer,
    256
    );
  if (EFI_ERROR (Status)) {
    AsciiStrCpyS(
      Name,
      LOG_ENTRY_IMAGE_NAME_LENGTH,
      "<ERR: can\'t decode>"
      );

    gBS->FreePool (ImageName);
    return;
  }

  if (LOG_ENTRY_IMAGE_NAME_LENGTH < StrLen(ImageName)) {
    AsciiBuffer[LOG_ENTRY_IMAGE_NAME_LENGTH - 2] = '*';
    AsciiBuffer[LOG_ENTRY_IMAGE_NAME_LENGTH - 1] = '\0';
  }

  AsciiStrCpyS(
    Name,
    LOG_ENTRY_IMAGE_NAME_LENGTH,
    AsciiBuffer
    );

  gBS->FreePool (ImageName);
}

// ----------------------------------------------------------------------------
/**
  Честно спижжено из ShellPkg.
  Function to find the file name associated with a LoadedImageProtocol.

  @param[in] LoadedImage     An instance of LoadedImageProtocol.

  @retval                    A string representation of the file name associated
                             with LoadedImage, or NULL if no name can be found.
**/
CHAR16*
FindLoadedImageFileName (
  IN EFI_LOADED_IMAGE_PROTOCOL *LoadedImage
  )
{
  EFI_GUID                       *NameGuid;
  EFI_STATUS                     Status;
  EFI_FIRMWARE_VOLUME2_PROTOCOL  *Fv;
  VOID                           *Buffer;
  UINTN                          BufferSize;
  UINT32                         AuthenticationStatus;

  if ((LoadedImage == NULL) || (LoadedImage->FilePath == NULL)) {
    return NULL;
  }

  NameGuid = EfiGetNameGuidFromFwVolDevicePathNode((MEDIA_FW_VOL_FILEPATH_DEVICE_PATH *)LoadedImage->FilePath);

  if (NameGuid == NULL) {
    return NULL;
  }

  //
  // Get the FirmwareVolume2Protocol of the device handle that this image was loaded from.
  //
  Status = gBS->HandleProtocol (LoadedImage->DeviceHandle, &gEfiFirmwareVolume2ProtocolGuid, (VOID**) &Fv);

  //
  // FirmwareVolume2Protocol is PI, and is not required to be available.
  //
  if (EFI_ERROR (Status)) {
    return NULL;
  }

  //
  // Read the user interface section of the image.
  //
  Buffer = NULL;
  Status = Fv->ReadSection(Fv, NameGuid, EFI_SECTION_USER_INTERFACE, 0, &Buffer, &BufferSize, &AuthenticationStatus);

  if (EFI_ERROR (Status)) {
    return NULL;
  }

  //
  // ReadSection returns just the section data, without any section header. For
  // a user interface section, the only data is the file name.
  //
  return Buffer;
}

// ----------------------------------------------------------------------------
