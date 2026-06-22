#include "console_helper.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void strip_newline(char *text)
{
    size_t len;

    if (text == NULL) {
        return;
    }

    len = strlen(text);
    while (len > 0u && (text[len - 1u] == '\n' || text[len - 1u] == '\r')) {
        text[len - 1u] = '\0';
        len--;
    }
}

void console_pause(const char *message)
{
    char line[8];

    if (message != NULL) {
        printf("%s", message);
    } else {
        printf("Press ENTER to continue...");
    }
    fflush(stdout);
    (void)fgets(line, sizeof(line), stdin);
}

void console_read_line_default(const char *prompt,
                               const char *default_value,
                               char *out,
                               size_t out_size)
{
    char line[256];

    if (out == NULL || out_size == 0u) {
        return;
    }

    printf("%s [%s]: ", prompt, default_value == NULL ? "" : default_value);
    fflush(stdout);

    if (fgets(line, sizeof(line), stdin) == NULL) {
        line[0] = '\0';
    }
    strip_newline(line);

    if (line[0] == '\0' && default_value != NULL) {
        strncpy(out, default_value, out_size - 1u);
    } else {
        strncpy(out, line, out_size - 1u);
    }
    out[out_size - 1u] = '\0';
}

uint32_t console_read_u32_default(const char *prompt, uint32_t default_value)
{
    char default_text[32];
    char line[64];
    char *end = NULL;
    unsigned long value;

    sprintf(default_text, "%u", (unsigned)default_value);
    console_read_line_default(prompt, default_text, line, sizeof(line));

    value = strtoul(line, &end, 10);
    if (end == line || *end != '\0') {
        printf("  input is not a number, use default %u\n", (unsigned)default_value);
        return default_value;
    }

    return (uint32_t)value;
}

void console_print_step(unsigned step, const char *title)
{
    printf("\n============================================================\n");
    printf("STEP %u: %s\n", step, title);
    printf("============================================================\n");
}
