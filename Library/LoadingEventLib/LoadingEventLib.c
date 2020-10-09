#include <Library/LoadingEventLib.h>
#include <Library/CommonMacrosLib.h>
#include <Library/UefiBootServicesTableLib.h>

// -----------------------------------------------------------------------------
/**
 * Корректно освобождает память из-под всех указателей в Event, не равных NULL.
*/
VOID
LoadingEvent_Destruct (
  LOADING_EVENT *Event
  )
{
  if (Event == NULL) {
    return;
  }

  switch (Event->Type)
  {
  case LOG_ENTRY_TYPE_PROTOCOL_INSTALLED:
    SHELL_FREE_NON_NULL (Event->ProtocolInstalled.HandleDescription);
    break;

  case LOG_ENTRY_TYPE_PROTOCOL_REINSTALLED:
    SHELL_FREE_NON_NULL (Event->ProtocolReinstalled.HandleDescription);
    break;

  case LOG_ENTRY_TYPE_PROTOCOL_REMOVED:
    SHELL_FREE_NON_NULL (Event->ProtocolRemoved.HandleDescription);
    break;

  case LOG_ENTRY_TYPE_PROTOCOL_EXISTS_ON_STARTUP:
    SHELL_FREE_NON_NULL (Event->ProtocolExistsOnStartup.HandleDescription);
    break;

  case LOG_ENTRY_TYPE_IMAGE_LOADED:
    SHELL_FREE_NON_NULL (Event->ImageLoaded.ImageName);
    SHELL_FREE_NON_NULL (Event->ImageLoaded.ParentImageName);
    break;

  case LOG_ENTRY_TYPE_IMAGE_EXISTS_ON_STARTUP:
    SHELL_FREE_NON_NULL (Event->ImageExistsOnStartup.ImageName);
    SHELL_FREE_NON_NULL (Event->ImageExistsOnStartup.ParentImageName);
    break;

  case LOG_ENTRY_TYPE_BDS_STAGE_ENTERED:
    // Нечего освобождать.
    break;

  case LOG_ENTRY_TYPE_ERROR:
    SHELL_FREE_NON_NULL (Event->Error.Message);
    break;

  default:
    DBG_ERROR1 ("ERROR: Unknown event type\n");
    break;
  }
}

// -----------------------------------------------------------------------------
