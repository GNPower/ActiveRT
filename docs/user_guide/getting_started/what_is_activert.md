# What Is ActiveRT?

## The Problem: Shared-State Concurrency

In a typical RTOS application, multiple tasks access shared hardware
peripherals, protocol state machines, and application data. Protecting
that shared state requires [mutexes](https://freertos.org/Documentation/02-Kernel/02-Kernel-features/02-Queues-mutexes-and-semaphores/04-Mutexes), and mutexes introduce deadlock risk,
priority inversion, and code that is difficult to reason about especially when interrupts are involved.

A common issue: a bug that only appears when two tasks happen to be
scheduled within a narrow timing window. These bugs are not reproducible
under a debugger.

## The Solution: Active Objects

An *Active Object* (AO) is a design pattern that eliminates shared-state
concurrency by enforcing one simple rule:

> **An object's internal state is only ever accessed by its own task.**

All communication between Active Objects happens by passing events through
queues. There are no shared variables to protect; no mutexes inside an
Active Object's state machine.

The pattern was formalised by Miro Samek in [*Practical UML Statecharts in
C/C++*](https://www.state-machine.com/doc/PSiCC2.pdf) and is widely used in safety-critical embedded software.

## How ActiveRT Implements It

ActiveRT maps each Active Object onto a FreeRTOS task. The task sits in a
loop, pulling events from one or more queues and dispatching them to a
user-supplied handler function:

```{figure} ../../img/event_life.png
:alt: Event lifecycle - allocate from pool, post to queue, dispatch by AO task, return to pool automatically
:align: center
:figclass: diagram-invertible
```

Key properties:

- **Zero heap at runtime.** All memory task (stack, queues, event pools)
  is declared statically by the caller. `pvPortMalloc` is never called after
  the scheduler start (unless you opt into `ACTIVERT_POOL_OVERFLOW_DYNAMIC`).
- **Deterministic latency.** Event allocation is an O(1) bitmap scan.
  Queue operations are FreeRTOS primitives with bounded execution time.
- **ISR-safe.** Every post, alloc, and notify operation has a
  `_from_isr` variant.
- **Observable.** Per-component statistics, health checks, and a CLI
  layer are built in (can opted out of with `ACTIVERT_ENABLE_STATS`to
  save memory and executable size).

## What ActiveRT Is Not

ActiveRT is not a full hierarchical state machine (HSM) framework. It
provides the *infrastructure* (tasks, queues, event pools, statistics)
that you plug a state machine into. The dispatch handler can be a flat
`switch` statement, a table-driven FSM, or a full HSM; ActiveRT does not care.

ActiveRT does not bundle FreeRTOS. Your BSP or toolchain supplies the
FreeRTOS kernel and ActiveRT links against it.
