#define PLUTO_TX
#include "../src/AD9361_transceiver.h"

// Define constants
#define BUFFER_SIZE     (1024 * 1024)
#define FREQ_CARRIER    GHZ(2.5)
#define FREQ_SAMPLE     MHZ(2.5)
#define RF_BANDWIDTH    MHZ(1.5)
ssize_t ntx = 0;

int main () {
	signal(SIGINT, handle_sig);
    allow_null_ptr(false);

    // Configure transmitter with "non-cyclic buffer":
    set_transmitter(FREQ_CARRIER, FREQ_SAMPLE, RF_BANDWIDTH, BUFFER_SIZE, NON_CYCLIC);

    while (!stop) {
        ntx += push_buffer(txbuf) / sample_size_tx;
        buf_start(txbuf, tx0_i, txdata);
        while (buf_avail(txdata)) {
            // Value range is -2047..2047
            buf_write(txdata, 2000 * 16, 0 * 16);
            buf_next(txdata);
        }
        printf("\tTX %8.2f MSmp\n", ntx/1e6);
    }
    
    call_shutdown(0);
    return 0;
}
