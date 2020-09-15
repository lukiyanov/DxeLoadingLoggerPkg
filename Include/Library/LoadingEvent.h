/** @file
 * Содержит описание типов событий, учёт которых мы ведём.
 */
#include <Uefi.h>

#ifndef LOG_EVENTS_H_
#define LOG_EVENTS_H_

#define LOG_ENTRY_IMAGE_NAME_LENGTH 64

// ----------------------------------------------------------------------------
typedef enum {
  LOG_ENTRY_TYPE_PROTOCOL_INSTALLED            = 0,
  LOG_ENTRY_TYPE_PROTOCOL_REMOVED              = 1,
  LOG_ENTRY_TYPE_IMAGE_LOADED                  = 2,
  LOG_ENTRY_TYPE_BDS_STAGE_ENTERED             = 3
} LOG_ENTRY_TYPE;

// ----------------------------------------------------------------------------
typedef PACKED struct {
  GUID    Guid;
  BOOLEAN Successful;
  CHAR8   ImageName[LOG_ENTRY_IMAGE_NAME_LENGTH];
} LOG_ENTRY_PROTOCOL_INSTALLED, LOG_ENTRY_PROTOCOL_REMOVED;

// ----------------------------------------------------------------------------
typedef PACKED struct {
  CHAR8 ImageName       [LOG_ENTRY_IMAGE_NAME_LENGTH];
  CHAR8 ParentImageName [LOG_ENTRY_IMAGE_NAME_LENGTH];
} LOG_ENTRY_IMAGE_LOADED;

// ----------------------------------------------------------------------------
typedef enum {
  BDS_STAGE_EVENT_BEFORE_ENTRY_CALLING  = 0,
  BDS_STAGE_EVENT_AFTER_ENTRY_CALLING   = 1
} BDS_STAGE_SUB_EVENT_INFO;

// ----------------------------------------------------------------------------
typedef PACKED struct {
  BDS_STAGE_SUB_EVENT_INFO  Event;
} LOG_ENTRY_BDS_STAGE_ENTERED;

// ----------------------------------------------------------------------------
typedef PACKED struct {
  LOG_ENTRY_TYPE Type;
  union {
    LOG_ENTRY_PROTOCOL_INSTALLED  ProtocolInstalled;
    LOG_ENTRY_PROTOCOL_REMOVED    ProtocolRemove;
    LOG_ENTRY_IMAGE_LOADED        ImageLoaded;
    LOG_ENTRY_BDS_STAGE_ENTERED   BdsStage;
  };
} LOADING_EVENT;

// ----------------------------------------------------------------------------

#endif // LOG_EVENTS_H_