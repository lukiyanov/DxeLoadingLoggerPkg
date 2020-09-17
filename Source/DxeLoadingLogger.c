#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseLib.h>
#include <Library/EventLoggerLib.h>
#include <Library/CommonMacrosLib.h>

#include <Protocol/SimpleFileSystem.h>


// ----------------------------------------------------------------------------
static LOGGER             gLogger;
static EFI_FILE_PROTOCOL  *gLogFileProtocol;


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

// VOID
// ShowAndWriteLog (
//   IN  DxeLoadingLog *DxeLoadingLogInstance
//   );

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
EFI_STATUS
EFIAPI
Initialize (
  IN  EFI_HANDLE         ImageHandle,
  IN  EFI_SYSTEM_TABLE   *SystemTable
  )
{
  Logger_Construct (&gLogger);
  Logger_Start     (&gLogger);

  return EFI_SUCCESS;
}

// ----------------------------------------------------------------------------
EFI_STATUS
EFIAPI
Unload (
  IN EFI_HANDLE ImageHandle
  )
{
  // CloseLogFile ();
  Logger_Destruct (&gLogger);

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

  //DxeLoadingLog_QuitObserving(&gLoadingLogInstance);

  PrintToConsole(L"Writing the log...\r\n");
  //ShowAndWriteLog(&gLoadingLogInstance);
  CloseLogFile();
  PrintToConsole(L"Done writing the log.\r\n");

  //DxeLoadingLog_Destruct(&gLoadingLogInstance);
}

// ----------------------------------------------------------------------------
// VOID
// ShowAndWriteLog (
//   IN  DxeLoadingLog *Log
//   )
// {
//   PrintToLog(L"-- Begin --\r\n");

  // static CHAR16 UnicodeBuffer[LOG_ENTRY_IMAGE_NAME_LENGTH];
  // BOOLEAN LoadedImagesAfterUs = FALSE;
  // UINTN SpacesCount;

  // FOR_EACH_VCT(LOADING_EVENT, entry, Log->LogData) {
  //   switch (entry->Type) {
  //   case LOG_ENTRY_TYPE_PROTOCOL_INSTALLED:
  //     PrintToLog(L"- PROTOCOL-INSTALLED: ");
  //     PrintToLog(entry->ProtocolInstalled.ProtocolName);
  //     PrintToLog(L"\r\n");
  //     break;

  //   case LOG_ENTRY_TYPE_IMAGE_LOADED:
  //     if (LoadedImagesAfterUs) {
  //       PrintToLog(L"\r\n");
  //     }

  //     PrintToLog(L"- IMAGE-LOADED: ");
  //     AsciiStrToUnicodeStrS (
  //       entry->ImageLoaded.ImageName,
  //       UnicodeBuffer,
  //       LOG_ENTRY_IMAGE_NAME_LENGTH
  //       );
  //     PrintToLog(UnicodeBuffer);

  //     // Выравниваем пробелами чтобы лучше читалось:
  //     SpacesCount = LOG_ENTRY_IMAGE_NAME_LENGTH - 1 - StrLen(UnicodeBuffer);
  //     for (UINTN i = 0; i < SpacesCount; i++) {
  //       PrintToLog(L" ");
  //     }

  //     PrintToLog(L" loaded by: ");
  //     AsciiStrToUnicodeStrS (
  //       entry->ImageLoaded.ParentImageName,
  //       UnicodeBuffer,
  //       LOG_ENTRY_IMAGE_NAME_LENGTH
  //       );
  //     PrintToLog(UnicodeBuffer);
  //     PrintToLog(L"\r\n");

  //     if (LoadedImagesAfterUs) {
  //       PrintToLog(L"\r\n");
  //     }
  //     break;

  //   case LOG_ENTRY_TYPE_PROTOCOL_EXISTANCE_ON_STARTUP:
  //     PrintToLog(L"- EXSISTENCE-ON-STARTUP: ");
  //     PrintToLog(entry->ProtocolExistence.ProtocolName);
  //     PrintToLog(L"\r\n");
  //     LoadedImagesAfterUs = TRUE;
  //     break;

  //   default:
  //     PrintToLog(L"- UNKNOWN: Log entry with the unknown type.\r\n");
  //   }
  // }

//   PrintToLog(L"-- End --\r\n");
// }

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
