[Defines]
  PLATFORM_NAME                  = DxeLoadingLogger
  PLATFORM_GUID                  = 9FE12CD6-D8B8-424D-B703-ED19D908C122
  PLATFORM_VERSION               = 1.0
  DSC_SPECIFICATION              = 0x0001001B
  SUPPORTED_ARCHITECTURES        = X64
  BUILD_TARGETS                  = DEBUG | RELEASE
  FLASH_DEFINITION               = DxeLoadingLoggerPkg/DxeLoadingLoggerPkg.fdf

  DEFINE DEBUG_PRINT_ERROR_LEVEL    = 0x80000040
  DEFINE DEBUG_PROPERTY_MASK        = 0x0f


[PcdsFixedAtBuild]
  # DebugLib
  gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask    | $(DEBUG_PROPERTY_MASK)
  gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel | $(DEBUG_PRINT_ERROR_LEVEL)


[BuildOptions]
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
  DebugLib                    | MdePkg/Library/UefiDebugLibConOut/UefiDebugLibConOut.inf
  DebugPrintErrorLevelLib     | MdePkg/Library/BaseDebugPrintErrorLevelLib/BaseDebugPrintErrorLevelLib.inf

  #
  # Библиотеки, входящие в состав данного пакета.
  #
  VectorLib                   | DxeLoadingLoggerPkg/Library/VectorLib/VectorLib.inf

[Components]
  DxeLoadingLoggerPkg/Source/DxeLoadingLogger.inf
