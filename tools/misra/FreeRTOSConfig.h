/*******************************************************************************
 * FreeRTOSConfig.h — MISRA-C analysis override
 *
 * cppcheck finds this file before test/posix_config/FreeRTOSConfig.h because
 * tools/misra/ is listed first in the include path.
 *
 * Pulls in the real test configuration then replaces configASSERT with a
 * MISRA-C 2012 compliant definition.  The standard test version expands to:
 *
 *   if ( ( x ) == 0 )  ...
 *
 * which violates Rule 10.4 when x is an essentially Boolean expression
 * (e.g. a comparison), because 0 is essentially signed.
 ******************************************************************************/

#include "../../test/posix_config/FreeRTOSConfig.h"

/* Replace with a MISRA-C clean expansion for static analysis only. */
#undef  configASSERT
#define configASSERT(x) (void)(x)