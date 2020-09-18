[Defines]
  PLATFORM_NAME                  = DxeLoadingLogger
  PLATFORM_GUID                  = 9FE12CD6-D8B8-424D-B703-ED19D908C122
  PLATFORM_VERSION               = 1.0
  DSC_SPECIFICATION              = 0x0001001B
  SUPPORTED_ARCHITECTURES        = X64
  BUILD_TARGETS                  = DEBUG | RELEASE
  FLASH_DEFINITION               = DxeLoadingLoggerPkg/DxeLoadingLoggerPkg.fdf


  #### BEHAVIOR ################################################################

  #
  # Определяет, каким способом происходит сбор событий.
  #
  # TRUE:
  #        События собираются путём модификации gST.
  #        Этот подход даёт больше информации, но системная прошивка может сопротивляться данному методу.
  #
  # FALSE:
  #        События собираются посредством вызова RegisterProtocolNotify() для известных протоколов.
  #        Гарантированно совместим со всеми прошивками, но даёт гораздо меньше информации.
  #
  DEFINE EVENT_PROVIDER_GST_HOOK = FALSE  # TODO: в RELEASE-версии установить в TRUE

  #
  # Выводить номера событий в консоль.
  # Полезно для сопоставления событий с тем, что выводит системная прошивка.
  #
  DEFINE PRINT_EVENT_NUMBERS_TO_CONSOLE = TRUE


  #### DEBUG ###################################################################

  #
  # Генерит подробный и длинный лог свой работы.
  # Только для отладки.
  #
  DEFINE DEBUG_MACROS_OUTPUT_ON = FALSE

  #
  # TRUE:
  #        Отладочные события выводятся в COM-порт.
  #
  # FALSE:
  #        Отладочные события выводятся на экран.
  #
  DEFINE DEBUG_OUTPUT_TO_SERIAL  = FALSE

  DEFINE DEBUG_PRINT_ERROR_LEVEL = 0x80000040
  DEFINE DEBUG_PROPERTY_MASK     = 0x0f


[BuildOptions]
  GCC:*_*_*_CC_FLAGS                   = -std=c11
  GCC:RELEASE_*_*_CC_FLAGS             = -DMDEPKG_NDEBUG
  MSFT:RELEASE_*_*_CC_FLAGS            = /D MDEPKG_NDEBUG


[LibraryClasses]
  #
  # Явно используются в inf файлах.
  #
  UefiDriverEntryPoint        | MdePkg/Library/UefiDriverEntryPoint/UefiDriverEntryPoint.inf
  UefiBootServicesTableLib    | MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  BaseLib                     | MdePkg/Library/BaseLib/BaseLib.inf
  BaseMemoryLib               | MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  UefiLib                     | MdePkg/Library/UefiLib/UefiLib.inf
  DevicePathLib               | MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf

  #
  # Не используются явно, но требуются как зависимости для других экземпляров библиотек.
  #
  MemoryAllocationLib         | MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  PcdLib                      | MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  PrintLib                    | MdePkg/Library/BasePrintLib/BasePrintLib.inf
  UefiRuntimeServicesTableLib | MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  DebugPrintErrorLevelLib     | MdePkg/Library/BaseDebugPrintErrorLevelLib/BaseDebugPrintErrorLevelLib.inf

!if $(DEBUG_OUTPUT_TO_SERIAL)
  DebugLib                    | MdePkg/Library/BaseDebugLibSerialPort/BaseDebugLibSerialPort.inf
  SerialPortLib               | PcAtChipsetPkg/Library/SerialIoLib/SerialIoLib.inf
!else
  DebugLib                    | MdePkg/Library/UefiDebugLibConOut/UefiDebugLibConOut.inf
!endif

  #
  # Библиотеки, входящие в состав данного пакета.
  #
  VectorLib                   | DxeLoadingLoggerPkg/Library/VectorLib/VectorLib.inf
  ProtocolGuidDatabaseLib     | DxeLoadingLoggerPkg/Library/ProtocolGuidDatabaseLib/ProtocolGuidDatabaseLib.inf
  EventLoggerLib              | DxeLoadingLoggerPkg/Library/EventLoggerLib/EventLoggerLib.inf
  CommonMacrosLib             | DxeLoadingLoggerPkg/Library/CommonMacrosLib/CommonMacrosLib.inf
  LoadingEventLib             | DxeLoadingLoggerPkg/Library/LoadingEventLib/LoadingEventLib.inf

!if $(EVENT_PROVIDER_GST_HOOK)
  EventProviderLib            | DxeLoadingLoggerPkg/Library/EventProviderLib/EventProviderSystemTableHookLib/EventProviderSystemTableHookLib.inf
!else
  EventProviderLib            | DxeLoadingLoggerPkg/Library/EventProviderLib/EventProviderProtocolNotifyLib/EventProviderProtocolNotifyLib.inf
!endif


[Components]
  DxeLoadingLoggerPkg/Source/DxeLoadingLogger.inf

[PcdsFixedAtBuild]
  # DebugLib
  gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask            | $(DEBUG_PROPERTY_MASK)
  gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel         | $(DEBUG_PRINT_ERROR_LEVEL)

[PcdsFeatureFlag]
  gDxeLoadingLoggerSpaceGuid.PcdPrintEventNumbersToConsole | $(PRINT_EVENT_NUMBERS_TO_CONSOLE)
  gDxeLoadingLoggerSpaceGuid.PcdDebugMacrosOutputEnabled   | $(DEBUG_MACROS_OUTPUT_ON)
