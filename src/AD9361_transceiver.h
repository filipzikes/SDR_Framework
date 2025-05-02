#ifndef __AD9361_TRANSCEIVER__
#define __AD9361_TRANSCEIVER__

#include "AD9361_procedures.h"

enum buffer_type {NON_CYCLIC, CYCLIC};
typedef long long ll;

// IIO device: context and PHY channels device
static struct iio_context *ctx    = NULL;
static struct iio_device  *phy    = NULL;

// TX variables
#ifdef PLUTO_TX
static struct iio_device  *stream_tx = NULL;
static struct iio_channel *lo_tx     = NULL;
static struct iio_channel *tx_chn    = NULL;
static struct iio_channel *tx0_i     = NULL;
static struct iio_channel *tx0_q     = NULL;
static struct iio_buffer  *txbuf     = NULL;
static struct buffer_ptr   data1     = {0};
static struct buffer_ptr  *txdata    = &data1;
ssize_t sample_size_tx;
#endif

// RX variables
#ifdef PLUTO_RX
static struct iio_device  *stream_rx = NULL;
static struct iio_channel *lo_rx     = NULL;
static struct iio_channel *rx_chn    = NULL;
static struct iio_channel *rx0_i     = NULL;
static struct iio_channel *rx0_q     = NULL;
static struct iio_buffer  *rxbuf     = NULL;
static struct buffer_ptr   data0     = {0};
static struct buffer_ptr  *rxdata    = &data0;
ssize_t sample_size_rx;
#endif

void set_ctx_phy() {
    ctx_local(&ctx);                                          // Context from local IIO devices (or uri="ip:192.168.2.1")
    ctx_find_device(ctx, &phy, "ad9361-phy");                 // AD9361 PHY channels device
}

#ifdef PLUTO_TX
void set_transmitter(ll freq_carrier, ll freq_sample, ll rf_bandwidth, ll buffer_size, enum buffer_type buffer_cyclic) {
    set_ctx_phy();
    ctx_find_device(ctx, &stream_tx, "cf-ad9361-dds-core-lpc"); // AD9361 TX streaming device

    dev_find_channel(phy, &tx_chn, "voltage0", OUTPUT);         // PHY TX channels
    ch_attr_write_str(tx_chn, "rf_port_select", "A");
    ch_attr_write_ll(tx_chn, "rf_bandwidth", rf_bandwidth);
    ch_attr_write_ll(tx_chn, "sampling_frequency", freq_sample);
    
    dev_find_channel(phy, &lo_tx, "altvoltage1", OUTPUT);       // LO channel (TX)
    ch_attr_write_ll(lo_tx, "frequency", freq_carrier);
    
    dev_find_channel(stream_tx, &tx0_i, "voltage0", OUTPUT);    // TX-I channel 
    dev_find_channel(stream_tx, &tx0_q, "voltage1", OUTPUT);    // TX-Q channel
    iio_channel_enable(tx0_i);
    iio_channel_enable(tx0_q);

    dev_create_buffer(stream_tx, &txbuf, buffer_size, buffer_cyclic==CYCLIC);
    sample_size_tx = iio_device_get_sample_size(stream_tx);
}
#endif

#ifdef PLUTO_RX
void set_receiver(ll freq_carrier, ll freq_sample, ll rf_bandwidth, ll buffer_size, enum buffer_type buffer_cyclic) {
    set_ctx_phy();
    ctx_find_device(ctx, &stream_rx, "cf-ad9361-lpc");          // AD9361 RX streaming device
    
    dev_find_channel(phy, &rx_chn, "voltage0", INPUT);          // PHY RX channels
    ch_attr_write_str(rx_chn, "rf_port_select", "A_BALANCED");
    ch_attr_write_ll(rx_chn, "rf_bandwidth", rf_bandwidth);
    ch_attr_write_ll(rx_chn, "sampling_frequency", freq_sample);

    dev_find_channel(phy, &lo_rx, "altvoltage0", OUTPUT);       // LO channel (RX)
    ch_attr_write_ll(lo_rx, "frequency", freq_carrier);

    dev_find_channel(stream_rx, &rx0_i, "voltage0", INPUT);        // RX-I channel 
    dev_find_channel(stream_rx, &rx0_q, "voltage1", INPUT);        // RX-Q channel
    iio_channel_enable(rx0_i);
    iio_channel_enable(rx0_q);

    dev_create_buffer(stream_rx, &rxbuf, buffer_size, buffer_cyclic==CYCLIC);
    sample_size_rx = iio_device_get_sample_size(stream_rx);
}
#endif

void shutdown(void) {
    printf(" (*) Destroying buffers\n");
    #ifdef PLUTO_TX
	    if (txbuf) { iio_buffer_destroy(txbuf); }
    #endif
    #ifdef PLUTO_RX
	    if (rxbuf) { iio_buffer_destroy(rxbuf); }
    #endif

    printf(" (*) Disabling streaming channels\n");
    #ifdef PLUTO_TX
	    if (tx0_i) { iio_channel_disable(tx0_i); }
	    if (tx0_q) { iio_channel_disable(tx0_q); }
    #endif
    #ifdef PLUTO_RX
        if (rx0_i) { iio_channel_disable(rx0_i); }
	    if (rx0_q) { iio_channel_disable(rx0_q); }
    #endif

	printf(" (*) Destroying context\n");
	if (ctx) { iio_context_destroy(ctx); }
	exit(exitCode);
}

#endif
