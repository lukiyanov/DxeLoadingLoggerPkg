#pragma once

#include <Uefi.h>
#include "Vector.h"

// ----------------------------------------------------------------------------
typedef struct
{
  EFI_GUID *Guid;
  CHAR16   *Name;
  VOID     *Logger;
} ProtocolEntry;

// ----------------------------------------------------------------------------
typedef enum {
  LOG_ENTRY_TYPE_PROTOCOL_INSTALLED            = 0,
  LOG_ENTRY_TYPE_IMAGE_LOADED                  = 1,
  LOG_ENTRY_TYPE_PROTOCOL_EXISTANCE_ON_STARTUP = 2
} LOG_ENTRY_TYPE;

typedef struct {
  CHAR16* ProtocolName;
} LOG_ENTRY_PROTOCOL_INSTALLED;

#define LOG_ENTRY_IMAGE_NAME_LENGTH 32
typedef struct {
  CHAR8 ImageName       [LOG_ENTRY_IMAGE_NAME_LENGTH];
  CHAR8 ParentImageName [LOG_ENTRY_IMAGE_NAME_LENGTH];
} LOG_ENTRY_IMAGE_LOADED;

typedef struct {
  CHAR16* ProtocolName;
} __attribute__ ((packed)) LOG_ENTRY_PROTOCOL_EXISTENCE_ON_STARTUP;

typedef struct {
  UINT8 Type;
  union {
    LOG_ENTRY_PROTOCOL_INSTALLED            ProtocolInstalled;
    LOG_ENTRY_IMAGE_LOADED                  ImageLoaded;
    LOG_ENTRY_PROTOCOL_EXISTENCE_ON_STARTUP ProtocolExistence;
  };
} __attribute__ ((packed)) LOG_ENTRY;


// ----------------------------------------------------------------------------
typedef struct {
  struct Vector LogData;
  struct Vector EventHandles;
  struct Vector LoadedImageHandles;
} DxeLoadingLog;


// ----------------------------------------------------------------------------
VOID       DxeLoadingLog_Construct             (DxeLoadingLog *This);
EFI_STATUS DxeLoadingLog_SetObservingProtocols (DxeLoadingLog *This, ProtocolEntry *Protocols, UINTN ProtocolCount);
VOID       DxeLoadingLog_QuitObserving         (DxeLoadingLog *This);
VOID       DxeLoadingLog_Destruct              (DxeLoadingLog *This);

// ----------------------------------------------------------------------------
