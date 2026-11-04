#ifndef MCP3008_H
#define MCP3008_H
#include <stdint.h>

typedef struct { int fd; } mcp3008_t;

int  mcp3008_open(mcp3008_t *m, const char *path);
int  mcp3008_read(mcp3008_t *m, unsigned channel, uint16_t *value); /* 0 ok, -1 error */
void mcp3008_close(mcp3008_t *m);

/* Funciones puras (probadas en PC sin hardware) */
void     mcp3008_build_tx(unsigned channel, uint8_t tx[3]);
uint16_t mcp3008_decode(const uint8_t rx[3]);
#endif
