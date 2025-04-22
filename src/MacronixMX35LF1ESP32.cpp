#include <stdio.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "MacronixMX35LF1.h"  

#define PIN_NUM_MISO 19
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK  18
#define PIN_NUM_CS   5

static const char *TAG = "MX35LF1";
static spi_device_handle_t spi;

void mx35lf1_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096
    };

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 20 * 1000 * 1000,  // 20 MHz
        .mode = 0,
        .spics_io_num = PIN_NUM_CS,
        .queue_size = 1,
    };

    esp_err_t ret;
    ret = spi_bus_initialize(HSPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);
    ret = spi_bus_add_device(HSPI_HOST, &devcfg, &spi);
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "MX35LF1 SPI flash initialized");
}

esp_err_t mx35lf1_send_command(const uint8_t *cmd, size_t cmd_len, uint8_t *rx_data, size_t rx_len)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = cmd_len * 8;
    t.tx_buffer = cmd;

    if (rx_data && rx_len > 0) {
        t.rxlength = rx_len * 8;
        t.rx_buffer = rx_data;
    }

    return spi_device_transmit(spi, &t);
}

void mx35lf1_read_jedec_id(uint8_t *manufacturer_id, uint8_t *memory_type, uint8_t *capacity)
{
    uint8_t cmd = 0x9F;
    uint8_t id[3] = {0};

    esp_err_t ret = mx35lf1_send_command(&cmd, 1, id, 3);
    if (ret == ESP_OK) {
        *manufacturer_id = id[0];
        *memory_type = id[1];
        *capacity = id[2];
        ESP_LOGI(TAG, "JEDEC ID read: %02X %02X %02X", id[0], id[1], id[2]);
    } else {
        ESP_LOGE(TAG, "Failed to read JEDEC ID");
    }
}

void mx35lf1_read_status_register(uint8_t *status)
{
    uint8_t cmd = 0x05;
    mx35lf1_send_command(&cmd, 1, status, 1);
}

void mx35lf1_write_enable()
{
    uint8_t cmd = 0x06;
    mx35lf1_send_command(&cmd, 1, NULL, 0);
}

void mx35lf1_sector_erase(uint32_t address)
{
    uint8_t cmd[4];
    cmd[0] = 0xD8; // sector erase
    cmd[1] = (address >> 16) & 0xFF;
    cmd[2] = (address >> 8) & 0xFF;
    cmd[3] = address & 0xFF;

    mx35lf1_write_enable();
    mx35lf1_send_command(cmd, 4, NULL, 0);
}

void mx35lf1_page_program(uint32_t address, const uint8_t *data, size_t length)
{
    if (length > 256) length = 256;  // max page size

    uint8_t cmd[4];
    cmd[0] = 0x02; // page program
    cmd[1] = (address >> 16) & 0xFF;
    cmd[2] = (address >> 8) & 0xFF;
    cmd[3] = address & 0xFF;

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = (4 + length) * 8;
    t.tx_buffer = malloc(4 + length);
    memcpy((uint8_t *)t.tx_buffer, cmd, 4);
    memcpy((uint8_t *)t.tx_buffer + 4, data, length);

    mx35lf1_write_enable();
    spi_device_transmit(spi, &t);
    free((void *)t.tx_buffer);
}

void mx35lf1_read_data(uint32_t address, uint8_t *buffer, size_t length)
{
    uint8_t cmd[4];
    cmd[0] = 0x03; // read command
    cmd[1] = (address >> 16) & 0xFF;
    cmd[2] = (address >> 8) & 0xFF;
    cmd[3] = address & 0xFF;

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 4 * 8;
    t.rxlength = length * 8;
    t.tx_buffer = cmd;
    t.rx_buffer = buffer;

    spi_device_transmit(spi, &t);
}
