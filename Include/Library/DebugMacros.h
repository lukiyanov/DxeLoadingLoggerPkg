#ifndef DEBUG_MACROS_H__
#define DEBUG_MACROS_H__

#include <Library/DebugLib.h>

#if DEBUG_MACROS_ENABLE_FULL_PRINT
#define WHERESTR                "%a:%a:%d "
#define ENTERSTR                "=> %a:%a:%d "
#define EXITSTR                 "<= %a:%a:%d "
#define WHEREARG                __FUNCTION__, __FILE__, __LINE__
#else
#define WHERESTR                "%a:%d: "
#define ENTERSTR                "=> %a:%d: "
#define EXITSTR                 "<= %a:%d: "
#define WHEREARG                __FUNCTION__, __LINE__
#endif

#define DBG_PRINT_LEVEL         EFI_D_INFO

#define DBG_PRINT(...)          DEBUG((DBG_PRINT_LEVEL, ## __VA_ARGS__))

#define DBG_ERROR(_format_,...)     DEBUG((EFI_D_ERROR, WHERESTR _format_, WHEREARG, __VA_ARGS__))
#define DBG_ERROR1(_format_)        DEBUG((EFI_D_ERROR, WHERESTR _format_, WHEREARG))
#define DBG_INFO(_format_,...)      DEBUG((EFI_D_INFO, WHERESTR _format_, WHEREARG, __VA_ARGS__))
#define DBG_INFO1(_format_)         DEBUG((EFI_D_INFO, WHERESTR _format_, WHEREARG))

#define DBG_ENTER()             DBG_PRINT(ENTERSTR "\n", WHEREARG)
#define DBG_EXIT_STATUS(Status) DBG_PRINT(EXITSTR "Status: %r\n", WHEREARG, Status)
#define DBG_EXIT()              DBG_PRINT(EXITSTR "\n", WHEREARG)

#endif // DEBUG_MACROS_H__
