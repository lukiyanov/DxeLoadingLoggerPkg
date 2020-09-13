#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseLib.h>
#include <Protocol/SimpleFileSystem.h>

#include "guid_db.h"
#include "DxeLoadingLog.h"

// ----------------------------------------------------------------------------
#define RETURN_ON_ERR(Status) if (EFI_ERROR (Status)) { return Status; }
#define UNICODE_BUFFER_SIZE 256

// ----------------------------------------------------------------------------
EFI_STATUS
ExecuteFunctionOnProtocolAppearance (
  IN  EFI_GUID         *ProtocolGuid,
  IN  EFI_EVENT_NOTIFY CallbackFunction
  );

VOID
EFIAPI
ShowLogAndCleanup (
  IN  EFI_EVENT  Event,
  IN  VOID       *Context
  );

VOID
ShowAndWriteLog (
  IN  DxeLoadingLog *DxeLoadingLogInstance
  );

VOID
PrintToConsole (
  IN  CHAR16  *Str
  );

VOID
PrintToLog (
  IN  CHAR16  *Str
  );

EFI_STATUS
FindFileSystem (
  OUT  EFI_FILE_PROTOCOL  **LogFileProtocol
  );

VOID
CloseLogFile (
  );

// ----------------------------------------------------------------------------
// Выдрано из IDA неподалёку либо от запуска Shell, либо от StartImage():

static EFI_GUID gAmiUnknownProtocol_A = {
  0x73905351, 0xEB4D, 0x4637, { 0xA8, 0x3B, 0xD1, 0xBF, 0x6C, 0x1C, 0x48, 0xEB }
};

static EFI_GUID gAmiUnknownProtocol_B;
static EFI_GUID gAmiUnknownProtocol_C;

void Set_gAmiUnknownProtocol_B_C()
{
  gAmiUnknownProtocol_B.Data1 = 0x3677770F;
  gAmiUnknownProtocol_B.Data2 = 0xEFB2u;
  gAmiUnknownProtocol_B.Data3 = 0x43B2;
  gAmiUnknownProtocol_B.Data4[0] = 0xB8u;
  gAmiUnknownProtocol_B.Data4[1] = 0xAEu;
  gAmiUnknownProtocol_B.Data4[2] = 0xB3u;
  gAmiUnknownProtocol_B.Data4[3] = 2;
  gAmiUnknownProtocol_B.Data4[4] = 0xE9u;
  gAmiUnknownProtocol_B.Data4[5] = 0x60;
  gAmiUnknownProtocol_B.Data4[6] = 0x48;
  gAmiUnknownProtocol_B.Data4[7] = 0x82u;

  gAmiUnknownProtocol_C.Data1 = 0x8C12A959;
  gAmiUnknownProtocol_C.Data2 = 0x70BC;
  gAmiUnknownProtocol_C.Data3 = 0x4362;
  gAmiUnknownProtocol_C.Data4[0] = 0xB4u;
  gAmiUnknownProtocol_C.Data4[1] = 0x37;
  gAmiUnknownProtocol_C.Data4[2] = 0xB8u;
  gAmiUnknownProtocol_C.Data4[3] = 5;
  gAmiUnknownProtocol_C.Data4[4] = 0x14;
  gAmiUnknownProtocol_C.Data4[5] = 0xA1u;
  gAmiUnknownProtocol_C.Data4[6] = 0x91u;
  gAmiUnknownProtocol_C.Data4[7] = 0x6E;
}

// ----------------------------------------------------------------------------
static ProtocolEntry Protocols[] = {
  GUID_DB,
  { &gAmiUnknownProtocol_A, L"AMI_UNKNOWN_PROTOCOL_A" },
  { &gAmiUnknownProtocol_B, L"AMI_UNKNOWN_PROTOCOL_B" },
  { &gAmiUnknownProtocol_C, L"AMI_UNKNOWN_PROTOCOL_C" }
};

static DxeLoadingLog      gLoadingLogInstance;
static EFI_FILE_PROTOCOL  *gLogFileProtocol;

#define RELEASE_BUILD
// ----------------------------------------------------------------------------
EFI_STATUS
EFIAPI
Initialize (
  IN  EFI_HANDLE         ImageHandle,
  IN  EFI_SYSTEM_TABLE   *SystemTable
)
{
  EFI_STATUS Status;
  UINTN ProtocolCount = sizeof(Protocols) / sizeof(Protocols[0]);

  Set_gAmiUnknownProtocol_B_C();

#ifdef RELEASE_BUILD

  Status = ExecuteFunctionOnProtocolAppearance (
    &my_gEfiEventReadyToBootGuid,
    &ShowLogAndCleanup
    );
  RETURN_ON_ERR(Status)

  Status = ExecuteFunctionOnProtocolAppearance (
    &my_gLenovoSystemHiiDatabaseDxeGuid,  // Устанавливается перед входом в Settings.
    &ShowLogAndCleanup
    );
  RETURN_ON_ERR(Status)

#else
  // Запуск вручную с флешки на время тестирования.
  Status = ExecuteFunctionOnProtocolAppearance(
    &gEfiBootManagerPolicyProtocolGuid,
    &ShowLogAndCleanup
    );
  RETURN_ON_ERR(Status)

#endif

  DxeLoadingLog_Construct (&gLoadingLogInstance);

  Status = DxeLoadingLog_SetObservingProtocols (&gLoadingLogInstance, Protocols, ProtocolCount);
  RETURN_ON_ERR(Status)

  return EFI_SUCCESS;
}

// ----------------------------------------------------------------------------
EFI_STATUS
ExecuteFunctionOnProtocolAppearance (
  IN  EFI_GUID         *ProtocolGuid,
  IN  EFI_EVENT_NOTIFY CallbackFunction
  )
{
  EFI_STATUS   Status;
  EFI_EVENT    CallbackEvent;
  VOID         *Registration;

  Status = gBS->CreateEvent(
    EVT_NOTIFY_SIGNAL,
    TPL_CALLBACK,
    CallbackFunction,
    ProtocolGuid,
    &CallbackEvent
    );
  RETURN_ON_ERR(Status)

  Status = gBS->RegisterProtocolNotify(
    ProtocolGuid,
    CallbackEvent,
    &Registration
    );
  RETURN_ON_ERR(Status)

  return EFI_SUCCESS;
}

// ----------------------------------------------------------------------------
VOID
EFIAPI
ShowLogAndCleanup (
  IN  EFI_EVENT  Event,
  IN  VOID       *Context
  )
{
  gBS->CloseEvent (Event);

  DxeLoadingLog_QuitObserving(&gLoadingLogInstance);

  PrintToConsole(L"Writing the log...\r\n");
  ShowAndWriteLog(&gLoadingLogInstance);
  CloseLogFile();
  PrintToConsole(L"Done writing the log.\r\n");

  DxeLoadingLog_Destruct(&gLoadingLogInstance);
}

// ----------------------------------------------------------------------------
VOID
ShowAndWriteLog (
  IN  DxeLoadingLog *Log
  )
{
  PrintToLog(L"-- Begin --\r\n");

  static CHAR16 UnicodeBuffer[UNICODE_BUFFER_SIZE];
  BOOLEAN LoadedImagesAfterUs = FALSE;
  UINTN SpacesCount;

  FOR_EACH_VCT(LOG_ENTRY, entry, Log->LogData) {
    switch (entry->Type) {
    case LOG_ENTRY_TYPE_PROTOCOL_INSTALLED:
      PrintToLog(L"- PROTOCOL-INSTALLED: ");
      PrintToLog(entry->ProtocolInstalled.ProtocolName);
      PrintToLog(L"\r\n");
      break;

    case LOG_ENTRY_TYPE_IMAGE_LOADED:
      if (LoadedImagesAfterUs) {
        PrintToLog(L"\r\n");
      }

      PrintToLog(L"- IMAGE-LOADED: ");
      AsciiStrToUnicodeStrS (
        entry->ImageLoaded.ImageName,
        UnicodeBuffer,
        UNICODE_BUFFER_SIZE
        );
      PrintToLog(UnicodeBuffer);

      // Выравниваем пробелами чтобы лучше читалось:
      SpacesCount = LOG_ENTRY_IMAGE_NAME_LENGTH - 1 - StrLen(UnicodeBuffer);
      for (UINTN i = 0; i < SpacesCount; i++) {
        PrintToLog(L" ");
      }

      PrintToLog(L" loaded by: ");
      AsciiStrToUnicodeStrS (
        entry->ImageLoaded.ParentImageName,
        UnicodeBuffer,
        UNICODE_BUFFER_SIZE
        );
      PrintToLog(UnicodeBuffer);
      PrintToLog(L"\r\n");

      if (LoadedImagesAfterUs) {
        PrintToLog(L"\r\n");
      }
      break;

    case LOG_ENTRY_TYPE_PROTOCOL_EXISTANCE_ON_STARTUP:
      PrintToLog(L"- EXSISTENCE-ON-STARTUP: ");
      PrintToLog(entry->ProtocolExistence.ProtocolName);
      PrintToLog(L"\r\n");
      LoadedImagesAfterUs = TRUE;
      break;

    default:
      PrintToLog(L"- UNKNOWN: Log entry with the unknown type.\r\n");
    }
  }

  PrintToLog(L"-- End --\r\n");
}

// ----------------------------------------------------------------------------
VOID
PrintToConsole (
  IN  CHAR16  *Str
  )
{
  if (gST->ConOut) {
    gST->ConOut->OutputString(gST->ConOut, Str);
  }
}

// ----------------------------------------------------------------------------
VOID
PrintToLog (
  IN  CHAR16  *Str
  )
{
  if (gLogFileProtocol == NULL) {
    FindFileSystem (&gLogFileProtocol);
  }

  if (gLogFileProtocol) {
    UINTN Length = StrLen(Str) * sizeof(CHAR16);

    gLogFileProtocol->Write(
      gLogFileProtocol,
      &Length,
      Str
      );

    // Это очень замедляет запись лога, но защищает от внезапных перезагрузок.
    // gLogFileProtocol->Flush(
    //   gLogFileProtocol
    //   );
  }
}

// ----------------------------------------------------------------------------
EFI_STATUS
FindFileSystem (
  OUT  EFI_FILE_PROTOCOL  **LogFileProtocol
  )
{
  EFI_STATUS  Status;
  UINTN       HandleCount;
  EFI_HANDLE  *Handles = 0;

  PrintToConsole(L"Searching a file system with the file \"log.txt\"...\r\n");

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiSimpleFileSystemProtocolGuid,
                  NULL,
                  &HandleCount,
                  &Handles
                  );
  RETURN_ON_ERR (Status)

  for (UINTN Index = 0; Index < HandleCount; Index++) {
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystemProtocol = NULL;

    // Получаем EFI_SIMPLE_FILE_SYSTEM_PROTOCOL.
    Status = gBS->OpenProtocol (
                    Handles[Index],
                    &gEfiSimpleFileSystemProtocolGuid,
                    (VOID **)&FileSystemProtocol,
                    gImageHandle,
                    NULL,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );
    if (EFI_ERROR (Status)) {
      continue;
    }

    PrintToConsole(L"Simple FS found...\r\n");

    // Получаем корень файловой системы...
    EFI_FILE_PROTOCOL *FileSystemRoot = NULL;
    Status = FileSystemProtocol->OpenVolume(
        FileSystemProtocol,
        &FileSystemRoot
        );
    if (EFI_ERROR (Status)) {
      continue;
    }

    EFI_FILE_PROTOCOL *File = NULL;

    // Ищем log.txt.
    Status = FileSystemRoot->Open(
                      FileSystemRoot,
                      &File,
                      L"log.txt",
                      EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE,
                      0
                      );
    if (EFI_ERROR (Status)) {
      // Не нашли.
      PrintToConsole(L"No log.txt file, skipping...\r\n");
      FileSystemRoot->Close(FileSystemRoot);
      continue;
    }

    // Нашли, удаляем его и создаём новый.
    PrintToConsole(L"File log.txt found...\r\n");
    Status = File->Delete(File);

    Status = FileSystemRoot->Open(
                      FileSystemRoot,
                      &File,
                      L"log.txt",
                      EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE,
                      0
                      );
    if (EFI_ERROR (Status)) {
      PrintToConsole(L"Can\t create log.txt.\r\n");
      FileSystemRoot->Close(FileSystemRoot);
      continue;
    }

    // Готово, можно начинать писать в файл.
    PrintToConsole(L"log.txt opened.\r\n");
    *LogFileProtocol = File;
    gBS->FreePool (Handles);
    return EFI_SUCCESS;
  }

  PrintToConsole(L"Simple FS not found =(\r\n");
  gBS->FreePool (Handles);
  return EFI_NOT_FOUND;
}

// ----------------------------------------------------------------------------
VOID
CloseLogFile (
  )
{
  if (gLogFileProtocol) {
    gLogFileProtocol->Flush(gLogFileProtocol);
    gLogFileProtocol->Close(gLogFileProtocol);
    gLogFileProtocol = NULL;
  }
}

// ----------------------------------------------------------------------------
