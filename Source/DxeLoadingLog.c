

// ----------------------------------------------------------------------------
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
  OUT LOADING_EVENT *LogEntry
  );

VOID
DetectEntryImageNames (
  IN  DxeLoadingLog *This,
  OUT LOADING_EVENT *Entry
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
  LOADING_EVENT LogEntry;

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


VOID
DetectEntryImageNames(
  IN  DxeLoadingLog *This,
  OUT LOADING_EVENT     *LogEntry
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

