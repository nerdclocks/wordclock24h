#include <stdarg.h>

#undef UART_PREFIX
#define UART_PREFIX     log
#include "uart.h"

#define log_char(c)     do { log_uart_putc (c); } while (0)
#define log_str(s)      do { log_uart_puts (s); } while (0)
#define log_msg(s)      do { log_uart_puts (s); log_uart_puts ("\r\n"); } while (0)

extern void             log_vprintf (const char *, va_list);
extern void             log_printf (const char *, ...);
extern void             log_init (void);
