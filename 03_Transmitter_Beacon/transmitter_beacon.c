/*******************************************************************************
 * FileName: "transmitter_beacon.c" (v5)
 * Configuration file path: "conf/tx.conf"
 *******************************************************************************/
#include <stdint.h>     // uint8_t
#include <stdio.h>      // file
#include "../src/AD9361_config.h"
#include "../src/C_bsearch_procedures.h"
#include "../src/C_file_procedures.h"
#include "../src/C_numeric_procedures.h"
#include "../src/C_string_procedures.h"

// Define constants
#define CONFIG_PATH "conf/tx.conf"
#define DATA_PATH "seq/seq1.txt"

// Config KEYWORDS
// Must be sorted ALFABETICALLY !!
keyword_t arr_param[] = {{"BW", 3}, {"Fc", 1}, {"Fsa", 2}, {"gain", 4}, {"nper", 8}, {"path", 6}, {"seqlen", 5}, {"zeros", 7}};
dictionary_t params = {.dict = arr_param, .size = 8};

/********************
 * CONFIGURATION
********************/
RF_unit tx_unit = {0};
size_t Buffer_Size = 0, Sequence_Size = 0;
size_t zeros_pre = 0, zeros_post = 0;
struct {uint8_t pre:1; uint8_t post:1;} zeros_conf;
uint8_t nper = 0, nper_zeros_pre = 0, nper_zeros_post = 0;
struct buffer_ptr buffer_pointer = {0}, buffer_copy = {0};
struct buffer_ptr *tx_buffer = &buffer_pointer;
struct buffer_ptr *read_buffer = &buffer_copy;
char* data_path = NULL;

// Sequence
void load_sequence() {
    if (Sequence_Size == 0) return;
    FILE * File_Data;
    if (data_path == NULL) {
        File_Data = fopen(DATA_PATH, "r");
        ptrchk(File_Data, "opening data file", DATA_PATH);
    } else {
        File_Data = fopen(data_path, "r");
        ptrchk(File_Data, "opening data file", data_path);
    }
    
    int i, seqsz = Buffer_Size - zeros_pre - zeros_post;
    size_t j;
    int16_t val_i = 0, val_q = 0;
    char* line = NULL; size_t len = 0;
    buf_start(tx_unit.buffer, tx_unit.channel_i, tx_buffer);
    for (j = 0; (j < zeros_pre) && buf_avail(tx_buffer); j++) {
        buf_write(tx_buffer, 0, 0);
        buf_next(tx_buffer);
    }

    char* buf_begin = buffer_pointer.ptr_dat;
    for (i = 0; (getline(&line, &len, File_Data) != -1) && (i < (int)Sequence_Size) && buf_avail(tx_buffer); i++) {
        val_i = 0; val_q = 0;
        sscanf(line, "%hd\t%hd", &val_i, &val_q);
        buf_write(tx_buffer, val_i * 16, val_q * 16);
        buf_next(tx_buffer);
    }
    buffer_copy.ptr_end = buffer_pointer.ptr_dat;
    buffer_copy.ptr_inc = buffer_pointer.ptr_inc;
    for (uint8_t k = 1; (k < nper) && (i < seqsz) && buf_avail(tx_buffer); k++) {
        buffer_copy.ptr_dat = buf_begin;
        for (;buf_avail(read_buffer) && (i < seqsz) && buf_avail(tx_buffer); i++) {
            buf_copy(tx_buffer, read_buffer);
        }
    }
    if (i < seqsz) {
        printf(" (!) The file %s contains less data than expected.\n", (data_path != NULL) ? data_path : DATA_PATH);
        call_shutdown(106);
    } else {
        printf("Total sequence length:\t%d\n", i);
    }
    for (j = 0; (j < zeros_post) && buf_avail(tx_buffer); j++) {
        buf_write(tx_buffer, 0, 0);
        buf_next(tx_buffer);
    }
    if (line != NULL) free(line);
    fclose(File_Data);
}

// Config
void set_zeros_conf(int ret, int ret_begin, uint8_t value_to_set) {
    if (ret > ret_begin) zeros_conf.pre = value_to_set;
    else zeros_conf.pre = 1-value_to_set;
    if (ret > ret_begin + 1) zeros_conf.post = value_to_set;
    else zeros_conf.post = 1-value_to_set;
}
void set_zeros() {
    if (zeros_conf.pre) zeros_pre = Sequence_Size * nper_zeros_pre;
    if (zeros_conf.post) zeros_post = Sequence_Size * nper_zeros_post;
}
void load_config() {
    FILE * File_Conf;
    File_Conf = fopen(CONFIG_PATH, "r");
    ptrchk(File_Conf, "opening config file", CONFIG_PATH);

    uint8_t key = 0, state = 0; long long val;
    char* line = NULL; size_t len = 0;
    int ret = 0;
    while (getline(&line, &len, File_Conf) != -1) {
        if (state == 0) {
            char* nl = strline(line);
            get_key(&params, line, &key);
            revert_strline(nl);
            if (key != 0) state = 1;
        } else {
            char* nl = NULL;
            switch (key) {
                case 1: get_hz(line, &val);
                        ch_attr_write_ll(tx_unit.lo_channel, "frequency", val); break;
                case 2: get_hz(line, &val);
                        ch_attr_write_ll(tx_unit.rf_channel, "sampling_frequency", val); break;
                case 3: get_hz(line, &val);
                        ch_attr_write_ll(tx_unit.rf_channel, "rf_bandwidth", val); break;
                case 4: nl = strline(line); 
                        ch_attr_write_str(tx_unit.rf_channel, "hardwaregain", line); revert_strline(nl); break;
                case 5: sscanf(line, "%zu", &Sequence_Size); break;
                case 6: set_string(line, &data_path); break;
                case 7: ret = sscanf(line, "%zu %zu", &zeros_pre, &zeros_post); set_zeros_conf(ret, 0, 0); break;
                case 8: ret = sscanf(line, "%hhu %hhu %hhu", &nper, &nper_zeros_pre, &nper_zeros_post); set_zeros_conf(ret, 1, 1); break;
                default: break;
            }
            state--;
        }
    }
    free_str(&line);
    fclose(File_Conf);
    
    if (Sequence_Size == 0) {
        if (data_path != NULL) Sequence_Size = count_nl(data_path);
        else                   Sequence_Size = count_nl(DATA_PATH);
        if (Sequence_Size == 0) {
            printf(" (!) No data found in \"");
            if (data_path != NULL) printf("%s\".\n", data_path);
            else printf(DATA_PATH "\".\n");
            call_shutdown(105);
        }
    }
    if (nper == 0) nper = 1;
    Buffer_Size = nper * Sequence_Size;
    set_zeros();
    Buffer_Size += zeros_pre + zeros_post;
    RF_unit_create_buffer(&tx_unit, Buffer_Size, 1);
}
void print_config() {
    if (!configPrint) return;
    const char* divider = "----\t-----\t-----\t-----\t-----\n";
    long long val; char line[16]; const char* hz = "Hz\n";
    printf(" (*)%s", divider+4);
    ch_attr_read_ll(tx_unit.lo_channel, "frequency", &val);          print_value_unit("Carrier freq. (Fc):\t", val, hz);
    ch_attr_read_ll(tx_unit.rf_channel, "sampling_frequency", &val); print_value_unit("Sampling freq. (Fsa):\t", val, hz);
    ch_attr_read_ll(tx_unit.rf_channel, "rf_bandwidth", &val);       print_value_unit("RF Bandwidth (BW):\t", val, hz);
    ch_attr_read_str(tx_unit.rf_channel, "hardwaregain", line, 16);  printf("Hardware gain (gain):\t%s\n", line);
    printf("Data path (data):\t\"%s\"\n", (data_path != NULL) ? data_path : DATA_PATH);
    printf("\nSequence size (seqlen, nper[1]):\t"); print_value_thousands((long long)Sequence_Size); printf("IQ samp. (%hhu periods)\n", nper);
    
    printf("Zeros before seq. (nper[2], zeros[1]):\t");
    if (zeros_conf.pre) {printf("%hhu periods\n", nper_zeros_pre);}
    else {print_value_thousands(zeros_pre); printf("IQ samp.\n");}
    printf("Zeros after seq.  (nper[3], zeros[2]):\t");
    if (zeros_conf.post) {printf("%hhu periods\n", nper_zeros_post);}
    else {print_value_thousands(zeros_post); printf("IQ samp.\n");}

    printf("Total buffer size:\t"); print_value_thousands((long long)Buffer_Size);  printf("(IQ samp.)\n");
    printf(divider);
    configPrint = false;
}

/********************
 * Main function
********************/
int main () {
	signal(SIGINT, handle_sig);
    allow_null_ptr(false);
    setvbuf(stdout, NULL, _IOLBF, BUFSIZ);

    TX_unit_init(&tx_unit);
    load_config();
    load_sequence();
    print_config();

    push_buffer(tx_unit.buffer);

    while (!stop) {}
    call_shutdown(0);
    return 0;
}

/********************
 * Shutdown procedure
********************/
void disable_RF_units() {
    RF_unit_disable(&tx_unit);
}
