/*
 * VS100_USB.h — user-space USB streaming engine for the EDIROL / Roland VS-100
 * (VID 0x0582, PID 0x00A3, "Advanced" driver mode).
 *
 * Runs entirely in user space on top of IOUSBLib (IOKit). No kernel extension,
 * no DriverKit entitlement required. Shared between the CoreAudio HAL plug-in
 * (VS100_Driver.c) and the standalone test tool (tools/vs100_test.c).
 */
#ifndef VS100_USB_H
#define VS100_USB_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VS100_VID           0x0582
#define VS100_PID_ADVANCED  0x00EB     /* Cakewalk/Roland V-Studio 100 (VS-100) */

/* VS-100: 6 out / 10 in, 24-bit audio in 32-bit little-endian slots (S32_LE, 24 valid MSBs), 48 kHz. */
#define VS100_CHANNELS_OUT  6
#define VS100_CHANNELS_IN   10
#define VS100_BYTES_PER_SAMPLE 4
#define VS100_BPF_OUT       (VS100_CHANNELS_OUT * VS100_BYTES_PER_SAMPLE)
#define VS100_BPF_IN        (VS100_CHANNELS_IN  * VS100_BYTES_PER_SAMPLE)
/* High-speed device: every 1 ms bus frame carries 8 microframe packets (endpoint interval 1). */
#define VS100_UFRAMES_PER_MS 8

/* Streaming geometry limits. One USB (full-speed) frame == 1 ms. */
#define VS100_MAX_FRAMES_PER_XFER   8     /* ms of audio per isoch transfer   */
#define VS100_MAX_XFERS_IN_FLIGHT   8     /* transfers queued per direction    */
#define VS100_DEFAULT_FRAMES_PER_XFER 1
#define VS100_DEFAULT_XFERS_IN_FLIGHT 8       /* capture queue depth (schedule headroom only; input latency does not depend on it) */
#define VS100_DEFAULT_XFERS_IN_FLIGHT_OUT 8   /* playback queue depth (ms queued ahead); data is filled late, see VS100_DEFAULT_OUTPUT_LEAD_MS */
#define VS100_DEFAULT_OUTPUT_LEAD_MS      2   /* playback data written into the queued DMA buffer this many ms before it is transmitted */
#define VS100_RING_FRAMES           32768 /* power of two, audio frames        */

typedef struct vs100_engine vs100_engine_t;

/* Called on the engine's USB thread when the device appears / disappears. */
typedef void (*vs100_hotplug_cb)(void *ctx, bool arrived);

/* Runtime-tunable geometry. Applied at the next start. */
typedef struct {
    uint32_t framesPerXfer;    /* 1..VS100_MAX_FRAMES_PER_XFER (ms per transfer) */
    uint32_t xfersInFlight;    /* capture:  2..VS100_MAX_XFERS_IN_FLIGHT         */
    uint32_t xfersInFlightOut; /* playback: 2..VS100_MAX_XFERS_IN_FLIGHT         */
    uint32_t outputLeadMs;     /* playback fill lead: 1..(xfersInFlightOut*framesPerXfer-1) */
} vs100_config_t;

/* Lifecycle */
vs100_engine_t *vs100_engine_create(void);
void            vs100_engine_destroy(vs100_engine_t *e);
void            vs100_engine_set_hotplug_callback(vs100_engine_t *e, vs100_hotplug_cb cb, void *ctx);

/* Configuration (clamped to the limits above) */
void vs100_engine_set_config(vs100_engine_t *e, const vs100_config_t *c);
void vs100_engine_get_config(vs100_engine_t *e, vs100_config_t *c);

/* Device info (valid when present) */
bool     vs100_engine_device_present(vs100_engine_t *e);
uint32_t vs100_engine_sample_rate(vs100_engine_t *e);      /* 44100 / 48000 / 96000, 0 if absent */
uint32_t vs100_engine_location_id(vs100_engine_t *e);
const char *vs100_engine_serial(vs100_engine_t *e);         /* "" if none */

/* IO control (thread-safe, synchronous) */
int  vs100_engine_start(vs100_engine_t *e);   /* 0 on success, else IOReturn */
void vs100_engine_stop(vs100_engine_t *e);
bool vs100_engine_is_running(vs100_engine_t *e);
bool vs100_engine_input_active(vs100_engine_t *e);   /* capture stream actually streaming */
bool vs100_engine_output_active(vs100_engine_t *e);

/* Timing for CoreAudio */
uint32_t vs100_engine_zts_period(vs100_engine_t *e);          /* frames between zero timestamps */
uint32_t vs100_engine_safety_offset_input(vs100_engine_t *e);  /* frames */
uint32_t vs100_engine_safety_offset_output(vs100_engine_t *e); /* frames */
uint32_t vs100_engine_latency_input(vs100_engine_t *e);        /* frames, presentation latency */
uint32_t vs100_engine_latency_output(vs100_engine_t *e);
/* Most recent zero timestamp. Returns false if IO is not running. */
bool vs100_engine_get_zero_timestamp(vs100_engine_t *e, double *sampleTime, uint64_t *hostTime, uint64_t *seed);

/* Ring access — called from the HAL IO thread. Data is S32_LE interleaved: VS100_BPF_IN / VS100_BPF_OUT bytes per frame. */
void vs100_engine_read_input (vs100_engine_t *e, int64_t sampleTime, uint32_t frames, void *dst);
void vs100_engine_write_output(vs100_engine_t *e, int64_t sampleTime, uint32_t frames, const void *src);

/* Diagnostics */
typedef struct {
    bool     running, inputActive, outputActive, captureMaster;
    uint32_t framesPerXfer, xfersInFlight, xfersInFlightOut, outputLeadMs;
    uint64_t rxFrames, txFrames;       /* audio frames */
    uint64_t rxPackets, txPackets;     /* USB packets */
    uint64_t rxErrors, txErrors;       /* frames with frStatus != 0 */
    uint64_t resyncs;                  /* schedule re-anchors after falling behind */
    double   measuredRate;             /* capture frames per second vs. host clock */
    double   rxPerBusFrame;            /* capture frames per USB frame (device vs. SOF clock) */
    double   outRate;                  /* playback frames per USB frame currently used */
    int32_t  feedbackError;            /* tx - rx at the same bus frame, frames */
    uint32_t packetsAdjusted;          /* playback packets that deviated from the nominal size */
    uint32_t snaps;                    /* hard realignments of the playback read pointer */
    double   maxCompletionLatencyUs;   /* decaying max: tick thread wake-up lateness */
    uint32_t lateCompletions;          /* tick wake-ups later than 0.7 ms */
    uint32_t lateFills;                /* playback frames the tick thread could not fill before transmission */
    uint32_t lateHarvests;             /* capture frames only picked up by the completion callback */
    double   harvestLagMaxUs;          /* decaying max: capture frame end -> data in ring */
    uint64_t harvestedByPoll, harvestedByCallback;
    bool     rtPolicyOK;               /* USB thread got the time-constraint policy */
} vs100_stats_t;
void vs100_engine_get_stats(vs100_engine_t *e, vs100_stats_t *out);

#ifdef __cplusplus
}
#endif
#endif
