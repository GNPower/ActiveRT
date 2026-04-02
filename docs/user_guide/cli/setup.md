# Setting Up the ActiveRT CLI

ActiveRT's CLI layer exposes all statistics and health checks as text
commands over any embedded serial interface. This page shows how to wire
it up using [embedded-cli](https://github.com/funbiscuit/embedded-cli) as
the example CLI library.

---

## Step 1: Enable the CLI in your config

```c
/* Before including any ActiveRT header */
#define ACTIVERT_ENABLE_CLI 1
```

---

## Step 2: Define `ACTIVERT_CLI_PRINTF`

The CLI output macro must be mapped to your platform's character-output
function. For a UART-backed CLI, this is typically the CLI library's own
print function:

```c
#define ACTIVERT_CLI_PRINTF  embeddedCliPrintf
```

Or redirect to a UART write function:

```c
static void uart_cli_write(const char *fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    UART_Write(buf, strlen(buf));
}
#define ACTIVERT_CLI_PRINTF  uart_cli_write
```

---

## Step 3: Define `ACTIVERT_CLI_GET_TOKEN`

The token-extraction macro wraps your CLI library's argument parser.
For embedded-cli:

```c
#define ACTIVERT_CLI_GET_TOKEN(args, n)  embeddedCliGetToken(args, (n) + 1)
```

`embeddedCliGetToken(args, n)` returns the *n*-th token (1-indexed),
so adding 1 converts from ActiveRT's 0-indexed convention.

---

## Step 4: Register the commands

After initialising your CLI instance, complete any additional setup
required by your CLI library. In this example, we will assume the
setup is completed with a call to register all ActiveRT commands.
This step is specific to the CLI library:

```c
#include "activert_cli.h"

void app_init(void)
{
    /* ... init pools and AOs ... */

    /* Register "ActiveRT" command group */
    activert_cli_register_commands(cli);
}
```

This registers one top-level command (`activert`) with subcommands dispatched
internally by the ActiveRT CLI layer.

---

## Complete example

```c
/* activert_cli_config.h included before activert.h */
#ifndef ACTIVERT_CLI_CONFIG_H
#define ACTIVERT_CLI_CONFIG_H

#include "embedded_cli.h"

#define ACTIVERT_ENABLE_CLI       1
#define ACTIVERT_ENABLE_STATS     1

#define ACTIVERT_CLI_PRINTF       embeddedCliPrintf
#define ACTIVERT_CLI_GET_TOKEN(args, n)  embeddedCliGetToken(args, (n) + 1)

#endif

/* main.c */
#include "activert_cli_config.h"
#include "activert.h"
#include "activert_cli.h"
#include "embedded_cli.h"

static EmbeddedCli *s_cli;

void app_init(void)
{
    EmbeddedCliConfig cfg = *embeddedCliDefaultConfig();
    s_cli = embeddedCliNew(&cfg);
    s_cli->writeChar = uart_write_char;   /* your UART write */

    /* Register ActiveRT commands */
    activert_cli_register_commands(s_cli);

    /* Start scheduler */
    vTaskStartScheduler();
}

/* Called from UART RX interrupt or a dedicated UART task */
void process_uart_byte(char c)
{
    embeddedCliReceiveChar(s_cli, c);
    embeddedCliProcess(s_cli);
}
```

---

## Verifying the Setup

Connect a terminal to your UART and type:

```text
activert help
```

Expected output:

```text
ActiveRT CLI commands:
  activert summary          — one-line status for all AOs and pools
  activert list             — list all registered Active Objects
  activert show <name>      — detailed stats for one AO
  activert pool <name>      — detailed stats for one event pool
  activert health           — health check for all components
  activert reset [name]     — reset stats (all or named component)
  activert perf             — performance summary
  activert report           — full diagnostic report
```
