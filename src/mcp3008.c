#include "mcp3008.h"
#include "config.h"
#include "util.h"
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/spi/spidev.h>

#define MCP_START_BIT   0x01u
#define MCP_SINGLE_ENDED 0x08u   /* bit SGL/DIFF: lectura de un solo canal contra GND */
#define MCP_CHANNEL_SHIFT 4u
#define MCP_HIGH_MASK   0x03u
#define MCP_FRAME_LEN   3u

void mcp3008_build_tx(unsigned channel, uint8_t tx[3])
{
    tx[0] = MCP_START_BIT;
    tx[1] = (uint8_t)((MCP_SINGLE_ENDED + channel) << MCP_CHANNEL_SHIFT);
    tx[2] = 0x00;
}

uint16_t mcp3008_decode(const uint8_t rx[3])
{
    return (uint16_t)(((unsigned)(rx[1] & MCP_HIGH_MASK) << 8) | rx[2]);
}

int mcp3008_open(mcp3008_t *m, const char *path)
{
    uint8_t mode = SPI_MODE_0;
    uint8_t bits = (uint8_t)SPI_BITS_PER_WORD;
    uint32_t speed = SPI_SPEED_HZ;

    m->fd = open(path, O_RDWR);
    if (m->fd < 0) { LOG_ERRNO("open spidev"); return -1; }

    if (ioctl(m->fd, SPI_IOC_WR_MODE, &mode) < 0 ||
        ioctl(m->fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
        ioctl(m->fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        LOG_ERRNO("ioctl SPI config");
        close(m->fd);
        m->fd = -1;
        return -1;
    }
    return 0;
}

int mcp3008_read(mcp3008_t *m, unsigned channel, uint16_t *value)
{
    if (channel >= ADC_CHANNELS) { errno = EINVAL; return -1; }

    uint8_t tx[MCP_FRAME_LEN], rx[MCP_FRAME_LEN] = {0};
    mcp3008_build_tx(channel, tx);

    struct spi_ioc_transfer tr;
    memset(&tr, 0, sizeof(tr));
    tr.tx_buf = (unsigned long)tx;
    tr.rx_buf = (unsigned long)rx;
    tr.len = MCP_FRAME_LEN;
    tr.speed_hz = SPI_SPEED_HZ;
    tr.bits_per_word = (uint8_t)SPI_BITS_PER_WORD;

    if (ioctl(m->fd, SPI_IOC_MESSAGE(1), &tr) < 0)
        return -1;
    *value = mcp3008_decode(rx);
    return 0;
}

void mcp3008_close(mcp3008_t *m)
{
    if (m->fd >= 0) close(m->fd);
    m->fd = -1;
}
