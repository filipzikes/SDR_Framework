/*******************************************************************************
 * FileName: "receiver_tdoa.c" (v3)
 * Configuration file path: "conf/rx.conf"
 * 
 * External libraries:
 *      FFTW    (https://www.fftw.org/doc/)   
 *          - Author: Matteo Frigo, MIT
 *          - GNU General Public License
 *      polyfit (https://github.com/henryfo/polyfit) 
 *          - Author: Henry M. Forson
 *          - MIT License
 *******************************************************************************/
#include <stdint.h>     // uint8_t
#include <stdio.h>      // file
#include <fftw3.h>
#include "../src/AD9361_config.h"
#include "../src/C_bsearch_procedures.h"
#include "../src/C_file_procedures.h"
#include "../src/C_numeric_procedures.h"
#include "../src/C_string_procedures.h"
#include "../src/C_timer.h"
#include "../src/lib_polyfit.h"

// Constants
#define CONFIG_PATH "conf/rx.conf"
#define MIN_SEQ_PATH_LEN 1
#define SPEED_OF_LIGHT 299792458.0    // SPEED of signal => DELTA
#define F_OSC_NOM      40000000.0
//fw_printenv ad936x_ext_refclk | cut -d'<' -f2 | cut -d'>' -f1
#define REPLICA_PERIOD 0.00025        // Default value
const unsigned int order = 1;         // LINEAR (polyfit)

// Config KEYWORDS      !! Must be sorted ALFABETICALLY !!
keyword_t arr_param[] = {{"AGC", 7}, {"BW", 3}, {"FFTW_flags", 11},  {"Fc", 1}, {"Fsa", 2},
    {"gain", 8}, {"input", 12}, {"inputbin", 13}, {"len", 4}, {"mult", 9}, {"path", 6}, {"seqlen", 5}, {"seqper", 14}, {"thr", 10}};
dictionary_t params = {.dict = arr_param, .size = 14};
const char* fftw_flags_str[] = {"FFTW_ESTIMATE", "FFTW_MEASURE"};

// MACROS
#define MULTIPLE_DIV (mult!=0) ? (size_t)mult : Buffer_Size/Sequence_Size
#define MULTIPLE_MOD Buffer_Size%Sequence_Size

// DEBUG
char filename[32];
void double_array_to_file(double *array, size_t n, const char* filename);

/********************
 * ARRAYS (Sequence, signal proc.)
 *   PNR = "Peak-to-noise Ratio"
********************/
fftw_complex *seq_array, *sig_array, *xcorr_array;
double *PNR_array, *peak_idx_array, *seg_idx_array, *result_coef;
size_t * idx_array;
uint32_t result_polyfit = 0;

char *seq_path = NULL, *input_path = NULL;
size_t Sequence_Size = 0;
size_t Sequence_Number = 0;
size_t Nc = 0, Nr = 0, M = 0;

void create_arrays() {
    const char *err1 = "fftw_malloc|create_arrays()";
    Nc = Sequence_Number * Sequence_Size;
    Nr = M * Sequence_Size;
    
    // Malloc FFTW arrays
    seq_array = (fftw_complex*) fftw_malloc(Nc * sizeof(fftw_complex));
    ptrchk(seq_array, err1, "seq");
    sig_array = (fftw_complex*) fftw_malloc(Nr * sizeof(fftw_complex));
    ptrchk(sig_array, err1, "sig");
    xcorr_array = (fftw_complex*) fftw_malloc(Nr * sizeof(fftw_complex));
    ptrchk(xcorr_array, err1, "xcorr");
    
    // Malloc xcorr-processing arrays
    idx_array = (size_t*) malloc(M * sizeof(size_t));
    ptrchk(idx_array, err1+5, "idx");
    PNR_array = (double*) malloc(M * sizeof(double));
    ptrchk(PNR_array, err1+5, "PNR");
    peak_idx_array = (double*) malloc(M * sizeof(double));
    ptrchk(peak_idx_array, err1+5, "peak_idx");
    seg_idx_array = (double*) malloc(M * sizeof(double));
    ptrchk(seg_idx_array, err1+5, "seg_idx");
    result_coef = (double*) malloc(Sequence_Number * (order+1) * sizeof(double));
    ptrchk(result_coef, err1+5, "result_coef");
    for(size_t i = 0; i < Sequence_Number*(order+1); i++) {result_coef[i] = 0.0;}
}
void load_sequence(const char* filename, size_t idx) {
    FILE * File_Data;
    File_Data = fopen(filename, "r");
    ptrchk(File_Data, "opening data file", filename);
    fftw_complex *seq_partial = seq_array + idx * Sequence_Size;
    
    size_t j = 0;
    int16_t val_i = 0, val_q = 0;
    char* line = NULL; size_t len = 0;
    while ((getline(&line, &len, File_Data) != -1) && (j < Sequence_Size)) {
        val_i = 0; val_q = 0;
        sscanf(line, "%hd\t%hd", &val_i, &val_q);
        seq_partial[j][0] = (double) val_i;
        seq_partial[j][1] = (double) val_q;
        j++;
    }
    free_str(&line);
    fclose(File_Data);
}
void load_seq_array() {
    if (Sequence_Size == 0) {printf(" (!) Undefined sequence size (seqlen)\n"); call_shutdown(105);}
    if (seq_path == NULL)   {printf(" (!) Undefined path to list of sequences (path)\n"); call_shutdown(110);}
    Sequence_Number = count_lines(seq_path, MIN_SEQ_PATH_LEN);
    create_arrays();
    
    FILE * file;
    file = fopen(seq_path, "r");
    ptrchk(file, "opening paths file", seq_path);
    size_t i = 0; char* pathline = NULL; size_t len;
    while ((i < Sequence_Number) && (getline(&pathline, &len, file) != -1)) {
        char* nl = strline(pathline);
        if (strlen(pathline) >= MIN_SEQ_PATH_LEN) {
            load_sequence(pathline, i);
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
struct buffer_ptr buffer_pointer = {0};
struct buffer_ptr *rx_buffer = &buffer_pointer;
size_t Buffer_Size = 0;
uint16_t mult = 0;
double PNR_threshold = 2.0;
double idx_to_meter = 0;
double idx_dif_to_hz = REPLICA_PERIOD;
unsigned long f_osc = F_OSC_NOM;
uint8_t fftw_flags_num = 255, input_bin = 0;

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
                        ch_attr_write_ll(rx_unit.rf_channel, "sampling_frequency", val);
                        idx_to_meter = SPEED_OF_LIGHT/val; break;
                case 3: get_hz(line, &val);
                        ch_attr_write_ll(rx_unit.rf_channel, "rf_bandwidth", val); break;
                case 4: sscanf(line, "%zu", &Buffer_Size); break;
                case 5: sscanf(line, "%zu", &Sequence_Size); break;
                case 6: set_string(line, &seq_path); break;
                case 7: nl = strline(line); 
                        ch_attr_write_str(rx_unit.rf_channel, "gain_control_mode", line); revert_strline(nl); break;
                case 8: sscanf(line, "%lld", &val);
                        ch_attr_write_ll(rx_unit.rf_channel, "hardwaregain", val); break;
                case 9: sscanf(line, "%hu", &mult); break;
                case 10: sscanf(line, "%lf", &PNR_threshold); break;
                case 11: sscanf(line, "%hhu", &fftw_flags_num); break;
                case 12: set_string(line, &input_path); break;
                case 13: sscanf(line, "%hhu", &input_bin); break;
                case 14: sscanf(line, "%lf", &idx_dif_to_hz); break;
                default: break;
            }
            state--;
        }
    }
    free_str(&line);
    fclose(File_Conf);

    if (idx_dif_to_hz == 0) {idx_dif_to_hz = REPLICA_PERIOD;}
    if (mult != 0) {Buffer_Size = mult * Sequence_Size;}
    if (Buffer_Size == 0) {printf(" (!) Undefined buffer size (len) or multiple of seq. size (mult, seqlen)\n"); call_shutdown(104);}
    if (Buffer_Size < Sequence_Size) {printf(" (!) Buffer size (len) is less than seq. size (seqlen)\n"); call_shutdown(104);}
    if (Buffer_Size % Sequence_Size != 0) {printf(" (W) 'Buffer size' is not integer multiple of 'Sequence size'\n");}
    M = Buffer_Size / Sequence_Size;
    RF_unit_create_buffer(&rx_unit, Buffer_Size, 0);
}
void print_config() {
    if (!configPrint) return;
    const char* divider = "----\t-----\t-----\t-----\t-----\n";
    long long val; char line[16]; const char* hz = "Hz\n";
    printf(" (*)%s", divider+4);
    printf("Osc. nominal frequency:\t"); print_value_thousands((long long)f_osc); printf(hz);
    printf("Sequence period (seqper): %lf s\n", idx_dif_to_hz);
    ch_attr_read_ll(rx_unit.lo_channel, "frequency", &val);          print_value_unit("Carrier freq. (Fc):\t", val, hz);
    ch_attr_read_ll(rx_unit.rf_channel, "sampling_frequency", &val); print_value_unit("Sampling freq. (Fsa):\t", val, hz);
        idx_dif_to_hz = f_osc / idx_dif_to_hz / val;
    ch_attr_read_ll(rx_unit.rf_channel, "rf_bandwidth", &val);       print_value_unit("RF Bandwidth (BW):\t", val, hz);
    ch_attr_read_str(rx_unit.rf_channel, "gain_control_mode", line, 16); printf("Gain control mode (AGC): %s\n", line);
    ch_attr_read_str(rx_unit.rf_channel, "hardwaregain", line, 16);      printf("Hardware gain (gain):\t %s\n", line);
    printf("Seqeuence path (paths):\t\"%s\"\n", (seq_path != NULL) ? seq_path : "---");
    printf("Buffer size (len):\t");         print_value_thousands((long long)Buffer_Size);  printf("(IQ samp.)\n");
    printf("Sequence size (seqlen):\t");    print_value_thousands((long long)Sequence_Size);printf("(IQ samp.)\n");
    printf("Buf./Seq. size (mult):\t%hhu", MULTIPLE_DIV);
        int unused = MULTIPLE_MOD;
        if (unused != 0) printf(" (+ %zd unused samp.)", unused);
    printf("\nPNR threshold (thr):\t%.2lf\n", PNR_threshold);
    if (input_path != NULL) printf("Signal record (input):\t\"%s\"%s\n", input_path, (input_bin==1)?" (bin)":"");
    if (fftw_flags_num < 2) printf("FFTW flags:\t\t%s\n", fftw_flags_str[fftw_flags_num]);
    printf(divider);
    configPrint = false;
}

/********************
 * FFTW functions
********************/
fftw_plan p1, p2, p3;
unsigned flags = FFTW_ESTIMATE;
void fftw_set_flags() {
    switch(fftw_flags_num) {
        case 1: flags = FFTW_MEASURE; break;
        default: flags = FFTW_ESTIMATE; break;
    }
}
// (1) FFT of replics (beacon sequences, "c(t)")
void fftw_create_seq_plan() {
    int rank = 1;
    int n[1]; n[0] = Sequence_Size; // inembed, onembed = n
    int howmany = Sequence_Number;
    int idist = n[0], odist = n[0];
    int istride = 1, ostride = 1;
    p1 = fftw_plan_many_dft(rank, n, howmany,
        seq_array, n, istride, idist,
        seq_array, n, ostride, odist,
        FFTW_FORWARD, flags);
}
void calc_sequence_fft() {
    fftw_execute(p1);
    fftw_destroy_plan(p1);
}
// (2) FFT of received signal ("r(t)")
void fftw_create_sig_plan() {
    int rank = 1;
    int n[1]; n[0] = Sequence_Size; // inembed, onembed = n
    int howmany = M;
    int idist = n[0], odist = n[0];
    int istride = 1, ostride = 1;
    p2 = fftw_plan_many_dft(rank, n, howmany,
        sig_array, n, istride, idist,
        sig_array, n, ostride, odist,
        FFTW_FORWARD, flags);
}
void calc_signal_fft() {
    fftw_execute(p2);
}
// (3) IFFT of Hadamard product of (1) and (2)*
void fftw_create_xcorr_plan() {
    int rank = 1;
    int n[1]; n[0] = Sequence_Size; // inembed, onembed = n
    int howmany = M;
    int idist = n[0], odist = n[0];
    int istride = 1, ostride = 1;
    p3 = fftw_plan_many_dft(rank, n, howmany,
        xcorr_array, n, istride, idist,
        xcorr_array, n, ostride, odist,
        FFTW_BACKWARD, flags);
}
void calc_xcorr(size_t seq_idx) {
    size_t c_begin = seq_idx * Sequence_Size, c, r;
    for (size_t r_begin = 0; r_begin < Nr; r_begin += Sequence_Size) {
    for (size_t j = 0; j < Sequence_Size; j++) {
        c = c_begin + j;
        r = r_begin + j;
        xcorr_array[r][0] = sig_array[r][0] * seq_array[c][0] + sig_array[r][1] * seq_array[c][1];
        xcorr_array[r][1] = sig_array[r][1] * seq_array[c][0] - sig_array[r][0] * seq_array[c][1];
    }}
    fftw_execute(p3);
}

/********************
 * SIGNAL Processing
********************/
void process_xcorr(size_t seq_idx) {
    size_t p_idx = 0, r_idx = 0, idx, r;
    double tmp, mean, peak;
    for (size_t r_begin = 0; r_begin < Nr; r_begin += Sequence_Size) {
        idx = 0;
        mean = 0.0;
        peak = 0.0;
        for (size_t j = 0; j < Sequence_Size; j++) {
            r = r_begin + j;
            tmp = xcorr_array[r][0] * xcorr_array[r][0] + xcorr_array[r][1] * xcorr_array[r][1];
            mean += tmp;
            if (tmp > peak) {
                peak = tmp;
                idx = j;
            }
        }
        mean = mean/Sequence_Size;
        idx_array[r_idx] = idx+1;
        PNR_array[r_idx] = peak/mean;
        if (PNR_array[r_idx] >= PNR_threshold) {
            peak_idx_array[p_idx] = (double)(idx+1);
            seg_idx_array[p_idx]  = (double)(r_idx+1);
            p_idx++;
        }
        r_idx++;
    }
    int result = polyfit(p_idx, seg_idx_array, peak_idx_array, (order+1), result_coef + (size_t)(seq_idx * (order+1)));
    if (result == 0) {
        result_polyfit |= (1<<seq_idx);
    }
    sprintf(filename, "x_%zd.txt", seq_idx+1); double_array_to_file(seg_idx_array, p_idx, filename);
    sprintf(filename, "y_%zd.txt", seq_idx+1); double_array_to_file(peak_idx_array, p_idx, filename);
}
void print_result() {
    if (Sequence_Number == 2) {
        // DELTA
        double delta = result_coef[2*order+1] - result_coef[order];
        if (delta < 0) delta += Sequence_Size;
        delta *= idx_to_meter;
        if ((result_polyfit & 0b11) != 0b11) printf("  --- --- ---\t| ");
        else if ((delta < 1E10) && (delta > -1E9)) printf("%13.2lf\t| ", delta);
        else printf("%13.6lE\t| ", delta);
    }
    for (size_t i = 0; i < Sequence_Number; i++) {
        // dF = a/T_seq*T_rx*F_osc
        double dF = result_coef[i*(order+1)+order-1];
        dF *= idx_dif_to_hz;
        if ((result_polyfit & (1<<i)) == 0) printf("%s  --- --- ---", (i==0)?"":"\t| ");
        else if ((dF < 1E10) && (dF > -1E9)) printf("%s%13.2lf", (i==0)?"":"\t| ", dF);
        else printf("%s%13.6lE", (i==0)?"":"\t| ", dF);
    }
    printf("\t| "); // Polyfit OK
    for (size_t i = 0; i < Sequence_Number; i++) {
        if ((result_polyfit & (1<<i)) != 0) printf(" %zd", i+1);
    }
    printf("\n");
}

/********************
 * DEBUG:
 * Write array to file
********************/
void complex_array_to_file(fftw_complex *array, size_t n, const char* filename) {
    FILE * File_Out;
    File_Out = fopen(filename, "w");
    ptrchk(File_Out, "opening output file", filename);

    for (size_t i = 0; i < n; i++) {
        fprintf(File_Out, "%.4e\t%.4e\n", array[i][0], array[i][1]);
    }

    fclose(File_Out);
}
void complex_array_to_bin(fftw_complex *array, size_t n, const char* filename) {
    FILE * File;
    File = fopen(filename, "wb");
    ptrchk(File, "opening w-binary file", filename);
    fwrite((double*)array, sizeof(double), 2*n, File);
    fclose(File);
}
void double_array_to_file(double *array, size_t n, const char* filename) {
    FILE * File_Out;
    File_Out = fopen(filename, "w");
    ptrchk(File_Out, "opening output file", filename);

    for (size_t i = 0; i < n; i++) {
        //fprintf(File_Out, "%.4e\n", array[i]);
        fprintf(File_Out, "%lf\n", array[i]);
    }

    fclose(File_Out);
}
void sizet_array_to_file(size_t *array, size_t n, const char* filename) {
    FILE * File_Out;
    File_Out = fopen(filename, "w");
    ptrchk(File_Out, "opening output file", filename);

    for (size_t i = 0; i < n; i++) {
        fprintf(File_Out, "%zd\n", array[i]);
    }

    fclose(File_Out);
}
void buffer_to_file(size_t n, const char* filename) {
    FILE * File_Out;
    File_Out = fopen(filename, "w");
    ptrchk(File_Out, "opening output file", filename);

    int16_t val_i, val_q;
    buf_start(rx_unit.buffer, rx_unit.channel_i, rx_buffer);
    for (size_t i = 0; (i < n) && buf_avail(rx_buffer); i++) {
        buf_read(rx_buffer, &val_i, &val_q);
        fprintf(File_Out, "%hd\t%hd\n", val_i, val_q);
        buf_next(rx_buffer);
    }

    fclose(File_Out);
}
void load_virtual_signal(const char* filename) {
    FILE * File_Data;
    File_Data = fopen(filename, "r");
    ptrchk(File_Data, "opening signal file", filename);
    
    size_t j = 0;
    double val_i = 0, val_q = 0;
    char* line = NULL; size_t len = 0;
    while ((getline(&line, &len, File_Data) != -1) && (j < Nr)) {
        val_i = 0; val_q = 0;
        sscanf(line, "%lf\t%lf", &val_i, &val_q);
        sig_array[j][0] = val_i;
        sig_array[j][1] = val_q;
        j++;
    }
    free_str(&line);
    for (; j < Nr; j++) {
        sig_array[j][0] = 0;
        sig_array[j][1] = 0;
    }
    fclose(File_Data);
}
void load_binary_signal(fftw_complex *array, size_t n, const char* filename) {
    FILE * File_Data;
    File_Data = fopen(filename, "r");
    ptrchk(File_Data, "opening signal-binary file", filename);
    const size_t ret_code = fread((double*)array, sizeof(double), 2*n, File_Data);
    if (ret_code != 2*n) {
        if (feof(File_Data)) printf(" (!) Unexpected end of file: %s\n", filename);
        else if (ferror(File_Data)) printf(" (!) Error reading %s\n", filename);
        call_shutdown(201);
    }
    fclose(File_Data);
}

/********************
 * Main function
********************/
int main (int argc, char *argv[]) {
	signal(SIGINT, handle_sig);
    allow_null_ptr(false);
    setvbuf(stdout, NULL, _IOLBF, BUFSIZ);
    if (argc > 1) {
        sscanf(argv[1], "%lu", &f_osc);
    }

    // Configure receiver
    RX_unit_init(&rx_unit);
    load_config();
    load_seq_array();
    fftw_set_flags();
    fftw_create_seq_plan();
    fftw_create_sig_plan();
    fftw_create_xcorr_plan();
    calc_sequence_fft();
    print_config();

    printf(" Wall time [ms]\t|   Delta [m]\t|    dF1 [Hz]\t|    dF2 [Hz]\t| Polyfit OK\n");
    while(!stop) {
        if (input_path == NULL) {
            // Receive signal samples
            refill_buffer(rx_unit.buffer);
            //size_t rx_nsa = refill_buffer(rx_unit.buffer);
            //printf("RX samples: %zd | ", rx_nsa/4);

            size_t i;
            int16_t val_i, val_q;
            buf_start(rx_unit.buffer, rx_unit.channel_i, rx_buffer);
            for (i = 0; (i < Nr) && buf_avail(rx_buffer); i++) {
                buf_read(rx_buffer, &val_i, &val_q);
                sig_array[i][0] = (double) val_i;
                sig_array[i][1] = (double) val_q;
                buf_next(rx_buffer);
            }
            for (; i < Sequence_Size; i++) {
                sig_array[i][0] = 0;
                sig_array[i][1] = 0;
            }
        } else {
            if (input_bin == 1) {load_binary_signal(sig_array, Nr, input_path);}
            else {load_virtual_signal(input_path);}
            stop = true;
        }
        timer_start();
        // Calculate signal FFT and cross-correlation
        calc_signal_fft();
        result_polyfit = 0;
        for (size_t i = 0; i < Sequence_Number; i++) {
            calc_xcorr(i);
            process_xcorr(i);
            timer_pause();
            sprintf(filename, "idx_array_%zd.txt", i+1); sizet_array_to_file(idx_array, M, filename);
            sprintf(filename, "PNR_array_%zd.txt", i+1); double_array_to_file(PNR_array, M, filename);
            sprintf(filename, "coef_array_%zd.txt", i+1); double_array_to_file(result_coef + i*(order+1), (order+1), filename);
            sprintf(filename, "xcorr_array_%zd.bin", i+1); complex_array_to_bin(xcorr_array, Nr, filename);
            timer_resume();
        }
        timer_stop();
        printf("%15.2lf\t| ", timer_get_wall());
        print_result();
    }

    // DEBUG: Write arrays to file
    //buffer_to_file(Sequence_Size, "sig.out");
    //complex_array_to_file(sig_array, Sequence_Size, "sig_fft.out");
    // *****  *****  *****  *****
    //complex_array_to_bin(seq_array, Nc, "seq_fft.bin");

    call_shutdown(0);
    return 0;
}

/********************
 * Shutdown procedure
********************/
void disable_RF_units() {
    RF_unit_disable(&rx_unit);
    printf(" (*) FFTW: Destroying plans & arrays\n");
    fftw_destroy_plan(p2);
    fftw_destroy_plan(p3);
    if (seq_array != NULL) fftw_free(seq_array);
    if (sig_array != NULL) fftw_free(sig_array);
    if (xcorr_array != NULL) fftw_free(xcorr_array);
}
