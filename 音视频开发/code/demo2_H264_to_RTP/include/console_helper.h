#ifndef CONSOLE_HELPER_H
#define CONSOLE_HELPER_H

#include <stdint.h>
#include <stddef.h>

/* 暂停并等待用户按回车，便于交互式调试。 */
void console_pause(const char *message);

void console_read_line_default(const char *prompt,
                               const char *default_value,
                               char *out,
                               size_t out_size);

/* 读取一个无符号整数，空输入时使用默认值。 */
uint32_t console_read_u32_default(const char *prompt, uint32_t default_value);

void console_print_step(unsigned step, const char *title);

#endif
