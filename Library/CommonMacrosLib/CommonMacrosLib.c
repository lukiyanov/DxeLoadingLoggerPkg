#include <Uefi.h>
#include <Library/CommonMacrosLib.h>
#include <Library/PcdLib.h>

BOOLEAN
CommonMacrosEnabled()
{
  return FeaturePcdGet (PcdDebugMacrosOutputEnabled);
}