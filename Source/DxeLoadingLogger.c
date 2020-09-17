#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/PcdLib.h>
#include <Library/EventLoggerLib.h>
#include <Library/CommonMacrosLib.h>
#include <Library/VectorLib.h>

#include <Protocol/SimpleFileSystem.h>


// -----------------------------------------------------------------------------
static LOGGER             gLogger;
static EFI_FILE_PROTOCOL  *gLogFileProtocol;
static VECTOR             gPreviousFsHandles;

static GLOBAL_REMOVE_IF_UNREFERENCED EFI_EVENT  gEventUpdateLog;

// -----------------------------------------------------------------------------
/**
 * Проверяет существование новых событий и при наличии обрабатывает их.
 * Взаимоисключающий способ обработки событий с ProcessNewEvent().
*/
VOID
EFIAPI
CheckAndProcessNewEvents (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  );

// -----------------------------------------------------------------------------
/**
 * Обрабатывает поступление нового события.
 * Взаимоисключающий способ обработки событий с CheckAndProcessNewEvents().
*/
VOID
ProcessNewEvent (
  IN LOADING_EVENT *Event
  );

// -----------------------------------------------------------------------------
/**
 * Дописывает конкретное событие в лог.
 *
 * @retval TRUE   Информация о событии успешно дописана в лог.
 * @retval FALSE  Ошибка записи, информация о событии не записана.
*/
BOOLEAN
AddNewEventToLog (
  IN LOADING_EVENT      *Event,
  IN EFI_FILE_PROTOCOL  *gLogFileProtocol
  );

// -----------------------------------------------------------------------------
/**
 * Пытается создать лог-файл.
 * В случае успеха возвращает указатель на протокол, с помощью которого можно вести запись данного файла.
 *
 * @param LogFileProtocol  Протокол, с помощью которого можно вести запись лог-файла.
 *
 * @retval EFI_SUCCESS     Файловая система найдена, результат записан в LogFileProtocol.
 * @retval Что-то другое.  Какая-то ошибка, LogFileProtocol остался без изменений.
*/
EFI_STATUS
FindFileSystem (
  OUT  EFI_FILE_PROTOCOL  **LogFileProtocol
  );

// -----------------------------------------------------------------------------
/**
 * Проверяет, есть ли среди Handles новые хэндлы.
 *
 * @param Handles     Массив хэндлов.
 * @param HandleCount Количество элементов в Handles.
 *
 * @retval TRUE   Обнаружены новые хэндлы.
 * @retval FALSE  Новых нет.
*/
BOOLEAN
CheckForNewFsHandles(
  IN EFI_HANDLE  *Handles,
  IN UINTN       HandleCount
  );

// -----------------------------------------------------------------------------
/**
 * Корректно закрывает FileProtocol, если это ещё не было сделано.
*/
VOID
FlushAndCloseFileProtocol (
  IN OUT EFI_FILE_PROTOCOL **FileProtocol
  );


// -----------------------------------------------------------------------------
/**
 * Точка входа драйвера.
*/
EFI_STATUS
EFIAPI
Initialize (
  IN  EFI_HANDLE         ImageHandle,
  IN  EFI_SYSTEM_TABLE   *SystemTable
  )
{
  DBG_ENTER ();

  if (FeaturePcdGet (PcdFlushEveryEventEnabled)) {
    Logger_Construct (&gLogger, &ProcessNewEvent);
  } else {
    Logger_Construct (&gLogger, NULL);

    EFI_STATUS Status;

    Status = gBS->CreateEvent (
                    EVT_TIMER | EVT_NOTIFY_SIGNAL,
                    TPL_NOTIFY,
                    CheckAndProcessNewEvents,
                    NULL,
                    &gEventUpdateLog
                    );
    RETURN_ON_ERR (Status);

    Status = gBS->SetTimer (
                    gEventUpdateLog,
                    TimerPeriodic,
                    EFI_TIMER_PERIOD_MILLISECONDS (1000)
                    );
    RETURN_ON_ERR (Status);
  }

  Vector_Construct (&gPreviousFsHandles, sizeof(EFI_HANDLE), 32);
  Logger_Start     (&gLogger);

  DBG_EXIT_STATUS (EFI_SUCCESS);
  return EFI_SUCCESS;
}

// -----------------------------------------------------------------------------
/**
 * Вызывается при необходимости выгрузить драйвер из памяти.
*/
EFI_STATUS
EFIAPI
Unload (
  IN EFI_HANDLE ImageHandle
  )
{
  DBG_ENTER ();

  // CloseLogFile ();

  if (FeaturePcdGet (PcdFlushEveryEventEnabled)) {
    gBS->CloseEvent (gEventUpdateLog);
  }

  Logger_Destruct (&gLogger);
  Vector_Destruct (&gPreviousFsHandles);

  FlushAndCloseFileProtocol(&gLogFileProtocol);

  DBG_EXIT_STATUS (EFI_SUCCESS);
  return EFI_SUCCESS;
}

// -----------------------------------------------------------------------------
/**
 * Проверяет существование новых событий и при наличии обрабатывает их.
 * Взаимоисключающий способ обработки событий с ProcessNewEvent().
*/
VOID
EFIAPI
CheckAndProcessNewEvents (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  // DBG_ENTER ();

  //
  // TODO
  //

  // DBG_EXIT ();
}

// -----------------------------------------------------------------------------
/**
 * Обрабатывает поступление нового события.
 * Взаимоисключающий способ обработки событий с CheckAndProcessNewEvents().
*/
VOID
ProcessNewEvent (
  LOADING_EVENT *Event
  )
{
  DBG_ENTER ()

  if (gLogFileProtocol == NULL) {
    EFI_STATUS Status;
    Status = FindFileSystem (&gLogFileProtocol);
    if (EFI_ERROR (Status)) {
      DBG_EXIT_STATUS (EFI_NOT_READY);
      return;
    }
  }

  DBG_INFO1 ("TODO: write the event to the log\n");
  DBG_EXIT ();
}

// -----------------------------------------------------------------------------
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

// -----------------------------------------------------------------------------
// VOID
// PrintToLog (
//   IN  CHAR16  *Str
//   )
// {
//   if (gLogFileProtocol == NULL) {
//     FindFileSystem (&gLogFileProtocol);
//   }

//   if (gLogFileProtocol) {
//     UINTN Length = StrLen(Str) * sizeof(CHAR16);

//     gLogFileProtocol->Write(
//       gLogFileProtocol,
//       &Length,
//       Str
//       );

//     // Это очень замедляет запись лога, но защищает от внезапных перезагрузок.
//     // gLogFileProtocol->Flush(
//     //   gLogFileProtocol
//     //   );
//   }
// }

// -----------------------------------------------------------------------------
/**
 * Пытается создать лог-файл.
 * В случае успеха возвращает указатель на протокол, с помощью которого можно вести запись данного файла.
 *
 * @param LogFileProtocol  Протокол, с помощью которого можно вести запись лог-файла.
 *
 * @retval EFI_SUCCESS     Файловая система найдена, результат записан в LogFileProtocol.
 * @retval Что-то другое.  Какая-то ошибка, LogFileProtocol остался без изменений.
*/
EFI_STATUS
FindFileSystem (
  OUT  EFI_FILE_PROTOCOL  **LogFileProtocol
  )
{
  DBG_ENTER ()

  // Принцип поиска: выбираем первую же файловую систему,
  // содержащую в корне файл "log.txt" и доступную для записи.

  EFI_STATUS  Status;
  UINTN       HandleCount;
  EFI_HANDLE  *Handles = NULL;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiSimpleFileSystemProtocolGuid,
                  NULL,
                  &HandleCount,
                  &Handles
                  );
  RETURN_ON_ERR (Status)

  if (!CheckForNewFsHandles(Handles, HandleCount)) {
    DBG_EXIT_STATUS (EFI_NOT_FOUND)
    return FALSE;
  }

  DBG_INFO1 ("New fs found. Searching a file system with the file \"log.txt\"...\n");

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

    DBG_INFO1 ("Simple FS is found...\n");

    // Получаем корень файловой системы.
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
      DBG_INFO1 ("log.txt is not found, skipping...\n");
      FileSystemRoot->Close(FileSystemRoot);
      continue;
    }

    // Нашли, удаляем его и создаём новый.
    DBG_INFO1 ("log.txt is found...\n");
    Status = File->Delete(File);

    Status = FileSystemRoot->Open(
                      FileSystemRoot,
                      &File,
                      L"log.txt",
                      EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE,
                      0
                      );
    if (EFI_ERROR (Status)) {
      DBG_INFO1 ("Can\t create log.txt.\n");
      FileSystemRoot->Close(FileSystemRoot);
      continue;
    }

    // Готово, можно начинать писать в файл.
    DBG_INFO1 ("log.txt is opened.\n");
    *LogFileProtocol = File;

    SHELL_FREE_NON_NULL (Handles);
    DBG_EXIT_STATUS (EFI_SUCCESS)
    return EFI_SUCCESS;
  }

  DBG_INFO1 ("Simple FS is not found =(\n");

  SHELL_FREE_NON_NULL (Handles);
  DBG_EXIT_STATUS (EFI_NOT_FOUND)
  return EFI_NOT_FOUND;
}

// -----------------------------------------------------------------------------
/**
 * Проверяет, есть ли среди Handles новые хэндлы.
 *
 * @param Handles     Массив хэндлов.
 * @param HandleCount Количество элементов в Handles.
 *
 * @retval TRUE   Обнаружены новые хэндлы.
 * @retval FALSE  Новых нет.
*/
BOOLEAN
CheckForNewFsHandles(
  IN EFI_HANDLE  *Handles,
  IN UINTN       HandleCount
  )
{
  DBG_ENTER ()

  BOOLEAN NewFsFound = FALSE;
  for (UINTN Index = 0; Index < HandleCount; ++Index) {
    BOOLEAN HandleIsAlreadyStored = FALSE;

    FOR_EACH_VCT (EFI_HANDLE, FsHandle, gPreviousFsHandles) {
      if (*FsHandle == Handles[Index]) {
        HandleIsAlreadyStored = TRUE;
      }
    }

    if (!HandleIsAlreadyStored) {
      NewFsFound = TRUE;
      break;
    }
  }

  if (!NewFsFound) {
    DBG_EXIT_STATUS (EFI_NOT_FOUND)
    return FALSE;
  }

  Vector_Clear (&gPreviousFsHandles);
  for (UINTN Index = 0; Index < HandleCount; Index++) {
    Vector_PushBack (&gPreviousFsHandles, &Handles[Index]);
  }

  DBG_EXIT ();
  return TRUE;
}

// -----------------------------------------------------------------------------
/**
 * Корректно закрывает FileProtocol, если это ещё не было сделано.
*/
VOID
FlushAndCloseFileProtocol (
  IN OUT EFI_FILE_PROTOCOL **FileProtocol
  )
{
  if (FileProtocol && *FileProtocol) {
    (*FileProtocol)->Flush(*FileProtocol);
    (*FileProtocol)->Close(*FileProtocol);
    *FileProtocol = NULL;
  }
}

// -----------------------------------------------------------------------------
