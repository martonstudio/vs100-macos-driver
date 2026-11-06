/*
 * vs100_test — standalone exerciser for the VS-100 USB engine.
 * Plays the role of the CoreAudio HAL: paces itself with the engine's zero
 * timestamps, writes a test tone into the output ring ahead of the device
 * clock and reads the input ring behind it, printing clock / feedback stats.
 *
 *   vs100_test [seconds] [--tone freq] [--silent]
 */
#define VS100_LOG_STDERR 1
#include "../driver/VS100_USB.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <mach/mach_time.h>

static double ticks_per_sec(void) { mach_timebase_info_data_t tb; mach_timebase_info(&tb); return 1e9 * tb.denom / tb.numer; }

int main(int argc, char **argv) {
    int seconds = 5; double tone = 440.0; int silent = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--tone") && i + 1 < argc) tone = atof(argv[++i]);
        else if (!strcmp(argv[i], "--silent")) silent = 1;
        else seconds = atoi(argv[i]);
    }
    vs100_engine_t *e = vs100_engine_create();
    if (!e) { fprintf(stderr, "engine create failed\n"); return 1; }
    for (int i = 0; i < 50 && !vs100_engine_device_present(e); i++) usleep(100000);
    if (!vs100_engine_device_present(e)) { fprintf(stderr, "VS-100 not found\n"); vs100_engine_destroy(e); return 1; }
    uint32_t rate = vs100_engine_sample_rate(e);
    printf("device present, rate=%u serial='%s' loc=0x%08x\n", rate, vs100_engine_serial(e), vs100_engine_location_id(e));
    printf("safety in=%u out=%u zts period=%u\n", vs100_engine_safety_offset_input(e), vs100_engine_safety_offset_output(e), vs100_engine_zts_period(e));

    int r = vs100_engine_start(e);
    if (r) { fprintf(stderr, "start failed 0x%x\n", r); vs100_engine_destroy(e); return 1; }

    const uint32_t buf = 512;                     /* pretend IO buffer size */
    const double tps = ticks_per_sec();
    double phase = 0; int64_t nextOut = -1; int64_t nextIn = -1;
    double lastZts = -1; uint64_t lastHost = 0; int ztsCount = 0; double maxJitterUs = 0; double sumRate = 0; int rateN = 0;
    double inPeak = 0, chPeak[VS100_CHANNELS_IN] = {0}; uint32_t lowBytesNonZero = 0; uint64_t t0 = mach_absolute_time();
    uint8_t *obuf = calloc(buf, VS100_BPF_OUT), *ibuf = calloc(buf, VS100_BPF_IN);
    uint32_t sOut = vs100_engine_safety_offset_output(e) + buf, sIn = vs100_engine_safety_offset_input(e) + buf;

    while ((double)(mach_absolute_time() - t0) / tps < seconds) {
        double zs; uint64_t zh, seed;
        if (!vs100_engine_get_zero_timestamp(e, &zs, &zh, &seed)) break;
        if (zs != lastZts) {
            if (lastZts >= 0 && zh > lastHost) {
                double dt = (double)(zh - lastHost) / tps; double dn = zs - lastZts;
                double instRate = dn / dt; sumRate += instRate; rateN++;
                double j = fabs(dt - dn / rate) * 1e6; if (j > maxJitterUs) maxJitterUs = j;
            }
            lastZts = zs; lastHost = zh; ztsCount++;
        }
        /* current device sample time, extrapolated like the HAL does */
        uint64_t now = mach_absolute_time();
        double nowSample = zs + ((double)now - (double)zh) / tps * rate;
        if (nextOut < 0) { nextOut = (int64_t)nowSample + sOut; nextIn = (int64_t)nowSample - sIn - buf; }
        while ((double)nextOut < nowSample + sOut + buf) {
            for (uint32_t k = 0; k < buf; k++) {
                int32_t v = silent ? 0 : (int32_t)(sin(phase) * 0.1 * 8388607.0); phase += 2 * M_PI * tone / rate;
                int32_t *f = (int32_t *)(obuf + k * VS100_BPF_OUT); f[0] = f[1] = v * 256;   /* tone on outputs 1+2, S32_LE */
            }
            vs100_engine_write_output(e, nextOut, buf, obuf); nextOut += buf;
        }
        while ((double)nextIn + buf < nowSample - sIn) {
            vs100_engine_read_input(e, nextIn, buf, ibuf); nextIn += buf;
            for (uint32_t k = 0; k < buf; k++) for (int c = 0; c < VS100_CHANNELS_IN; c++) {
                int32_t raw = ((const int32_t *)(ibuf + k * VS100_BPF_IN))[c]; if (raw & 0xff) lowBytesNonZero++;
                double a = fabs(raw / 2147483648.0); if (a > inPeak) inPeak = a; if (a > chPeak[c]) chPeak[c] = a; }
        }
        usleep(2000);
    }
    vs100_stats_t st; vs100_engine_get_stats(e, &st);
    printf("zts: %d stamps, mean rate from stamps=%.3f Hz, max |dt error|=%.1f us\n", ztsCount, rateN ? sumRate / rateN : 0, maxJitterUs);
    printf("rx frames=%llu pkts=%llu err=%llu | tx frames=%llu pkts=%llu err=%llu | resyncs=%llu | measured capture rate=%.3f | fb err=%d\n",
        (unsigned long long)st.rxFrames, (unsigned long long)st.rxPackets, (unsigned long long)st.rxErrors, (unsigned long long)st.txFrames, (unsigned long long)st.txPackets,
        (unsigned long long)st.txErrors, (unsigned long long)st.resyncs, st.measuredRate, st.feedbackError);
    printf("input peak level: %.4f (%.1f dBFS); samples with non-zero low byte: %u\n", inPeak, inPeak > 0 ? 20 * log10(inPeak) : -200.0, lowBytesNonZero);
    printf("per-input peak dBFS:"); for (int c = 0; c < VS100_CHANNELS_IN; c++) printf(" %d:%.0f", c + 1, chPeak[c] > 0 ? 20 * log10(chPeak[c]) : -200.0); printf("\n");
    printf("rxPerBusFrame=%.4f outRate=%.4f lateFills=%u lateHarvests=%u snaps=%u\n", st.rxPerBusFrame, st.outRate, st.lateFills, st.lateHarvests, st.snaps);
    vs100_engine_stop(e);
    vs100_engine_destroy(e);
    free(obuf); free(ibuf);
    return 0;
}
