#include "hexdump.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

void hexdump(void* mem, uint32_t len, uint64_t base)
{
    unsigned int i, j;
    char buffer[256] = { 0 };
    char str[80] = { 0 };

    printf("[*] Dumping %08x bytes at address %llx:\n", len, (unsigned long long)mem);

    for (i = 0; i < len + ((len % HEXDUMP_COLS) ? (HEXDUMP_COLS - len % HEXDUMP_COLS) : 0); i++) {
        if (i % HEXDUMP_COLS == 0) {
            sprintf(str, "[*] 0x%08x (+0x%04x): ", (unsigned int)(i + base), i);
            strcat(buffer, str);
        }

        if (i < len) {
            sprintf(str, "%02x ", 0xFF & ((char*)mem)[i]);
            strcat(buffer, str);
        } else {
            sprintf(str, "   ");
            strcat(buffer, str);
        }

        if (i % HEXDUMP_COLS == (HEXDUMP_COLS - 1)) {
            for (j = i - (HEXDUMP_COLS - 1); j <= i; j++) {
                if (j >= len) {
                    sprintf(str, " ");
                    strcat(buffer, str);

                } else if (isprint(((char*)mem)[j])) {
                    sprintf(str, "%c", 0xFF & ((char*)mem)[j]);
                    strcat(buffer, str);
                } else {
                    sprintf(str, ".");
                    strcat(buffer, str);
                }
            }

            printf("%s\n", buffer);
            memset(&buffer[0], 0, sizeof(buffer));
        }
    }
}
