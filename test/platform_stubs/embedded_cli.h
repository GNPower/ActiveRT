/*******************************************************************************
 * embedded_cli.h — Stub for MISRA-C static analysis
 *
 * Provides a minimal declaration of embeddedCliGetToken() so that
 * ACTIVERT_CLI_GET_TOKEN can be overridden to a real function call during
 * cppcheck analysis, preventing false MISRA-C 2.7 (unused parameter)
 * violations caused by the default NULL expansion in activert_config.h.
 *
 * The override is applied via -D on the cppcheck command line in
 * tools/misra/run_misra_check.py, which takes precedence over the
 * #ifndef guard in activert_config.h.
 *
 * Used only for static analysis — not compiled into any test or target build.
 ******************************************************************************/

#ifndef EMBEDDED_CLI_H
#define EMBEDDED_CLI_H

/**
 * @brief Get the Nth whitespace-delimited token from a CLI argument string.
 *
 * @param args  Raw argument string passed to the CLI command handler.
 * @param n     Token index (1-based).
 * @return      Pointer to start of the token, or NULL if not present.
 */
const char* embeddedCliGetToken(const char* args, unsigned int n);

#endif /* EMBEDDED_CLI_H */