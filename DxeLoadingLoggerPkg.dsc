[Defines]
  PLATFORM_NAME                  = DxeLoadingLogger
  PLATFORM_GUID                  = a8b753ed-a2b9-4d3e-b73b-c0a5261dba07
  PLATFORM_VERSION               = 1.0
  DSC_SPECIFICATION              = 0x0001001B
  SUPPORTED_ARCHITECTURES        = X64
  BUILD_TARGETS                  = DEBUG|RELEASE
  FLASH_DEFINITION               = DxeLoadingLoggerPkg/DxeLoadingLoggerPkg.fdf

  DEFINE DEBUG_PRINT_ERROR_LEVEL    = 0x80000040
  DEFINE DEBUG_PROPERTY_MASK        = 0x0f


[PcdsFixedAtBuild]
  # DebugLib
  gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask    | $(DEBUG_PROPERTY_MASK)
  gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel | $(DEBUG_PRINT_ERROR_LEVEL)


[BuildOptions]
  GCC:RELEASE_*_*_CC_FLAGS             = -DMDEPKG_NDEBUG
  GCC:DEBUG_*_*_CC_FLAGS               = -g
  MSFT:RELEASE_*_*_CC_FLAGS            = /D MDEPKG_NDEBUG /WX /O1 /wd"4201" /wd"4117" /D DEBUG_MACROS_ENABLE_FULL_PRINT
  MSFT:DEBUG_*_*_CC_FLAGS              = /X /Zc:wchar_t /Oi- /GL- /WX /Od /wd"4201" /wd"4117"


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


[Components]
  DxeLoadingLoggerPkg/Source/DxeLoadingLogger.inf
