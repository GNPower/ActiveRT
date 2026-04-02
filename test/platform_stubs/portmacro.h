/*******************************************************************************
 * portmacro.h — Minimal port type definitions for MISRA-C static analysis
 *
 * Satisfies the FreeRTOS.h → portable.h → portmacro.h include chain so that
 * cppcheck can resolve FreeRTOS types and function declarations when running
 * the MISRA-C check without a real compiled port.
 *
 * Used only for static analysis. The host POSIX test build and the embedded
 * target each supply their own portmacro.h via the FreeRTOS portable/ tree.
 *
 * FreeRTOS compatibility: V11.x
 ******************************************************************************/

#ifndef PORTMACRO_H
#define PORTMACRO_H

#include <stdint.h>

/*----------------------------------------------------------------------------
 * Basic type mappings
 *--------------------------------------------------------------------------*/
#define portCHAR             char
#define portFLOAT            float
#define portDOUBLE           double
#define portLONG             long
#define portSHORT            short
#define portSTACK_TYPE       uint8_t
#define portBASE_TYPE        long
#define portPOINTER_SIZE_TYPE uint32_t

typedef uint8_t       StackType_t;
typedef long          BaseType_t;
typedef unsigned long UBaseType_t;
typedef uint32_t      TickType_t;

#define portMAX_DELAY        ((TickType_t) 0xFFFFFFFFUL)
#define portTICK_PERIOD_MS   ((TickType_t) 1000U / configTICK_RATE_HZ)

/*----------------------------------------------------------------------------
 * Byte alignment
 *--------------------------------------------------------------------------*/
#define portBYTE_ALIGNMENT      8
#define portBYTE_ALIGNMENT_MASK (0x0007U)

/*----------------------------------------------------------------------------
 * Inline / compiler attributes (simplified for static analysis)
 *--------------------------------------------------------------------------*/
#define portINLINE       inline
#define portFORCE_INLINE inline
#define portNOP()        do {} while (0)

/*----------------------------------------------------------------------------
 * Critical sections — no-ops for static analysis
 *--------------------------------------------------------------------------*/
#define portENTER_CRITICAL()
#define portEXIT_CRITICAL()
#define portDISABLE_INTERRUPTS()
#define portENABLE_INTERRUPTS()
#define portSET_INTERRUPT_MASK_FROM_ISR()       0
#define portCLEAR_INTERRUPT_MASK_FROM_ISR(x)    (void)(x)

/*----------------------------------------------------------------------------
 * Yield macros — no-ops for static analysis
 *--------------------------------------------------------------------------*/
#define portYIELD()
#define portYIELD_FROM_ISR(x)    (void)(x)
#define portEND_SWITCHING_ISR(x) (void)(x)

/*----------------------------------------------------------------------------
 * Task function macros
 *--------------------------------------------------------------------------*/
#define portTASK_FUNCTION_PROTO(vFunction, pvParameters) \
    void vFunction(void *pvParameters)
#define portTASK_FUNCTION(vFunction, pvParameters) \
    void vFunction(void *pvParameters)

/*----------------------------------------------------------------------------
 * Miscellaneous
 *--------------------------------------------------------------------------*/
#define portMEMORY_BARRIER()
#define portCRITICAL_NESTING_IN_TCB 0

#endif /* PORTMACRO_H */