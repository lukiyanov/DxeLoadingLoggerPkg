#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PrintLib.h>
#include <Library/EventLoggerLib.h>
#include <Library/CommonMacrosLib.h>
#include <Library/VectorLib.h>
#include <Library/ProtocolGuidDatabaseLib.h>
#include <Library/TextAnimationLib.h>

#include <Protocol/SimpleFileSystem.h>


// -----------------------------------------------------------------------------
#define PRINT_TO_FILE_BUFFER_LENGTH 4096


// -----------------------------------------------------------------------------
STATIC LOGGER             gLogger;
STATIC EFI_FILE_PROTOCOL  *gLogFileProtocol;


// -----------------------------------------------------------------------------
/**
 * Обрабатывает поступление новых событий, записывая их в лог.
*/
VOID
ProcessNewEvents ();

// -----------------------------------------------------------------------------
/**
 * Дописывает информацию о событии Event в gLogFileProtocol.
 * В случае неудачи устанавливает gLogFileProtocol в NULL.
*/
VOID
AddNewEventToLog (
  IN     LOADING_EVENT      *Event,
  IN     UINTN              EventNumber,
  IN OUT EFI_FILE_PROTOCOL  **gLogFileProtocol
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
 * Печатает в файл.
 * Если не удалось, то устанавливает FileProtocol в NULL.
*/
VOID
EFIAPI
PrintToFile (
  IN OUT EFI_FILE_PROTOCOL **FileProtocol,
  IN     CHAR16            *Format,
  ...
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

  Logger_Construct (&gLogger, &ProcessNewEvents);
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

  Logger_Destruct (&gLogger);
  FlushAndCloseFileProtocol(&gLogFileProtocol);

  DBG_EXIT_STATUS (EFI_SUCCESS);
  return EFI_SUCCESS;
}

// -----------------------------------------------------------------------------
/**
 * Обрабатывает поступление нового события.
*/
VOID
ProcessNewEvents ()
{
  DBG_ENTER ()

  STATIC UINTN gLoggedEventCount;
  EFI_STATUS Status;

  if (gLogFileProtocol == NULL) {
    // В случае если у нас отняли флешку начинаем писать лог с начала.
    gLoggedEventCount = 0;

    Status = FindFileSystem (&gLogFileProtocol);
    if (EFI_ERROR (Status)) {
      DBG_EXIT_STATUS (Status);
      return;
    }
  }

  StartPlayingAnimation ();

  for (UINTN EventCount = Logger_GetEventCount (&gLogger); gLoggedEventCount < EventCount; ++gLoggedEventCount) {
    LOADING_EVENT Event;
    Status = Logger_GetEvent (&gLogger, gLoggedEventCount, &Event);
    if (EFI_ERROR (Status)) {
      // Такого происходить не должно вообще никогда.
      StopPlayingAnimation();
      DBG_EXIT_STATUS (Status);
      return;
    }

    AddNewEventToLog(&Event, gLoggedEventCount + 1, &gLogFileProtocol);
    UpdatePlayingAnimation ();

    if (gLogFileProtocol == NULL) {
      // Флешку вынули во время записи.
      // Ничего страшного, запишем лог заново когда её снова подключат.
      break;
    }
  }

  // Если у нас не вынули флешку, то flush'им лог.
  if (gLogFileProtocol) {
    gLogFileProtocol->Flush(gLogFileProtocol);
  }

  StopPlayingAnimation ();

  DBG_EXIT ();
}

// -----------------------------------------------------------------------------
/**
 * Дописывает информацию о событии Event в gLogFileProtocol.
 * В случае неудачи устанавливает gLogFileProtocol в NULL.
*/
VOID
AddNewEventToLog (
  IN     LOADING_EVENT      *Event,
  IN     UINTN              EventNumber,
  IN OUT EFI_FILE_PROTOCOL  **gLogFileProtocol
  )
{
  STATIC CHAR16 *StrUnknown = L"<UNKNOWN>";
  unsigned Number = EventNumber;

  switch (Event->Type)
  {
  case LOG_ENTRY_TYPE_PROTOCOL_INSTALLED:
    {
      CHAR16 *HandleDescription = Event->ProtocolInstalled.HandleDescription;
      CHAR16 *GuidName          = GetProtocolName (&Event->ProtocolInstalled.Guid);
      CHAR16 *Success           = NULL;

      if (Event->ProtocolInstalled.Successful) {
        Success = L"SUCCESS";
      } else {
        Success = L"FAIL";
      }

      if (GuidName != NULL) {
        PrintToFile (gLogFileProtocol, L"-%5u- PROTOCOL-INSTALLED (%s): %-60s", Number, Success,
          GuidName
          );
      } else {
        PrintToFile (gLogFileProtocol, L"-%5u- PROTOCOL-INSTALLED (%s): %-60g", Number, Success,
          &Event->ProtocolInstalled.Guid
          );
      }

      if (HandleDescription != NULL) {
        PrintToFile (gLogFileProtocol, L" at: %s", HandleDescription);
      }

      PrintToFile (gLogFileProtocol, L"\r\n");
    }
    break;

  case LOG_ENTRY_TYPE_PROTOCOL_REINSTALLED:
    {
      CHAR16 *HandleDescription = Event->ProtocolReinstalled.HandleDescription;
      CHAR16 *GuidName          = GetProtocolName (&Event->ProtocolReinstalled.Guid);
      CHAR16 *Success           = NULL;

      if (Event->ProtocolReinstalled.Successful) {
        Success = L"SUCCESS";
      } else {
        Success = L"FAIL";
      }

      if (GuidName != NULL) {
        PrintToFile (gLogFileProtocol, L"-%5u- PROTOCOL-REINSTALLED (%s): %-60s", Number, Success,
          GuidName
          );
      } else {
        PrintToFile (gLogFileProtocol, L"-%5u- PROTOCOL-REINSTALLED (%s): %-60g", Number, Success,
          &Event->ProtocolReinstalled.Guid
          );
      }

      if (HandleDescription != NULL) {
        PrintToFile (gLogFileProtocol, L" at: %s", HandleDescription);
      }

      PrintToFile (gLogFileProtocol, L"\r\n");
    }
    break;

  case LOG_ENTRY_TYPE_PROTOCOL_EXISTS_ON_STARTUP:
    {
      CHAR16 *GuidName = GetProtocolName(&Event->ProtocolExistsOnStartup.Guid);

      if (Event->ProtocolExistsOnStartup.HandleDescription != NULL) {
        if (GuidName != NULL) {
          PrintToFile (gLogFileProtocol, L"-%5u- PROTOCOL-EXISTS-ON-STARTUP: %-60s at: %s\r\n", Number,
            GuidName,
            Event->ProtocolExistsOnStartup.HandleDescription
            );
        } else {
          PrintToFile (gLogFileProtocol, L"-%5u- PROTOCOL-EXISTS-ON-STARTUP: %-60g at: %s\r\n", Number,
            &Event->ProtocolExistsOnStartup.Guid,
            Event->ProtocolExistsOnStartup.HandleDescription
            );
        }
      } else {
        if (GuidName != NULL) {
          PrintToFile (gLogFileProtocol, L"-%5u- PROTOCOL-EXISTS-ON-STARTUP: %s\r\n", Number,
            GuidName
            );
        } else {
          PrintToFile (gLogFileProtocol, L"-%5u- PROTOCOL-EXISTS-ON-STARTUP: %g\r\n", Number,
            &Event->ProtocolExistsOnStartup.Guid
            );
        }
      }
    }
    break;

  case LOG_ENTRY_TYPE_PROTOCOL_REMOVED:
    {
      CHAR16 *HandleDescription = Event->ProtocolRemoved.HandleDescription;
      CHAR16 *GuidName          = GetProtocolName (&Event->ProtocolRemoved.Guid);
      CHAR16 *Success           = NULL;

      if (Event->ProtocolRemoved.Successful) {
        Success = L"SUCCESS";
      } else {
        Success = L"FAIL";
      }

      if (GuidName != NULL) {
        PrintToFile (gLogFileProtocol, L"-%5u- PROTOCOL-REMOVED (%s): %-60s", Number, Success,
          GuidName
          );
      } else {
        PrintToFile (gLogFileProtocol, L"-%5u- PROTOCOL-REMOVED (%s): %-60g", Number, Success,
          &Event->ProtocolRemoved.Guid
          );
      }

      if (HandleDescription != NULL) {
        PrintToFile (gLogFileProtocol, L" at: %s", HandleDescription);
      }

      PrintToFile (gLogFileProtocol, L"\r\n");
    }
    break;

  case LOG_ENTRY_TYPE_IMAGE_LOADED:
    {
      CHAR16 *ImageName  = Event->ImageLoaded.ImageName       ? Event->ImageLoaded.ImageName       : StrUnknown;
      CHAR16 *ParentName = Event->ImageLoaded.ParentImageName ? Event->ImageLoaded.ParentImageName : StrUnknown;

      PrintToFile (gLogFileProtocol, L"\r\n-%5u- IMAGE-LOADED: %-60s loaded by: %s\r\n",
        Number, ImageName, ParentName
        );
    }
    break;

  case LOG_ENTRY_TYPE_IMAGE_EXISTS_ON_STARTUP:
    {
      LOG_ENTRY_IMAGE_EXISTS_ON_STARTUP *ImgExists = &Event->ImageExistsOnStartup;
      CHAR16 *ImageName  = ImgExists->ImageName       ? ImgExists->ImageName       : StrUnknown;
      CHAR16 *ParentName = ImgExists->ParentImageName ? ImgExists->ParentImageName : StrUnknown;

      PrintToFile (gLogFileProtocol, L"-%5u- IMAGE-EXISTS-ON-STARTUP: %-60s loaded by: %s\r\n",
        Number, ImageName, ParentName
        );
    }
    break;

  case LOG_ENTRY_TYPE_BDS_STAGE_ENTERED:
    {
      CHAR16 *SubType = NULL;

      switch (Event->BdsStageEntered.SubEvent) {
      case BDS_STAGE_EVENT_BEFORE_ENTRY_CALLING:
        SubType = L"BEFORE";
        break;
      case BDS_STAGE_EVENT_AFTER_ENTRY_CALLING:
        SubType = L"AFTER";
        break;
      default:
        SubType = L"<ERROR: Unknown SubEvent type>";
        break;
      }

      STATIC CHAR16 Line[] = L"- --------------------------------------------------------------------------------\r\n";
      PrintToFile (gLogFileProtocol, Line);
      PrintToFile (gLogFileProtocol, L"-%5u- BDS-STAGE-ENTERED: %s\r\n", Number, SubType);
      PrintToFile (gLogFileProtocol, Line);
    }
    break;

  case LOG_ENTRY_TYPE_ERROR:
    {
      CHAR16 *Message = Event->Error.Message ? Event->Error.Message : StrUnknown;

      PrintToFile (gLogFileProtocol, L"\r\n\r\n-%5u- ERROR: %s\r\n\r\n", Number, Message);
    }
    break;

  default:
    PrintToFile (gLogFileProtocol, L"\r\n\r\n-%5u- ERROR: Unknown event type\r\n\r\n\r\n", Number);
    break;
  }
}

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
    FileSystemRoot->Close(FileSystemRoot);
    if (EFI_ERROR (Status)) {
      DBG_INFO1 ("Can\t create log.txt.\n");
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
 * Печатает в файл.
 * Если не удалось, то устанавливает FileProtocol в NULL.
*/
VOID
EFIAPI
PrintToFile (
  IN OUT EFI_FILE_PROTOCOL **FileProtocol,
  IN     CHAR16            *Format,
  ...
  )
{
  DBG_ENTER ();

  STATIC CHAR16 Buffer[PRINT_TO_FILE_BUFFER_LENGTH];

  if (FileProtocol == NULL || *FileProtocol == NULL) {
    DBG_EXIT_STATUS (EFI_ABORTED);
    return;
  }

  VA_LIST Marker;
  VA_START (Marker, Format);
  UINTN Length = UnicodeVSPrint (Buffer, PRINT_TO_FILE_BUFFER_LENGTH * sizeof(CHAR16), Format, Marker) * sizeof(CHAR16);
  VA_END (Marker);

  EFI_STATUS Status;
  Status = (*FileProtocol)->Write(
                            (*FileProtocol),
                            &Length,
                            Buffer
                            );
  if (EFI_ERROR (Status)) {
    (*FileProtocol)->Close(*FileProtocol);
    *FileProtocol = NULL;

    DBG_EXIT_STATUS (EFI_ACCESS_DENIED);
    return;
  }

  DBG_EXIT ();
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
