/*******************************************************************************
 * FileName: "correlation_fftw.c"
 * 
 * External libraries:
 *      FFTW    (https://www.fftw.org/doc/)   
 *          - Author: Matteo Frigo, MIT
 *          - GNU General Public License
 *******************************************************************************/
#include <stdint.h>     // uint8_t
#include <stdio.h>      // file
#include <fftw3.h>
#include "../src/AD9361_config.h"
#include "../src/C_bsearch_procedures.h"
#include "../src/C_file_procedures.h"
#include "../src/C_math_algorithm.h"
#include "../src/C_numeric_procedures.h"
#include "../src/C_string_procedures.h"
#include "../src/C_timer.h"

// Define constants
#define CONFIG_PATH "conf/rx.conf"
#define MIN_SEQ_PATH_LEN 1

// Config KEYWORDS
keyword_t arr_param[] = {{"AGC", 7}, {"BW", 3}, {"Fc", 1}, {"Fsa", 2}, {"gain", 8}, {"len", 4}, {"path", 6}, {"seqlen", 5}};
dictionary_t params = {.dict = arr_param, .size = 8};

/********************
 * SEQUENCE ARRAYS
********************/
cplxarr_int16* sequences;
char* seq_path = NULL;
size_t Sequence_Size = 0;
int seq_num = 0;

void create_arrays(int n) {
    sequences = calloc(n, sizeof(cplxarr_int16));
    ptrchk(sequences, "malloc", "cplxarr_int16[]");
    for (int i = 0; i < n; i++) {
        create_cplx_arr16(&(sequences[i]), Sequence_Size);
        if (sequences[i].size != Sequence_Size) {
            printf(" (!) Error allocating %d. cplxarr_int16\n", i+1);
            call_shutdown(111);
        }
    }
}
void load_sequence(const char* filename, cplxarr_int16 arr) {
    FILE * File_Data;
    File_Data = fopen(filename, "r");
    ptrchk(File_Data, "opening data file", filename);
    
    size_t j = 0;
    int16_t val_i = 0, val_q = 0;
    char* line = NULL; size_t len = 0;
    while ((getline(&line, &len, File_Data) != -1) && (j < Sequence_Size)) {
        val_i = 0; val_q = 0;
        sscanf(line, "%hd\t%hd", &val_i, &val_q);
        arr.real[j] = val_i;
        arr.imag[j] = val_q;
        j++;
    }
    free_str(&line);
    fclose(File_Data);
}
void load_seq_array() {
    if (Sequence_Size == 0) {printf(" (!) Undefined sequence size (seqlen)\n"); call_shutdown(105);}
    if (seq_path == NULL)   {printf(" (!) Undefined path to list of sequences (path)\n"); call_shutdown(110);}
    seq_num = count_lines(seq_path, MIN_SEQ_PATH_LEN);
    create_arrays(seq_num);
    
    FILE * file;
    file= fopen(seq_path, "r");
    ptrchk(file, "opening paths file", seq_path);
    int i = 0; char* pathline = NULL; size_t len;
    while ((i < seq_num) && (getline(&pathline, &len, file) != -1)) {
        char* nl = strline(pathline);
        if (strlen(pathline) >= MIN_SEQ_PATH_LEN) {
            load_sequence(pathline, sequences[i]);
            i++;
        }
        revert_strline(nl);
    }
    free_str(&pathline);
    fclose(file);
}

/********************
 * CONFIGURATION
********************/
RF_unit rx_unit = {0};
size_t Buffer_Size = 0;

void load_config() {
    FILE * File_Conf;
    File_Conf = fopen(CONFIG_PATH, "r");
    ptrchk(File_Conf, "opening config file", CONFIG_PATH);

    uint8_t key = 0, state = 0; long long val;
    char* line = NULL; size_t len = 0;
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
                        ch_attr_write_ll(rx_unit.lo_channel, "frequency", val); break;
                case 2: get_hz(line, &val);
                        ch_attr_write_ll(rx_unit.rf_channel, "sampling_frequency", val); break;
                case 3: get_hz(line, &val);
                        ch_attr_write_ll(rx_unit.rf_channel, "rf_bandwidth", val); break;
                case 4: sscanf(line, "%zu", &Buffer_Size); break;
                case 5: sscanf(line, "%zu", &Sequence_Size); break;
                case 6: set_string(line, &seq_path); break;
                case 7: nl = strline(line); 
                        ch_attr_write_str(rx_unit.rf_channel, "gain_control_mode", line); revert_strline(nl); break;
                case 8: nl = strline(line); 
                        ch_attr_write_str(rx_unit.rf_channel, "hardwaregain", line); revert_strline(nl); break;
                default: break;
            }
            state--;
        }
    }
    free_str(&line);
    fclose(File_Conf);

    if (Buffer_Size == 0) {printf(" (!) Undefined buffer size (len)\n"); call_shutdown(104);}
    RF_unit_create_buffer(&rx_unit, Buffer_Size, 0);
}
void print_config() {
    if (!configPrint) return;
    const char* divider = "----\t-----\t-----\t-----\t-----\n";
    long long val; char line[16]; const char* hz = "Hz\n";
    printf(" (*)%s", divider+4);
    ch_attr_read_ll(rx_unit.lo_channel, "frequency", &val);          print_value_unit("Carrier freq. (Fc):\t", val, hz);
    ch_attr_read_ll(rx_unit.rf_channel, "sampling_frequency", &val); print_value_unit("Sampling freq. (Fsa):\t", val, hz);
    ch_attr_read_ll(rx_unit.rf_channel, "rf_bandwidth", &val);       print_value_unit("RF Bandwidth (BW):\t", val, hz);
    ch_attr_read_str(rx_unit.rf_channel, "gain_control_mode", line, 16); printf("Gain control mode(AGC): %s\n", line);
    ch_attr_read_str(rx_unit.rf_channel, "hardwaregain", line, 16);      printf("Hardware gain (gain):\t%s\n", line);
    printf("Seqeuence path (paths):\t\"%s\"\n", (seq_path != NULL) ? seq_path : "---");
    printf("Buffer size (len):\t%zd (IQ samp.)\n", Buffer_Size);
    printf("Sequence size (seqlen):\t%zd (IQ samp.)\n", Sequence_Size);
    printf(divider);
    configPrint = false;
}

/********************
 * Main function
********************/
int16_t value_i, value_q;
int main () {
	signal(SIGINT, handle_sig);
    allow_null_ptr(false);

    RX_unit_init(&rx_unit);
    load_config();
    load_seq_array();
    print_config();

    if (seq_num > 1) {
        size_t M = Sequence_Size;
        size_t N = 2*M-1;
        fftw_complex *in, *out, *tmp1, *tmp2;
        fftw_plan p;

        in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N);
        out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N);
        tmp1 = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N);
        tmp2 = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N);
        timer_start();

        // FFT SEQ 1
        p = fftw_plan_dft_1d(N, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
        for (size_t i = 0; i < M; i++) {
            in[i][0] = sequences[0].real[i];
            in[i][1] = sequences[0].imag[i];
        }
        for (size_t i = M; i < N; i++) {
            in[i][0] = 0;
            in[i][1] = 0;
        }
        fftw_execute(p);
        memcpy(tmp1, out, sizeof(fftw_complex) * N);
        
        // FFT SEQ 2
        for (size_t i = 0; i < M; i++) {
            in[i][0] = sequences[0].real[i];
            in[i][1] = sequences[0].imag[i];
        }
        for (size_t i = M; i < N; i++) {
            in[i][0] = 0;
            in[i][1] = 0;
        }
        fftw_execute(p);
        memcpy(tmp2, out, sizeof(fftw_complex) * N);

        // IFFT
        fftw_destroy_plan(p);
        p = fftw_plan_dft_1d(N, in, out, FFTW_BACKWARD, FFTW_ESTIMATE);
        for (size_t i = 0; i < N; i++) {
            in[i][0] = tmp1[i][0] * tmp2[i][0] + tmp1[i][1] * tmp2[i][1];
            in[i][1] = tmp1[i][1] * tmp2[i][0] - tmp1[i][0] * tmp2[i][1];
        }
        fftw_execute(p);
        fftw_destroy_plan(p);

        timer_stop();
        timer_print_full();
        
        FILE * fd;
        fd = fopen("out.txt", "w");
        if (fd != NULL) {
            for (size_t i = 0; i < N; i++) {fprintf(fd, "%f\t%f\n", out[i][0], out[i][1]);}
            fclose(fd);
        }
        
        fftw_free(in); fftw_free(out); fftw_free(tmp1); fftw_free(tmp2);
    }

    //size_t r_iq = refill_buffer(rxbuf);     // Receive samples

    //buf_start(rxbuf, rx0_i, rxdata);        // Absence causes warning: "rxdata" not used
    /*while(buf_avail(rxdata)) {
        buf_read(rxdata, &value_i, &value_q);
        buf_next(rxdata);
    }*/
    
    call_shutdown(0);
    return 0;
}

/********************
 * Shutdown procedure
********************/
void disable_RF_units() {
    RF_unit_disable(&rx_unit);
}
