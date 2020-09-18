/** @file
 * Содержит описание типов событий, учёт которых мы ведём.
 */
#include <Uefi.h>

#ifndef LOG_EVENTS_H_
#define LOG_EVENTS_H_

// -----------------------------------------------------------------------------
typedef enum {
  LOG_ENTRY_TYPE_PROTOCOL_INSTALLED            = 0,
  LOG_ENTRY_TYPE_PROTOCOL_EXISTS_ON_STARTUP    = 1,
  LOG_ENTRY_TYPE_PROTOCOL_REMOVED              = 2,
  LOG_ENTRY_TYPE_IMAGE_LOADED                  = 3,
  LOG_ENTRY_TYPE_IMAGE_EXISTS_ON_STARTUP       = 4,
  LOG_ENTRY_TYPE_BDS_STAGE_ENTERED             = 5
} LOG_ENTRY_TYPE;

// -----------------------------------------------------------------------------
typedef PACKED struct {
  GUID    Guid;
  BOOLEAN Successful;
  CHAR16  *ImageNameWhoInstalled;
  CHAR16  *ImageNameWhereInstalled;
} LOG_ENTRY_PROTOCOL_INSTALLED, LOG_ENTRY_PROTOCOL_REMOVED;

// -----------------------------------------------------------------------------
typedef PACKED struct {
  GUID    Guid;
  CHAR16  *ImageNames;
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
  LOG_ENTRY_TYPE Type;
  union {
    LOG_ENTRY_PROTOCOL_INSTALLED          ProtocolInstalled;
    LOG_ENTRY_PROTOCOL_EXISTS_ON_STARTUP  ProtocolExistsOnStartup;
    LOG_ENTRY_PROTOCOL_REMOVED            ProtocolRemoved;
    LOG_ENTRY_IMAGE_LOADED                ImageLoaded;
    LOG_ENTRY_IMAGE_EXISTS_ON_STARTUP     ImageExistsOnStartup;
    LOG_ENTRY_BDS_STAGE_ENTERED           BdsStageEntered;
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

#endif // LOG_EVENTS_H_