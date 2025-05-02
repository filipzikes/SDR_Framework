#ifndef __AD9361_CONFIG__
#define __AD9361_CONFIG__

#include "AD9361_procedures.h"

typedef unsigned int range_t[2];

void disable_RF_units();
void print_config();

/********************
 * RF UNIT (AD9361)
********************/
struct iio_context *context    = NULL;
struct iio_device  *phy_device = NULL;

typedef struct {
    struct iio_device  *stream_device;
    struct iio_channel *lo_channel;
    struct iio_channel *rf_channel;
    struct iio_channel *channel_i;
    struct iio_channel *channel_q;
    struct iio_buffer  *buffer;
} RF_unit;

void context_init() {
    // Get: Local context, AD9361 PHY device
    ctx_local(&context);
    ctx_find_device(context, &phy_device, "ad9361-phy");
}
void TX_unit_init(RF_unit* unit) {
    // Get: AD9361 context, PHY device
    if (context == NULL || phy_device == NULL) context_init();

    // Get: AD9361 TX streaming device
    ctx_find_device(context, &(unit->stream_device), "cf-ad9361-dds-core-lpc");

    // Get: PHY TX channels
    dev_find_channel(phy_device, &(unit->rf_channel), "voltage0", OUTPUT);
    ch_attr_write_str(unit->rf_channel, "rf_port_select", "A");

    // Get: LO channel
    dev_find_channel(phy_device, &(unit->lo_channel), "altvoltage1", OUTPUT);

    // Get & enable: TX stream channels (I and Q)
    dev_find_channel(unit->stream_device, &(unit->channel_i), "voltage0", OUTPUT);
    dev_find_channel(unit->stream_device, &(unit->channel_q), "voltage1", OUTPUT);
    iio_channel_enable(unit->channel_i);
    iio_channel_enable(unit->channel_q);
}
void RX_unit_init(RF_unit* unit) {
    // Get: AD9361 context, PHY device
    if (context == NULL || phy_device == NULL) context_init();

    // Get: AD9361 RX streaming device
    ctx_find_device(context, &(unit->stream_device), "cf-ad9361-lpc");

    // Get: PHY RX channels
    dev_find_channel(phy_device, &(unit->rf_channel), "voltage0", INPUT);
    ch_attr_write_str(unit->rf_channel, "rf_port_select", "A_BALANCED");

    // Get: LO channel
    dev_find_channel(phy_device, &(unit->lo_channel), "altvoltage0", OUTPUT);

    // Get & enable: TX stream channels (I and Q)
    dev_find_channel(unit->stream_device, &(unit->channel_i), "voltage0", INPUT);
    dev_find_channel(unit->stream_device, &(unit->channel_q), "voltage1", INPUT);
    iio_channel_enable(unit->channel_i);
    iio_channel_enable(unit->channel_q);
}
void RF_unit_create_buffer(RF_unit* unit, long long samples, uint8_t cyclic) {
    dev_create_buffer(unit->stream_device, &(unit->buffer), samples, cyclic != 0);
}
void RF_unit_disable(RF_unit* unit) {
    printf(" (*) Destroying buffer\n");
    if (unit->buffer) iio_buffer_destroy(unit->buffer);
    printf(" (*) Disabling streaming channels\n");
    if (unit->channel_i) iio_channel_disable(unit->channel_i);
    if (unit->channel_q) iio_channel_disable(unit->channel_q);
    *unit = (RF_unit){0};
    printf(" (OK) RF unit disabled\n");
}
void shutdown(void) {
    printf("\n (/) SHUTDOWN\n");
    print_config();
    disable_RF_units();
	printf(" (*) Destroying context\n");
	if (context) iio_context_destroy(context);
	exit(exitCode);
}

/********************
 * Device: AD9361
 * Device specific attributes
********************/
void get_xo_corrextion_range(uint32_t *from, uint32_t *to) {
    char line[28];
    dev_attr_read_str(phy_device, "xo_correction_available", line, 28);
    sscanf(line, "[%d 1 %d]", from, to);
}
long long get_xo_corrextion() {
    long long val;
    dev_attr_read_ll(phy_device, "xo_correction", &val);
    return val;
}
void set_xo_correction(long long value) {
    dev_attr_write_ll(phy_device, "xo_correction", value);
    if (xoCorrPrint) {
        dev_attr_read_ll(phy_device, "xo_correction", &value);
        printf(" (*) XO Correction: "); print_value_thousands(value); printf("\n");
    }
}

#endif
