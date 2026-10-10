/* Encode/decode benchmarks for the SSE binary library, mirroring
 * bench/bench.zig in fin-proto-sse-bin-zig and examples/bench.rs in
 * fin-proto-sse-bin-rs: same methodology (10k warmup iterations, then 10
 * batches of 100k operations, fastest batch reported), so the numbers are
 * directly comparable.
 *
 * Output is one TSV line per case on stdout:
 *   lib \t lang \t op \t ns_per_op \t frame_bytes
 */
/* clock_gettime/CLOCK_MONOTONIC are POSIX; -std=c11 hides them otherwise */
#define _POSIX_C_SOURCE 199309L
#include "root.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define LIB "sse"
#define ITERS 100000
#define BATCHES 10
#define WARMUP 10000

static volatile uint64_t sink;

static double now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

static void report(const char *op, double best_ns, size_t bytes) {
    printf("%s\tc\t%s\t%.1f\t%zu\n", LIB, op, best_ns / (double)ITERS, bytes);
}

static void fill_new_order_single(sse_binary_new_order_single *msg) {
    memset(msg, 0, sizeof(*msg));
    msg->biz_id = 123456;
    msg->biz_pbu = FC_STR("PBU0001");
    msg->cl_ord_id = FC_STR("ORD000001");
    msg->security_id = FC_STR("600000");
    msg->account = FC_STR("ACC000000001");
    msg->owner_type = 1;
    msg->side = FC_STR("1");
    msg->price = 10500;
    msg->order_qty = 100;
    msg->ord_type = FC_STR("2");
    msg->time_in_force = FC_STR("0");
    msg->transact_time = 20260109093000123ULL;
    msg->credit_tag = FC_STR("XY");
    msg->clearing_firm = FC_STR("01");
    msg->branch_id = FC_STR("0001");
    msg->user_info = FC_STR("user01");
}

static size_t bench_encode(void) {
    sse_binary_new_order_single msg;
    fill_new_order_single(&msg);
    fc_buffer buf;
    fc_buffer_init(&buf, 0);
    for (int i = 0; i < WARMUP; i++) {
        fc_buffer_reset(&buf);
        sse_binary_new_order_single_encode(&msg, &buf);
    }
    double best = 1e18;
    for (int b = 0; b < BATCHES; b++) {
        double t0 = now_ns();
        for (int i = 0; i < ITERS; i++) {
            fc_buffer_reset(&buf);
            sse_binary_new_order_single_encode(&msg, &buf);
            sink += buf.len;
        }
        double d = now_ns() - t0;
        if (d < best) best = d;
    }
    size_t bytes = buf.len;
    report("encode", best, bytes);
    fc_buffer_free(&buf);
    return bytes;
}

static void bench_decode(const uint8_t *raw, size_t len) {
    uint64_t s = 0;
    for (int i = 0; i < WARMUP; i++) {
        fc_reader r;
        fc_reader_init(&r, raw, len, 0);
        sse_binary_new_order_single m;
        sse_binary_new_order_single_decode(&r, &m);
        s += m.transact_time;
        sse_binary_new_order_single_free(&m);
    }
    double best = 1e18;
    for (int b = 0; b < BATCHES; b++) {
        double t0 = now_ns();
        for (int i = 0; i < ITERS; i++) {
            fc_reader r;
            fc_reader_init(&r, raw, len, 0);
            sse_binary_new_order_single m;
            sse_binary_new_order_single_decode(&r, &m);
            s += m.transact_time;
            sse_binary_new_order_single_free(&m);
        }
        double d = now_ns() - t0;
        if (d < best) best = d;
    }
    sink += s;
    report("decode", best, len);
}

int main(void) {
    bench_encode();

    /* pre-encode one frame for the decode benchmark */
    sse_binary_new_order_single msg;
    fill_new_order_single(&msg);
    fc_buffer buf;
    fc_buffer_init(&buf, 0);
    if (!sse_binary_new_order_single_encode(&msg, &buf)) return 1;
    bench_decode(buf.data, buf.len);
    fc_buffer_free(&buf);

    if (sink == 1) fprintf(stderr, "\n");
    return 0;
}
