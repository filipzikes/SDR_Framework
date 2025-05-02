#ifndef __AD9361_PROCEDURES__
#define __AD9361_PROCEDURES__

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <iio.h>

/********************
 * APP CONFIGURATION
********************/
bool stop = false;
bool allowPtr = false;
bool xoCorrPrint = true;
bool configPrint = true;
int exitCode = 1;

void shutdown();
void handle_sig(int sig) {
	printf("\n (/) Waiting for process to finish... Got signal %d\n", sig);
	stop = true;
}
void set_exit(int exit_code) {
	exitCode = exit_code;
}
void allow_null_ptr(bool yes) {
    allowPtr = yes;
}
void call_shutdown(int exit_code) {
	exitCode = exit_code;
	shutdown();
}

/********************
 * AD9361 CONFIGURATION
********************/
#define MHZ(x) ((long long)(x*1000000.0 + .5))
#define GHZ(x) ((long long)(x*1000000000.0 + .5))

enum port_dir {INPUT, OUTPUT};
enum details_format {NONE, LONGLONG, STRING};
enum details_process {WRITING, READING};
typedef union {
    const char * str;
    long long ll;
} details_d;
typedef struct {
    enum details_format format:2;
    enum details_process proc:1;
} details_t;
typedef struct buffer_ptr {
    char* ptr_dat;
    char* ptr_end;
    ptrdiff_t ptr_inc;
} buffer_t;

void ptrchk(void* ptr, const char* what, const char* details) {
    if (ptr == NULL && !allowPtr) {
        fprintf(stderr, " (!) Error getting pointer: %s [%s]\n", what, details);
        shutdown();
    }
}
void errchk(int v, const char* what, details_t type, details_d details) {
    if (v < 0) {
        switch (type.proc) {
            case READING: fprintf(stderr, " (!) Error %d reading attribute \"%s", v, what); break;
            case WRITING: default:
                          fprintf(stderr, " (!) Error %d writing attribute \"%s", v, what); break;
        }
        switch (type.format) {
            case STRING: fprintf(stderr, "=%.10s\"\n", details.str); break;
            case LONGLONG: fprintf(stderr, "=%lld\"\n", details.ll); break;
            default: fprintf(stderr, "\"\n"); break;
        }
        fprintf(stderr, " (!) Value may not be supported.\n");
        shutdown();
    }
}

void ctx_local(struct iio_context** ctx) {
    *ctx = iio_create_local_context();
    ptrchk((void*)(*ctx), "Context", "local");
}
void ctx_from_uri(struct iio_context** ctx, const char* uri) {
    *ctx = iio_create_context_from_uri(uri);
    ptrchk((void*)(*ctx), "Context", uri);
}
void ctx_find_device(struct iio_context* ctx, struct iio_device** dev, const char* name) {
    *dev = iio_context_find_device(ctx, name);
    ptrchk((void*)(*dev), "Device", name);
}
void dev_find_channel(struct iio_device* dev, struct iio_channel** chan, const char* name, enum port_dir type) {
    *chan = iio_device_find_channel(dev, name, type==OUTPUT);
    size_t len = strlen(name)+5;
    char tmp[len];
    if (type==OUTPUT)
        snprintf(tmp, len, "%s%s", name, "/out");
    else
        snprintf(tmp, len, "%s%s", name, "/in");
    ptrchk((void*)(*chan), "Channel", (const char *)tmp);
}
void dev_create_buffer(struct iio_device* dev, struct iio_buffer** buf, size_t samples_count, bool cyclic) {
    *buf = iio_device_create_buffer(dev, samples_count, cyclic);
}
void dev_attr_read_str(struct iio_device* dev, const char* name, char* dst, size_t len) {
    int ret = iio_device_attr_read(dev, name, dst, len);
    details_t t; t.format = NONE; t.proc = READING;
    details_d d;
    errchk(ret, name, t, d);
}
void dev_attr_write_ll(struct iio_device* dev, const char* name, long long value) {
    int ret = iio_device_attr_write_longlong(dev, name, value);
    details_t t; t.format = LONGLONG; t.proc = WRITING;
    details_d d; d.ll = value;
    errchk(ret, name, t, d);
}
void dev_attr_read_ll(struct iio_device* dev, const char* name, long long* value) {
    int ret = iio_device_attr_read_longlong(dev, name, value);
    details_t t; t.format = NONE; t.proc = READING;
    details_d d;
    errchk(ret, name, t, d);
}
void ch_attr_write_str(struct iio_channel* chan, const char* name, const char* value) {
    int ret = iio_channel_attr_write(chan, name, value);
    details_t t; t.format = STRING; t.proc = WRITING;
    details_d d; d.str = value;
    errchk(ret, name, t, d);
}
void ch_attr_read_str(struct iio_channel* chan, const char* name, char* dst, size_t len) {
    int ret = iio_channel_attr_read(chan, name, dst, len);
    details_t t; t.format = NONE; t.proc = READING;
    details_d d;
    errchk(ret, name, t, d);
}
void ch_attr_write_ll(struct iio_channel* chan, const char* name, long long value) {
    int ret = iio_channel_attr_write_longlong(chan, name, value);
    details_t t; t.format = LONGLONG; t.proc = WRITING;
    details_d d; d.ll = value;
    errchk(ret, name, t, d);
}
void ch_attr_read_ll(struct iio_channel* chan, const char* name, long long* value) {
    int ret = iio_channel_attr_read_longlong(chan, name, value);
    details_t t; t.format = NONE; t.proc = READING;
    details_d d;
    errchk(ret, name, t, d);
}

ssize_t push_buffer(struct iio_buffer* buf) {
    ssize_t bytes = iio_buffer_push(buf);
    if (bytes < 0) {
        printf(" (!) Error pushing buffer %d\n", (int) bytes);
        shutdown();
    }
    return bytes;
}
ssize_t refill_buffer(struct iio_buffer* buf) {
    ssize_t bytes = iio_buffer_refill(buf);
    if (bytes < 0) {
        printf(" (!) Error refilling buffer %d\n", (int) bytes);
        shutdown();
    }
    return bytes;
}
void buf_start(struct iio_buffer* buf, struct iio_channel* chan, buffer_t* data) {
    data->ptr_inc = iio_buffer_step(buf);
    data->ptr_dat = (char*) iio_buffer_first(buf, chan);
    data->ptr_end = (char*) iio_buffer_end(buf);
}
bool buf_avail(buffer_t* data) {
    return data->ptr_dat < data->ptr_end;
}
void buf_write(buffer_t* data, int16_t value_i, int16_t value_q) {
    ((int16_t*)(data->ptr_dat))[0] = value_i;
    ((int16_t*)(data->ptr_dat))[1] = value_q;
}
void buf_read(buffer_t* data, int16_t* value_i, int16_t* value_q) {
    *value_i = ((int16_t*)(data->ptr_dat))[0];
    *value_q = ((int16_t*)(data->ptr_dat))[1];
}
void buf_next(buffer_t* data) {
    data->ptr_dat += data->ptr_inc;
}
void buf_copy(buffer_t* dest, buffer_t* src) {
    ((int16_t*)(dest->ptr_dat))[0] = ((int16_t*)(src->ptr_dat))[0];
    ((int16_t*)(dest->ptr_dat))[1] = ((int16_t*)(src->ptr_dat))[1];
    dest->ptr_dat += dest->ptr_inc;
    src->ptr_dat += src->ptr_inc;
}

/********************
 * VALUE PROCEDURES
********************/
void print_value_thousands(long long value) {
    int16_t num[4] = {0}, sign = 1;
    int8_t begin = 0;
    if (value < 0) {
        sign = -1;
        value = -value;
    }
    if (value >= 1000000000ll) {
		num[3] = sign * (int16_t)(value / 1000000000ll);
        value = value % 1000000000ll;
        sign = 1;
        if (begin == 0) begin = 3;
	}
    if (value >= 1000000ll) {
        num[2] = sign * (int16_t)(value / 1000000ll);
        value = value % 1000000ll;
        sign = 1;
        if (begin == 0) begin = 2;
	}
    if (value >= 1000ll) {
		num[1] = sign * (int16_t)(value / 1000ll);
        value = value % 1000ll;
        sign = 1;
        if (begin == 0) begin = 1;
	}
    num[0] = sign * (int16_t)(value);
    printf("%hd ", num[begin]);
    for (int i = begin - 1; i >= 0; i--) {
        printf("%.3hd ", num[i]);
    }
}
void print_value_prefix(long long value) {
	double div = 1;
	char pref = 0;
	if (value >= 1000000000ll) {
		div = 1e9;
		pref = 'G';
	} else if (value >= 1000000ll) {
		div = 1e6;
		pref = 'M';
	} else if (value >= 1000ll) {
		div = 1e3;
		pref = 'k';
	} else {
		printf("%8lld.00 ", value);
		return;
	}
	printf("%8.2f %c", (double)value / div, pref);
}
void print_value_unit(const char* name, long long value, const char* unit) {
    printf(name);
    print_value_prefix(value);
    printf(unit);
}
void amplitude_range(int16_t* value) {
	if (*value > 2047) *value = 2047;
	else if (*value < -2048) *value = -2048;
}

#endif
