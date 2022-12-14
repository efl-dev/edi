#ifndef __EDI_PROCESS_H__
#define __EDI_PROCESS_H__

/**
 * @file
 * @brief Routines for querying processes.
 */

/**
 * @brief Querying Processes
 * @defgroup Proc
 *
 * @{
 *
 * Query processes.
 *
 */

#include <Eina.h>

#ifdef EAPI
# undef EAPI
#endif

#ifdef _WIN32
# ifdef EFL_EDI_BUILD
#  ifdef DLL_EXPORT
#   define EAPI __declspec(dllexport)
#  else
#   define EAPI
#  endif /* ! DLL_EXPORT */
# else
#  define EAPI __declspec(dllimport)
# endif /* ! EFL_EDI_BUILD */
#else
# ifdef __GNUC__
#  if __GNUC__ >= 4
#   define EAPI __attribute__ ((visibility("default")))
#  else
#   define EAPI
#  endif
# else
#  define EAPI
# endif
#endif /* ! _WIN32 */

#include <stdint.h>
#include <unistd.h>

#if !defined(PID_MAX)
# define PID_MAX     99999
#endif

#define CMD_NAME_MAX 8192

typedef struct _Edi_Proc_Stats
{
   pid_t       pid;
   pid_t       ppid;
   uid_t       uid;
   int8_t      nice;
   int8_t      priority;
   int         cpu_id;
   int32_t     numthreads;
   int64_t     mem_size;
   int64_t     mem_rss;
   double      cpu_usage;
   char        command[CMD_NAME_MAX];
   const char *state;

   long        cpu_time;
} Edi_Proc_Stats;

/**
 * Query a process for its current state.
 *
 * @param pid The process ID to query.
 *
 * @return Pointer to object containing the process information or NULL if non-existent.
 */
EAPI Edi_Proc_Stats *edi_process_stats_by_pid(int pid);


/**
 * @}
 */

#endif
