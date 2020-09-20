#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/TextAnimationLib.h>
#include <Library/CommonMacrosLib.h>

//------------------------------------------------------------------------------
STATIC UINTN   gAnimationIndex;
STATIC BOOLEAN gSavedEnableCursor;
STATIC CHAR16  OutStr[] = L" Log writing: * ";
STATIC CHAR16  ClrStr[] = L"                ";
STATIC UINTN   UpdateCounter;

#define STR_SIZE(Str) ((sizeof(Str) / sizeof(Str[0]) - 1))

//------------------------------------------------------------------------------
/**
 * Начинает проигрывать анимацию.
*/
VOID
StartPlayingAnimation ()
{
  if (gST->ConOut == NULL) {
    return;
  }

  gSavedEnableCursor = gST->ConOut->Mode->CursorVisible;
  gST->ConOut->EnableCursor (gST->ConOut, FALSE);

  UpdateCounter = 0;
  UpdatePlayingAnimation ();
}

//------------------------------------------------------------------------------
/**
 * Прекращает проигрывать анимацию, если она воспроизводится в данный момент.
*/
VOID
StopPlayingAnimation ()
{
  if (gST->ConOut == NULL) {
    return;
  }

  INT32 CursorColumn = gST->ConOut->Mode->CursorColumn;
  INT32 CursorRow    = gST->ConOut->Mode->CursorRow;

  gST->ConOut->SetCursorPosition (gST->ConOut, 40, CursorRow);
  gST->ConOut->OutputString (gST->ConOut, ClrStr);
  gST->ConOut->SetCursorPosition (gST->ConOut, CursorColumn, CursorRow);

  gST->ConOut->EnableCursor (gST->ConOut, gSavedEnableCursor);
}

//------------------------------------------------------------------------------
/**
 * Перерисовывает анимацию.
*/
VOID
UpdatePlayingAnimation ()
{
  if (gST->ConOut == NULL) {
    return;
  }

  if (UpdateCounter++ % 10 != 0) {
    return;
  }

  STATIC CHAR16 AnimationChars[] = L"|/-\\|/-\\";

  OutStr[STR_SIZE (OutStr) - 2] = AnimationChars[gAnimationIndex++];
  if (gAnimationIndex >= STR_SIZE (AnimationChars)) {
    gAnimationIndex = 0;
  }

  INT32 CursorColumn = gST->ConOut->Mode->CursorColumn;
  INT32 CursorRow    = gST->ConOut->Mode->CursorRow;

  gST->ConOut->SetCursorPosition (gST->ConOut, 40, CursorRow);
  gST->ConOut->OutputString (gST->ConOut, OutStr);
  gST->ConOut->SetCursorPosition (gST->ConOut, CursorColumn, CursorRow);
}

//------------------------------------------------------------------------------
