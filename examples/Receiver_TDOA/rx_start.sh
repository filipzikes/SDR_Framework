FOSC=$(fw_printenv ad936x_ext_refclk | cut -d'<' -f2 | cut -d'>' -f1)
./receiver_tdoa $FOSC
