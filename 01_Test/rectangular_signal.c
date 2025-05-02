#define PLUTO_TX
#include "../src/AD9361_transceiver.h"

// Define constants
#define FREQ_CARRIER    GHZ(2.5)
#define FREQ_SAMPLE     MHZ(2.5)
#define RF_BANDWIDTH    MHZ(1.5)

#define SIGNAL_LENGTH   100
#define HIGH_LENGTH     50
#define BUFFER_SIZE     100 * SIGNAL_LENGTH

#define HIGH_VALUE      2000
#define LOW_VALUE      -2000

#define RECTANGLE(x) ((x < HIGH_LENGTH) ? HIGH_VALUE : LOW_VALUE)
#define INCREASE(x)  ( x = (x+1) % SIGNAL_LENGTH)

int main () {
	signal(SIGINT, handle_sig);
    allow_null_ptr(false);

    // Configure transmitter with "cyclic buffer":
    set_transmitter(FREQ_CARRIER, FREQ_SAMPLE, RF_BANDWIDTH, BUFFER_SIZE, CYCLIC);

    buf_start(txbuf, tx0_i, txdata);
    for (int i = 0; buf_avail(txdata); INCREASE(i)) {
        buf_write(txdata, RECTANGLE(i) * 16, 0 * 16);
        buf_next(txdata);
    }
    push_buffer(txbuf); // The submitted buffer will be repeated

    while (!stop) {}
    call_shutdown(0);
    return 0;
}
