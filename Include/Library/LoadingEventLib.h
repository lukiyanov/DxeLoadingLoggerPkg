/** @file
 * Содержит описание типов событий, учёт которых мы ведём.
 */
#include <Uefi.h>

#ifndef LOG_EVENT_LIB_H_
#define LOG_EVENT_LIB_H_

// -----------------------------------------------------------------------------
typedef enum {
  LOG_ENTRY_TYPE_PROTOCOL_INSTALLED,
  LOG_ENTRY_TYPE_PROTOCOL_REINSTALLED,
  LOG_ENTRY_TYPE_PROTOCOL_REMOVED,
  LOG_ENTRY_TYPE_PROTOCOL_EXISTS_ON_STARTUP,
  LOG_ENTRY_TYPE_IMAGE_LOADED,
  LOG_ENTRY_TYPE_IMAGE_EXISTS_ON_STARTUP,
  LOG_ENTRY_TYPE_BDS_STAGE_ENTERED,
  LOG_ENTRY_TYPE_ERROR
} LOG_ENTRY_TYPE;

// -----------------------------------------------------------------------------
typedef PACKED struct {
  GUID    Guid;
  BOOLEAN Successful;
  CHAR16  *HandleDescription;
} LOG_ENTRY_PROTOCOL_INSTALLED, LOG_ENTRY_PROTOCOL_REINSTALLED, LOG_ENTRY_PROTOCOL_REMOVED;

// -----------------------------------------------------------------------------
typedef PACKED struct {
  GUID    Guid;
  CHAR16  *HandleDescription;
} LOG_ENTRY_PROTOCOL_EXISTS_ON_STARTUP;

// -----------------------------------------------------------------------------
typedef PACKED struct {
  CHAR16  *ImageName;
  CHAR16  *ParentImageName;
} LOG_ENTRY_IMAGE_LOADED, LOG_ENTRY_IMAGE_EXISTS_ON_STARTUP;

// -----------------------------------------------------------------------------
typedef enum {
  BDS_STAGE_EVENT_BEFORE_ENTRY_CALLING  = 0,
  BDS_STAGE_EVENT_AFTER_ENTRY_CALLING   = 1
} BDS_STAGE_SUB_EVENT_INFO;

// -----------------------------------------------------------------------------
typedef PACKED struct {
  BDS_STAGE_SUB_EVENT_INFO  SubEvent;
} LOG_ENTRY_BDS_STAGE_ENTERED;

// -----------------------------------------------------------------------------
typedef PACKED struct {
  CHAR16  *Message;
} LOG_ENTRY_ERROR;

// -----------------------------------------------------------------------------
typedef PACKED struct {
  LOG_ENTRY_TYPE Type;
  union {
    LOG_ENTRY_PROTOCOL_INSTALLED          ProtocolInstalled;
    LOG_ENTRY_PROTOCOL_REINSTALLED        ProtocolReinstalled;
    LOG_ENTRY_PROTOCOL_REMOVED            ProtocolRemoved;
    LOG_ENTRY_PROTOCOL_EXISTS_ON_STARTUP  ProtocolExistsOnStartup;
    LOG_ENTRY_IMAGE_LOADED                ImageLoaded;
    LOG_ENTRY_IMAGE_EXISTS_ON_STARTUP     ImageExistsOnStartup;
    LOG_ENTRY_BDS_STAGE_ENTERED           BdsStageEntered;
    LOG_ENTRY_ERROR                       Error;
  };
} LOADING_EVENT;

// -----------------------------------------------------------------------------
/**
 * Корректно освобождает память из-под всех указателей в Event, не равных NULL.
*/
VOID
LoadingEvent_Destruct (
  LOADING_EVENT *Event
  );

// -----------------------------------------------------------------------------

#endif // LOG_EVENT_LIB_H_