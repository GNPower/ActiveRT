ActiveRT Documentation
======================

**ActiveRT** is an Active Object real-time framework for FreeRTOS-based
embedded systems. It implements the `Active Object design pattern
<https://en.wikipedia.org/wiki/Active_object>`_, providing event-driven
concurrency with deterministic behaviour, comprehensive statistics, and
zero-heap static allocation support.

.. toctree::
   :maxdepth: 2
   :caption: API Reference

   api/index

.. toctree::
   :maxdepth: 1
   :caption: About

   changelog

----

Quick Example
-------------

.. code-block:: c

   /* 1. Define event pool and Active Object (static — zero heap) */
   ACTIVERT_EVENT_POOL_DEFINE(cmd_pool, cmd_event_t, 8, ACTIVERT_POOL_OVERFLOW_DROP);
   ACTIVERT_ACTIVE_DEFINE_SIMPLE(cmd_task, cmd_dispatch, 5, 2048, 8);

   /* 2. Initialise in startup */
   void system_init(void) {
       ACTIVERT_EVENT_POOL_INIT(cmd_pool, cmd_event_t, 8, ACTIVERT_POOL_OVERFLOW_DROP);
       ACTIVERT_ACTIVE_INIT_SIMPLE(cmd_task, cmd_dispatch, 5);
   }

   /* 3. Dispatch handler */
   void cmd_dispatch(activert_active_t *me, const activert_event_t *e) {
       switch (e->sig) {
           case INIT_SIG:   /* startup */  break;
           case CMD_SIG_A:  handle_a();   break;
       }
   }

   /* 4. Post an event */
   cmd_event_t *evt = (cmd_event_t *)activert_event_pool_alloc(cmd_pool);
   evt->base.sig = CMD_SIG_A;
   activert_active_post(cmd_task, &evt->base);

Indices and tables
------------------

* :ref:`genindex`
* :ref:`search`
