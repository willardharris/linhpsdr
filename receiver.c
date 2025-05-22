/* Copyright (C)
*
* 2018 - John Melton, G0ORX/N6LYT
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
*/

#include <gtk/gtk.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <wdsp.h>

#include "agc.h"
#include "mode.h"
#include "filter.h"
#include "bandstack.h"
#include "band.h"
#include "discovered.h"
#include "bpsk.h"
#include "receiver.h"
#include "transmitter.h"
#include "wideband.h"
#include "adc.h"
#include "dac.h"
#include "radio.h"
#include "main.h"
#include "vfo.h"
#include "meter.h"
#include "radio_info.h"
#include "rx_panadapter.h"
#include "tx_panadapter.h"
#include "waterfall.h"
#include "protocol1.h"
#include "protocol2.h"
#ifdef SOAPYSDR
#include "soapy_protocol.h"
#endif
#include "audio.h"
#include "receiver_dialog.h"
#include "configure_dialog.h"
#include "property.h"
#include "rigctl.h"
#include "subrx.h"

// Metadata for savable fields
typedef enum {
    TYPE_INT,
    TYPE_LONG,
    TYPE_INT64,
    TYPE_DOUBLE,
    TYPE_STRING
} FieldType;

typedef struct {
    const char *name;   // Property name (without receiver[channel] prefix)
    FieldType type;     // Data type
    size_t offset;      // Offset in RECEIVER struct
    int conditional;    // 0 = always save, 1 = conditional (e.g., waterfall_low)
} FieldDescriptor;

#define OFFSET(field) offsetof(RECEIVER, field)

// Metadata table for RECEIVER fields
static const FieldDescriptor receiver_fields[] = {
    {"adc", TYPE_INT, OFFSET(adc), 0},
    {"sample_rate", TYPE_INT, OFFSET(sample_rate), 0},
    {"dsp_rate", TYPE_INT, OFFSET(dsp_rate), 0},
    {"output_rate", TYPE_INT, OFFSET(output_rate), 0},
    {"buffer_size", TYPE_INT, OFFSET(buffer_size), 0},
    {"fft_size", TYPE_INT, OFFSET(fft_size), 0},
    {"low_latency", TYPE_INT, OFFSET(low_latency), 0},
    {"fps", TYPE_INT, OFFSET(fps), 0},
    {"display_average_time", TYPE_DOUBLE, OFFSET(display_average_time), 0},
    {"panadapter_low", TYPE_INT, OFFSET(panadapter_low), 0},
    {"panadapter_high", TYPE_INT, OFFSET(panadapter_high), 0},
    {"panadapter_step", TYPE_INT, OFFSET(panadapter_step), 0},
    {"panadapter_filled", TYPE_INT, OFFSET(panadapter_filled), 0},
    {"panadapter_gradient", TYPE_INT, OFFSET(panadapter_gradient), 0},
    {"panadapter_agc_line", TYPE_INT, OFFSET(panadapter_agc_line), 0},
    {"waterfall_low", TYPE_INT, OFFSET(waterfall_low), 1}, // Conditional
    {"waterfall_high", TYPE_INT, OFFSET(waterfall_high), 1}, // Conditional
    {"waterfall_automatic", TYPE_INT, OFFSET(waterfall_automatic), 0},
    {"waterfall_ft8_marker", TYPE_INT, OFFSET(waterfall_ft8_marker), 0},
    {"frequency_a", TYPE_INT64, OFFSET(frequency_a), 0},
    {"lo_a", TYPE_INT64, OFFSET(lo_a), 0},
    {"error_a", TYPE_INT64, OFFSET(error_a), 0},
    {"lo_tx", TYPE_INT64, OFFSET(lo_tx), 0},
    {"error_tx", TYPE_INT64, OFFSET(error_tx), 0},
    {"tx_track_rx", TYPE_INT, OFFSET(tx_track_rx), 0},
    {"band_a", TYPE_INT, OFFSET(band_a), 0},
    {"mode_a", TYPE_INT, OFFSET(mode_a), 0},
    {"filter_a", TYPE_INT, OFFSET(filter_a), 0},
    {"filter_low_a", TYPE_INT, OFFSET(filter_low_a), 0},
    {"filter_high_a", TYPE_INT, OFFSET(filter_high_a), 0},
    {"frequency_b", TYPE_INT64, OFFSET(frequency_b), 0},
    {"lo_b", TYPE_INT64, OFFSET(lo_b), 0},
    {"error_b", TYPE_INT64, OFFSET(error_b), 0},
#ifdef USE_VFO_B_MODE_AND_FILTER
    {"band_b", TYPE_INT, OFFSET(band_b), 0},
    {"mode_b", TYPE_INT, OFFSET(mode_b), 0},
    {"filter_b", TYPE_INT, OFFSET(filter_b), 0},
    {"filter_low_b", TYPE_INT, OFFSET(filter_low_b), 0},
    {"filter_high_b", TYPE_INT, OFFSET(filter_high_b), 0},
#endif
    {"ctun", TYPE_INT, OFFSET(ctun), 0},
    {"ctun_offset", TYPE_LONG, OFFSET(ctun_offset), 0},
    {"ctun_frequency", TYPE_LONG, OFFSET(ctun_frequency), 0},
    {"ctun_min", TYPE_LONG, OFFSET(ctun_min), 0},
    {"ctun_max", TYPE_LONG, OFFSET(ctun_max), 0},
    {"qo100_beacon", TYPE_INT, OFFSET(qo100_beacon), 0},
    {"split", TYPE_INT, OFFSET(split), 0},
    {"offset", TYPE_LONG, OFFSET(offset), 0},
    {"bandstack", TYPE_INT, OFFSET(bandstack), 0},
    {"remote_audio", TYPE_INT, OFFSET(remote_audio), 0},
    {"local_audio", TYPE_INT, OFFSET(local_audio), 0},
    {"output_index", TYPE_INT, OFFSET(output_index), 0},
    {"mute_when_not_active", TYPE_INT, OFFSET(mute_when_not_active), 0},
    {"local_audio_buffer_size", TYPE_INT, OFFSET(local_audio_buffer_size), 0},
    {"local_audio_latency", TYPE_INT, OFFSET(local_audio_latency), 0},
    {"audio_channels", TYPE_INT, OFFSET(audio_channels), 0},
    {"step", TYPE_LONG, OFFSET(step), 0},
    {"zoom", TYPE_INT, OFFSET(zoom), 0},
    {"pan", TYPE_INT, OFFSET(pan), 0},
    {"agc", TYPE_INT, OFFSET(agc), 0},
    {"agc_gain", TYPE_DOUBLE, OFFSET(agc_gain), 0},
    {"agc_slope", TYPE_DOUBLE, OFFSET(agc_slope), 0},
    {"agc_hang_threshold", TYPE_DOUBLE, OFFSET(agc_hang_threshold), 0},
    {"enable_equalizer", TYPE_INT, OFFSET(enable_equalizer), 0},
    {"volume", TYPE_DOUBLE, OFFSET(volume), 0},
    {"nr", TYPE_INT, OFFSET(nr), 0},
    {"nr2", TYPE_INT, OFFSET(nr2), 0},
    {"nr2_gain_method", TYPE_INT, offsetof(RECEIVER, nr2_gain_method), 0},
    {"nr2_npe_method", TYPE_INT, offsetof(RECEIVER, nr2_npe_method), 0},
    {"nb", TYPE_INT, OFFSET(nb), 0},
    {"nb2", TYPE_INT, OFFSET(nb2), 0},
    {"anf", TYPE_INT, OFFSET(anf), 0},
    {"snb", TYPE_INT, OFFSET(snb), 0},
    {"rit_enabled", TYPE_INT, OFFSET(rit_enabled), 0},
    {"rit", TYPE_LONG, OFFSET(rit), 0},
    {"rit_step", TYPE_LONG, OFFSET(rit_step), 0},
    {"bpsk_enable", TYPE_INT, OFFSET(bpsk_enable), 0},
    {"duplex", TYPE_INT, OFFSET(duplex), 0},
    {"mute_while_transmitting", TYPE_INT, OFFSET(mute_while_transmitting), 0},
    {"rigctl_port", TYPE_INT, OFFSET(rigctl_port), 0},
    {"rigctl_enable", TYPE_INT, OFFSET(rigctl_enable), 0},
    {"rigctl_serial_enable", TYPE_INT, OFFSET(rigctl_serial_enable), 0},
    {"rigctl_debug", TYPE_INT, OFFSET(rigctl_debug), 0},
    {"paned_position", TYPE_INT, OFFSET(paned_position), 0},
    {"paned_percent", TYPE_DOUBLE, OFFSET(paned_percent), 0},
    // New fields from noise_menu.c
    {"nr2_trained_threshold", TYPE_DOUBLE, OFFSET(nr2_trained_threshold), 0},
    {"nr2_trained_t2", TYPE_DOUBLE, OFFSET(nr2_trained_t2), 0},
    {"nb2_mode", TYPE_INT, OFFSET(nb2_mode), 0},
    {"nb_tau", TYPE_DOUBLE, OFFSET(nb_tau), 0},
    {"nb_advtime", TYPE_DOUBLE, OFFSET(nb_advtime), 0},
    {"nb_hang", TYPE_DOUBLE, OFFSET(nb_hang), 0},
    {"nb_thresh", TYPE_DOUBLE, OFFSET(nb_thresh), 0},
    {NULL, 0, 0, 0} // Sentinel
};

// Forward declaration for full_rx_buffer
static void full_rx_buffer(RECEIVER *rx);

static gpointer render_processing_thread(gpointer data);

static gboolean update_display_cb(gpointer data) {
    RECEIVER *rx = (RECEIVER *)data;
    ReceiverThreadContext *ctx = &rx->thread_context;

    g_mutex_lock(&ctx->render_mutex);

    // If widgets are already destroyed, bail early
    if (!GTK_IS_WIDGET(rx->panadapter) || !GTK_IS_WIDGET(rx->waterfall)) {
        g_mutex_unlock(&ctx->render_mutex);
        return FALSE;
    }

    gboolean protocol_running = FALSE;
    switch (radio->discovered->protocol) {
        case PROTOCOL_1:
            protocol_running = protocol1_is_running();
            break;
        case PROTOCOL_2:
            protocol_running = protocol2_is_running();
            break;
#ifdef SOAPYSDR
        case PROTOCOL_SOAPYSDR:
            protocol_running = soapy_protocol_is_running();
            break;
#endif
    }

    update_rx_panadapter(rx, protocol_running);
    update_waterfall(rx);

    gtk_widget_queue_draw(rx->panadapter);
    gtk_widget_queue_draw(rx->waterfall);

    g_mutex_unlock(&ctx->render_mutex);
    return FALSE; // Run once
}


static gpointer render_processing_thread(gpointer data) {
    RECEIVER *rx = (RECEIVER *)data;
    ReceiverThreadContext *ctx = &rx->thread_context;
    while (ctx->running) {
        gpointer queue_data = g_async_queue_pop(ctx->render_queue);
        if (GPOINTER_TO_INT(queue_data) == -1) break;

        g_mutex_lock(&ctx->render_mutex);
        //gboolean protocol_running = FALSE;
        // Preprocess data (e.g., update rx->pixel_samples)
        // Schedule GTK update in main thread
        g_idle_add((GSourceFunc)update_display_cb, rx);
        g_mutex_unlock(&ctx->render_mutex);
    }
    return NULL;
}


static void focus_in_event_cb(GtkWindow *window, GdkEventFocus *event, gpointer data) {
    if (event->in) {
        radio->active_receiver = (RECEIVER *)data;
    }
}

static gpointer wdsp_processing_thread(gpointer data) {
    RECEIVER *rx = (RECEIVER *)data;
    ReceiverThreadContext *ctx = &rx->thread_context;

    fprintf(stderr, "WDSP thread started: channel=%d\n", rx->channel);

    while (ctx->running) {
        gpointer queue_data = g_async_queue_pop(ctx->iq_queue);
        if (GPOINTER_TO_INT(queue_data) == -1) {
            fprintf(stderr, "WDSP thread exiting (channel=%d)\n", rx->channel);
            break;
        }

        int required = rx->buffer_size * 2; // I + Q

        if (rx->iq_ring_buffer.count < required) {

            // Retry later
            g_async_queue_push(ctx->iq_queue, GINT_TO_POINTER(1));
            continue;
        }


        full_rx_buffer(rx);
    }

    fprintf(stderr, "WDSP thread (channel=%d): exiting\n", rx->channel);
    return NULL;
}

void receiver_save_state(RECEIVER *rx) {
    char name[128];
    char value[128];
    int i;
    gint x, y, width, height;

    if (!rx) {
        fprintf(stderr, "receiver_save_state: Error: rx is NULL\n");
        return;
    }
    fprintf(stderr, "receiver_save_state: rx=%p, channel=%d\n", rx, rx->channel);

    // Save channel
    sprintf(name, "receiver[%d].channel", rx->channel);
    sprintf(value, "%d", rx->channel);
    fprintf(stderr, "Saving: %s = %s\n", name, value);
    setProperty(name, value);

    // Generic save loop
    for (const FieldDescriptor *field = receiver_fields; field->name; field++) {
        if (field->conditional) {
            if (strcmp(field->name, "waterfall_low") == 0 || strcmp(field->name, "waterfall_high") == 0) {
                if (rx->waterfall_automatic) continue;
            }
        }

        sprintf(name, "receiver[%d].%s", rx->channel, field->name);
        void *ptr = (char *)rx + field->offset;
        fprintf(stderr, "Field: %s, offset=%zu, ptr=%p\n", field->name, field->offset, ptr);

        switch (field->type) {
            case TYPE_INT:
                fprintf(stderr, "  Value: %d\n", *(int *)ptr);
                sprintf(value, "%d", *(int *)ptr);
                setProperty(name, value);
                break;
            case TYPE_LONG:
                fprintf(stderr, "  Value: %ld\n", *(long *)ptr);
                sprintf(value, "%ld", *(long *)ptr);
                setProperty(name, value);
                break;
            case TYPE_INT64:
                fprintf(stderr, "  Value: %" G_GINT64_FORMAT "\n", *(gint64 *)ptr);
                sprintf(value, "%" G_GINT64_FORMAT, *(gint64 *)ptr);
                setProperty(name, value);
                break;
            case TYPE_DOUBLE:
                fprintf(stderr, "  Value: %f\n", *(double *)ptr);
                sprintf(value, "%f", *(double *)ptr);
                setProperty(name, value);
                break;
            case TYPE_STRING:
                break;
        }
        fprintf(stderr, "Saved: %s = %s\n", name, value);
    }

    // Special case: strings
    fprintf(stderr, "Saving audio_name: rx->audio_name=%p\n", rx->audio_name);
    if (rx->audio_name) {
        sprintf(name, "receiver[%d].audio_name", rx->channel);
        fprintf(stderr, "Saving: %s = %s\n", name, rx->audio_name);
        setProperty(name, rx->audio_name);
    }

    fprintf(stderr, "Saving rigctl_serial_port: rx->rigctl_serial_port=%s\n", rx->rigctl_serial_port);
    sprintf(name, "receiver[%d].rigctl_serial_port", rx->channel);
    setProperty(name, rx->rigctl_serial_port);
    fprintf(stderr, "Saved: %s = %s\n", name, rx->rigctl_serial_port);

    // Special case: equalizer array
    fprintf(stderr, "Saving equalizer array\n");
    for (i = 0; i < 4; i++) {
        sprintf(name, "receiver[%d].equalizer[%d]", rx->channel, i);
        sprintf(value, "%d", rx->equalizer[i]);
        fprintf(stderr, "Saving: %s = %s\n", name, value);
        setProperty(name, value);
    }

    // GTK window properties
    fprintf(stderr, "Saving window properties: rx->window=%p\n", rx->window);
    if (rx->window) {
        gtk_window_get_position(GTK_WINDOW(rx->window), &x, &y);
        sprintf(name, "receiver[%d].x", rx->channel);
        sprintf(value, "%d", x);
        fprintf(stderr, "Saving: %s = %s\n", name, value);
        setProperty(name, value);
        sprintf(name, "receiver[%d].y", rx->channel);
        sprintf(value, "%d", y);
        fprintf(stderr, "Saving: %s = %s\n", name, value);
        setProperty(name, value);

        gtk_window_get_size(GTK_WINDOW(rx->window), &width, &height);
        sprintf(name, "receiver[%d].width", rx->channel);
        sprintf(value, "%d", width);
        fprintf(stderr, "Saving: %s = %s\n", name, value);
        setProperty(name, value);
        sprintf(name, "receiver[%d].height", rx->channel);
        sprintf(value, "%d", height);
        fprintf(stderr, "Saving: %s = %s\n", name, value);
        setProperty(name, value);
    }

    fprintf(stderr, "Saving paned properties: rx->vpaned=%p\n", rx->vpaned);
    if (rx->vpaned) {
        fprintf(stderr, "receiver_save_state: paned_position=%d paned_height=%d paned_percent=%f\n",
                rx->paned_position, gtk_widget_get_allocated_height(rx->vpaned), rx->paned_percent);
    } else {
        fprintf(stderr, "receiver_save_state: paned_position=%d paned_percent=%f\n",
                rx->paned_position, rx->paned_percent);
    }
}

void receiver_restore_state(RECEIVER *rx) {
    char name[80];
    char *value;

    // Generic restore loop
    for (const FieldDescriptor *field = receiver_fields; field->name; field++) {
        sprintf(name, "receiver[%d].%s", rx->channel, field->name);
        value = getProperty(name);
        if (!value) continue;

        void *ptr = (char *)rx + field->offset;
        switch (field->type) {
            case TYPE_INT:
                *(int *)ptr = atoi(value);
                break;
            case TYPE_LONG:
                *(long *)ptr = atol(value);
                break;
            case TYPE_INT64:
                *(gint64 *)ptr = atoll(value);
                break;
            case TYPE_DOUBLE:
                *(double *)ptr = atof(value);
                break;
            case TYPE_STRING:
                // Handled separately
                break;
        }
    }

    // Special case: strings
    sprintf(name, "receiver[%d].audio_name", rx->channel);
    value = getProperty(name);
    if (value) {
        rx->audio_name = g_new0(gchar, strlen(value) + 1);
        strcpy(rx->audio_name, value);
    }

    sprintf(name, "receiver[%d].rigctl_serial_port", rx->channel);
    value = getProperty(name);
    if (value) {
        strncpy(rx->rigctl_serial_port, value, sizeof(rx->rigctl_serial_port) - 1);
        rx->rigctl_serial_port[sizeof(rx->rigctl_serial_port) - 1] = '\0';
    }

    // Special case: equalizer array
    for (int i = 0; i < 4; i++) {
        sprintf(name, "receiver[%d].equalizer[%d]", rx->channel, i);
        value = getProperty(name);
        if (value) rx->equalizer[i] = atoi(value);
    }
}

void receiver_xvtr_changed(RECEIVER *rx) {
}

void receiver_change_sample_rate(RECEIVER *rx,int sample_rate) {
g_print("receiver_change_sample_rate: from %d to %d radio=%d\n",rx->sample_rate,sample_rate,radio->sample_rate);
#ifdef SOAPYSDR
  if(radio->discovered->protocol==PROTOCOL_SOAPYSDR) {
    soapy_protocol_stop();
  }
#endif
  g_mutex_lock(&rx->mutex);
  g_mutex_lock(&rx->iq_ring_buffer.mutex);
    SetChannelState(rx->channel, 0, 1);
    g_free(rx->audio_output_buffer);
    g_free(rx->iq_ring_buffer.buffer);
    
    rx->sample_rate = sample_rate;
    rx->output_samples = rx->buffer_size / (rx->sample_rate / 48000);
    rx->audio_output_buffer = g_new0(gdouble, 2 * rx->output_samples);
    rx->iq_ring_buffer.size = rx->buffer_size * 16; // 16x for I and Q
    rx->iq_ring_buffer.buffer = g_new0(gdouble, rx->iq_ring_buffer.size);
    rx->iq_ring_buffer.head = 0;
    rx->iq_ring_buffer.tail = 0;
    rx->iq_ring_buffer.count = 0;
  rx->audio_output_buffer=NULL;
  rx->sample_rate=sample_rate;
  rx->output_samples=rx->buffer_size/(rx->sample_rate/48000);
  rx->audio_output_buffer=g_new0(gdouble,2*rx->output_samples);
  rx->hz_per_pixel=(double)rx->sample_rate/(double)rx->samples;
  SetAllRates(rx->channel,rx->sample_rate,48000,48000);

  receiver_init_analyzer(rx);
  SetEXTANBSamplerate (rx->channel, sample_rate);
  SetEXTNOBSamplerate (rx->channel, sample_rate);
fprintf(stderr,"receiver_change_sample_rate: channel=%d rate=%d buffer_size=%d output_samples=%d\n",rx->channel, rx->sample_rate, rx->buffer_size, rx->output_samples);

#ifdef SOAPYSDR
  if(radio->discovered->protocol==PROTOCOL_SOAPYSDR) {
    soapy_protocol_change_sample_rate(rx,sample_rate);
    soapy_protocol_start_receiver(rx);
  }
#endif

  SetChannelState(rx->channel,1,0);
  g_mutex_unlock(&rx->mutex);
  receiver_update_title(rx);
}

void update_noise(RECEIVER *rx) {
  SetEXTANBRun(rx->channel, rx->nb);
  SetEXTNOBRun(rx->channel, rx->nb2);
  SetRXAANRRun(rx->channel, rx->nr);
  SetRXAEMNRRun(rx->channel, rx->nr2);
  SetRXAANFRun(rx->channel, rx->anf);
  SetRXASNBARun(rx->channel, rx->snb);
  update_vfo(rx);
}

static gboolean destroy_widgets_cb(gpointer data) {
    RECEIVER *rx = (RECEIVER *)data;
    if (radio->dialog != NULL) {
        gtk_widget_destroy(radio->dialog);
        radio->dialog = NULL;
    }
    if (rx->bookmark_dialog != NULL) {
        gtk_widget_destroy(rx->bookmark_dialog);
        rx->bookmark_dialog = NULL;
    }
    return FALSE; // Run once
}

// Helper function for thread join with timeout
gboolean g_thread_join_timeout(GThread *thread, gdouble timeout_seconds) {
    GTimer *timer = g_timer_new();
    while (g_timer_elapsed(timer, NULL) < timeout_seconds) {
        if (g_thread_join(thread)) {
            g_timer_destroy(timer);
            return TRUE;
        }
        g_usleep(10000); // Sleep 10ms
    }
    g_timer_destroy(timer);
    return FALSE;
}

static gboolean delete_receiver_idle_cb(gpointer data) {
    DeleteReceiverData *dr = (DeleteReceiverData *)data;
    RECEIVER *rx = dr->rx;

    delete_receiver(rx);  // Your existing cleanup
    g_free(dr);           // Free the wrapper

    return FALSE;         // Run once
}

static gboolean window_delete(GtkWidget *widget, GdkEvent *event, gpointer data) {
    RECEIVER *rx = (RECEIVER *)data;
    ReceiverThreadContext *ctx = &rx->thread_context;

    // Stop the update timer
    g_source_remove(rx->update_timer_id);
    rx->update_timer_id = 0;

    // Signal threads to exit
    ctx->running = FALSE;
    if (ctx->iq_queue && g_async_queue_length(ctx->iq_queue) < 3) {
    g_async_queue_push(ctx->iq_queue, GINT_TO_POINTER(1));
}

    if (ctx->render_queue && g_async_queue_length(ctx->render_queue) < 3) {
    g_async_queue_push(ctx->render_queue, GINT_TO_POINTER(1));
}
    // Free ring buffer
    g_mutex_lock(&rx->iq_ring_buffer.mutex);
    g_free(rx->iq_ring_buffer.buffer);
    rx->iq_ring_buffer.buffer = NULL;
    g_mutex_unlock(&rx->iq_ring_buffer.mutex);
    g_mutex_clear(&rx->iq_ring_buffer.mutex);

    // Join threads with a timeout
    GTimer *timer = g_timer_new();
    if (ctx->wdsp_thread) {
        if (!g_thread_join_timeout(ctx->wdsp_thread, 1.0)) { // 1-second timeout
            fprintf(stderr, "WDSP thread (channel=%d) failed to join\n", rx->channel);
        }
        ctx->wdsp_thread = NULL;
    }
    if (ctx->render_thread) {
        if (!g_thread_join_timeout(ctx->render_thread, 1.0)) {
            fprintf(stderr, "Render thread (channel=%d) failed to join\n", rx->channel);
        }
        ctx->render_thread = NULL;
    }
    g_timer_destroy(timer);

    // Free queues
    g_async_queue_unref(ctx->iq_queue);
    ctx->iq_queue = NULL;
    if (ctx->render_queue) {
        g_async_queue_unref(ctx->render_queue);
        ctx->render_queue = NULL;
    }

    // Clean up mutex
    g_mutex_clear(&ctx->render_mutex);

    // Defer widget destruction and receiver deletion
    g_idle_add(destroy_widgets_cb, rx);
    DeleteReceiverData *dr = g_new0(DeleteReceiverData, 1);
dr->rx = rx;
g_idle_add(delete_receiver_idle_cb, dr);
    return FALSE;
}

gboolean receiver_button_press_event_cb(GtkWidget *widget, GdkEventButton *event, gpointer data) {
  RECEIVER *rx=(RECEIVER *)data;
  switch(event->button) {
    case 1: // left button
      //if(!rx->locked) {
        rx->last_x=(int)event->x;
        rx->has_moved=FALSE;
        if(rx->zoom>1 && event->y>=rx->panadapter_height-20) {
          rx->is_panning=TRUE;
        }
      //}
      break;
    case 3: // right button
      if(radio->dialog==NULL) {
        int i;
        for(i=0;i<radio->discovered->supported_receivers;i++) {
          if(rx==radio->receiver[i]) {
            break;
          }
        }
        radio->dialog=create_configure_dialog(radio,i+rx_base);
        update_receiver_dialog(rx);
      }
      break;
  }
  return TRUE;
}

void update_frequency(RECEIVER *rx) {
  if(!rx->locked) {
    if(rx->vfo!=NULL) {
      update_vfo(rx);
    }
    if(radio->transmitter!=NULL) {
      if(radio->transmitter->rx==rx) {
        update_tx_panadapter(radio);
      }
    }
  }
}

long long receiver_move_a(RECEIVER *rx,long long hz,gboolean round) {
  long long delta=0LL;
  if(!rx->locked) {
    if(rx->ctun) {
      delta=rx->ctun_frequency;
      rx->ctun_frequency=rx->ctun_frequency+hz;
      if(round && (rx->mode_a!=CWL || rx->mode_a!=CWU)) {
        rx->ctun_frequency=(rx->ctun_frequency/rx->step)*rx->step;
      }
      delta=rx->ctun_frequency-delta;
    } else {
      delta=rx->frequency_a;
      rx->frequency_a=rx->frequency_a-hz; // DEBUG was +
      if(round && (rx->mode_a!=CWL || rx->mode_a!=CWU)) {
        rx->frequency_a=(rx->frequency_a/rx->step)*rx->step;
      }
      delta=rx->frequency_a-delta;
    }
  }
  return delta;
}

void receiver_move_b(RECEIVER *rx,long long hz,gboolean b_only,gboolean round) {
  if(!rx->locked) {
    long long f=rx->frequency_b;
    switch(rx->split) {
      case SPLIT_OFF:
        if(round) {
          rx->frequency_b=(rx->frequency_b+hz)/rx->step*rx->step;
        } else {
          rx->frequency_b=rx->frequency_b+hz;
        }
        update_frequency(rx);
        break;
      case SPLIT_ON:
        if(round) {
          rx->frequency_b=(rx->frequency_b+hz)/rx->step*rx->step;
        } else {
          rx->frequency_b=rx->frequency_b+hz;
        }
        update_frequency(rx);
        break;
      case SPLIT_SAT:
        if(round) {
          rx->frequency_b=(rx->frequency_b+hz)/rx->step*rx->step;
        } else {
          rx->frequency_b=rx->frequency_b+hz;
        }
	if(rx->subrx!=NULL) {
  	  rx->ctun_min=rx->frequency_a-(rx->sample_rate/2);
          rx->ctun_max=rx->frequency_a+(rx->sample_rate/2);
          if(rx->frequency_b<rx->ctun_min || rx->frequency_b>rx->ctun_max) {
            rx->frequency_b=f;
          }
	}
        if(!b_only) {
          receiver_move_a(rx,hz,round);
          frequency_changed(rx);
        }
        update_frequency(rx);
        break;
      case SPLIT_RSAT:
        if(round) {
          rx->frequency_b=(rx->frequency_b-hz)/rx->step*rx->step;
        } else {
          rx->frequency_b=rx->frequency_b-hz;
        }
	if(rx->subrx!=NULL) {
  	  rx->ctun_min=rx->frequency_a-(rx->sample_rate/2);
          rx->ctun_max=rx->frequency_a+(rx->sample_rate/2);
          if(rx->frequency_b<rx->ctun_min || rx->frequency_b>rx->ctun_max) {
            rx->frequency_b=f;
          }
	}
        if(!b_only) {
          receiver_move_a(rx,-hz,round);
          frequency_changed(rx);
          update_frequency(rx);
        }
        break;
    }
  
    if(rx->subrx_enable) {
      // dont allow frequency outside of passband
    }


  }
}

void receiver_move(RECEIVER *rx,long long hz,gboolean round) {
  if(!rx->locked) {
    long long delta=receiver_move_a(rx,hz,round);
    switch(rx->split) {
      case SPLIT_OFF:
        break;
      case SPLIT_ON:
        break;
      case SPLIT_SAT:
      case SPLIT_RSAT:
        receiver_move_b(rx,delta,TRUE,round);
        break;
    }

    frequency_changed(rx);
    update_frequency(rx);
  }
}

void receiver_move_to(RECEIVER *rx,long long hz) {
  long long delta;
  long long start=(long long)rx->frequency_a-(long long)(rx->sample_rate/2);
  long long offset=hz;
  long long f;

  if(!rx->locked) {
    offset=hz;
  
    f=start+offset+(long long)((double)rx->pan*rx->hz_per_pixel);
    f=f/rx->step*rx->step;
    if(rx->ctun) {
      delta=rx->ctun_frequency;
      rx->ctun_frequency=f;
      delta=rx->ctun_frequency-delta;
    } else {
      if(rx->split==SPLIT_ON && (rx->mode_a==CWL || rx->mode_a==CWU)) {
        if(rx->mode_a==CWU) {
          f=f-radio->cw_keyer_sidetone_frequency;
        } else {
          f=f+radio->cw_keyer_sidetone_frequency;
        }
        rx->frequency_b=f;
      } else {
        delta=rx->frequency_a;
        rx->frequency_a=f;
        delta=rx->frequency_a-delta;
      }
    }
  
    switch(rx->split) {
      case SPLIT_OFF:
        break;
      case SPLIT_ON:
        break;
      case SPLIT_SAT:
        receiver_move_b(rx,delta,TRUE,TRUE);
        break;
      case SPLIT_RSAT:
        receiver_move_b(rx,-delta,TRUE,TRUE);
        break;
    }

    frequency_changed(rx);
    update_frequency(rx);
  }
}

gboolean receiver_button_release_event_cb(GtkWidget *widget, GdkEventButton *event, gpointer data) {
  //gint64 hz;
  RECEIVER *rx=(RECEIVER *)data;
  int x;
  //int y;
  //GdkModifierType state;
  x=event->x;
  //y=event->y;
  //state=event->state;
  int moved=x-rx->last_x;
  switch(event->button) {
    case 1: // left button
      if(rx->is_panning) {
        int pan=rx->pan+(moved*rx->zoom);
        if(pan<0) {
          pan=0;
        } else if(pan>(rx->pixels-rx->panadapter_width)) {
          pan=rx->pixels-rx->panadapter_width;
        }
        rx->pan=pan;
        rx->last_x=x;
        rx->has_moved=FALSE;
        rx->is_panning=FALSE;
      } else if(!rx->locked) {
        if(rx->has_moved) {
          // drag
          receiver_move(rx,(long long)((double)(moved*rx->hz_per_pixel)),TRUE);
        } else {
          // move to this frequency
          receiver_move_to(rx,(long long)((float)x*rx->hz_per_pixel));
        }
        rx->last_x=x;
        rx->has_moved=FALSE;
    }
  }

  return TRUE;
}

gboolean receiver_motion_notify_event_cb(GtkWidget *widget, GdkEventMotion *event, gpointer data) {
  GdkModifierType state;
  RECEIVER *rx=(RECEIVER *)data;
  //long long delta;

  state=event->state;
  int moved=event->x-rx->last_x;
  if(rx->is_panning) {
    int pan=rx->pan+(moved*rx->zoom);
    if(pan<0) {
      pan=0;
    } else if(pan>(rx->pixels-rx->panadapter_width)) {
      pan=rx->pixels-rx->panadapter_width;
    }
    rx->pan=pan;
    rx->last_x=event->x;
  } else if(!rx->locked) {
    if((state & GDK_BUTTON1_MASK) == GDK_BUTTON1_MASK) {
      //receiver_move(rx,(long long)((double)(moved*rx->hz_per_pixel)),FALSE);
      receiver_move(rx,(long long)((double)(moved*rx->hz_per_pixel)),TRUE);
      rx->last_x=event->x;
      rx->has_moved=TRUE;
    } else {
      if(event->x<35 && widget==rx->panadapter) {
        gdk_window_set_cursor(gtk_widget_get_window(widget),gdk_cursor_new_from_name(gdk_display_get_default(),"v_double_arrow"));
      } else if(rx->zoom>1 && event->y>=rx->panadapter_height-20) {
	gdk_window_set_cursor(gtk_widget_get_window(widget),gdk_cursor_new_from_name(gdk_display_get_default(),"h_double_arrow"));
      } else if(event->x<rx->panadapter_width/2) {
        gdk_window_set_cursor(gtk_widget_get_window(widget),gdk_cursor_new_from_name(gdk_display_get_default(),"left_side"));
      } else {
        gdk_window_set_cursor(gtk_widget_get_window(widget),gdk_cursor_new_from_name(gdk_display_get_default(),"right_side"));
      }
    }
  }
  return TRUE;
}

gboolean receiver_scroll_event_cb(GtkWidget *widget, GdkEventScroll *event, gpointer data) {
  RECEIVER *rx=(RECEIVER *)data;
  int half=rx->panadapter_height/2;

  if(rx->zoom>1 && event->y>=rx->panadapter_height-20) {
    int pan;
    if(event->direction==GDK_SCROLL_UP) {
      pan=rx->pan+rx->zoom;
    } else {
      pan=rx->pan-rx->zoom;
    }

    if(pan<0) {
      pan=0;
    } else if(pan>(rx->pixels-rx->panadapter_width)) {
      pan=rx->pixels-rx->panadapter_width;
    }
    rx->pan=pan;
  } else if(!rx->locked) {
    if((event->x<35) && (widget==rx->panadapter)) {
      if(event->direction==GDK_SCROLL_UP) {
        if(event->y<half) {
          rx->panadapter_high=rx->panadapter_high-5;
        } else {
          rx->panadapter_low=rx->panadapter_low-5;
        }
      } else {
        if(event->y<half) {
          rx->panadapter_high=rx->panadapter_high+5;
        } else {
          rx->panadapter_low=rx->panadapter_low+5;
        }
      }
    } else if(event->direction==GDK_SCROLL_UP) {
      if(rx->ctun) {
        receiver_move(rx,rx->step,TRUE);
      } else {
        receiver_move(rx,-rx->step,TRUE);
      }
    } else {
      if(rx->ctun) {
        receiver_move(rx,-rx->step,TRUE);
      } else {
        receiver_move(rx,+rx->step,TRUE);
      }
    }
  }
  return TRUE;
}
        
static gboolean update_timer_cb(void *data) {
    int rc;
    RECEIVER *rx = (RECEIVER *)data;
    ReceiverThreadContext *ctx = &rx->thread_context;

    g_mutex_lock(&rx->mutex);
    if (!isTransmitting(radio) || (rx->duplex)) {
        if (rx->panadapter_resize_timer == -1 && rx->pixel_samples != NULL) {
            GetPixels(rx->channel, 0, rx->pixel_samples, &rc);
            if (rc && g_async_queue_length(ctx->render_queue) < 3) { // Allow up to 3 tasks
                if (ctx->render_queue && g_async_queue_length(ctx->render_queue) < 3) {
    g_async_queue_push(ctx->render_queue, GINT_TO_POINTER(1));
}
            }
        }
        rx->meter_db = GetRXAMeter(rx->channel, rx->smeter) + radio->meter_calibration;
        update_meter(rx);
    }
    update_radio_info(rx);

#ifdef SOAPYSDR
    if (radio->discovered->protocol == PROTOCOL_SOAPYSDR) {
        if (radio->transmitter != NULL && radio->discovered->info.soapy.has_temp) {
            radio->transmitter->updated = FALSE;
        }
    }
#endif

    if (radio->transmitter != NULL && !radio->transmitter->updated) {
        update_tx_panadapter(radio);
    }

    g_mutex_unlock(&rx->mutex);
    return TRUE;
}
 
static void set_mode(RECEIVER *rx,int m) {
  //int previous_mode;
  //previous_mode=rx->mode_a;
  rx->mode_a=m;
  SetRXAMode(rx->channel, m);
}

void set_filter(RECEIVER *rx,int low,int high) {
//fprintf(stderr,"set_filter: %d %d\n",low,high);
  if(rx->mode_a==CWL) {
    rx->filter_low_a=-radio->cw_keyer_sidetone_frequency-low;
    rx->filter_high_a=-radio->cw_keyer_sidetone_frequency+high;
  } else if(rx->mode_a==CWU) {
    rx->filter_low_a=radio->cw_keyer_sidetone_frequency-low;
    rx->filter_high_a=radio->cw_keyer_sidetone_frequency+high;
  } else {
    rx->filter_low_a=low;
    rx->filter_high_a=high;
  }
  RXASetPassband(rx->channel,(double)rx->filter_low_a,(double)rx->filter_high_a);
  if(radio->transmitter!=NULL && radio->transmitter->rx==rx && radio->transmitter->use_rx_filter) {
    transmitter_set_filter(radio->transmitter,rx->filter_low_a,rx->filter_high_a);
    update_tx_panadapter(radio);
  }
}

void set_deviation(RECEIVER *rx) {
fprintf(stderr,"set_deviation: %d\n",rx->deviation);
  SetRXAFMDeviation(rx->channel, (double)rx->deviation);
}

void calculate_display_average(RECEIVER *rx) {
  double display_avb;
  int display_average;

  double t=0.001 * rx->display_average_time;
  
  display_avb = exp(-1.0 / ((double)rx->fps * t));
  display_average = max(2, (int)fmin(60, (double)rx->fps * t));
  SetDisplayAvBackmult(rx->channel, 0, display_avb);
  SetDisplayNumAverage(rx->channel, 0, display_average);
}

void receiver_fps_changed(RECEIVER *rx) {
  g_source_remove(rx->update_timer_id);
  rx->update_timer_id=g_timeout_add(1000/rx->fps,update_timer_cb,(gpointer)rx);
  calculate_display_average(rx);
}

void receiver_filter_changed(RECEIVER *rx,int filter) {
//fprintf(stderr,"receiver_filter_changed: %d\n",filter);
  rx->filter_a=filter;
  if(rx->mode_a==FMN) {
    switch(rx->deviation) {
      case 2500:
        set_filter(rx,-4000,4000);
        break;
      case 5000:
        set_filter(rx,-8000,8000);
        break;
    }
    set_deviation(rx);
  } else {
    FILTER *mode_filters=filters[rx->mode_a];
    FILTER *f=&mode_filters[rx->filter_a];
    set_filter(rx,f->low,f->high);
  }
  update_vfo(rx);
}

void receiver_mode_changed(RECEIVER *rx,int mode) {
  set_mode(rx,mode);
  receiver_filter_changed(rx,rx->filter_a);
}

void receiver_band_changed(RECEIVER *rx,int band) {
#ifdef OLD_BAND
  BANDSTACK *bandstack=bandstack_get_bandstack(band);
  if(rx->band_a==band) {
    // same band selected - step to the next band stack
    rx->bandstack++;
    if(rx->bandstack>=bandstack->entries) {
      rx->bandstack=0;
    }
  } else {
    // new band - get band stack entry
    rx->band_a=band;
    rx->bandstack=bandstack->current_entry;
  }

  BANDSTACK_ENTRY *entry=&bandstack->entry[rx->bandstack];
  rx->frequency_a=entry->frequency;
  receiver_mode_changed(rx,entry->mode);
  receiver_filter_changed(rx,entry->filter);
#else
  rx->band_a=band;
  receiver_mode_changed(rx,rx->mode_a);
  receiver_filter_changed(rx,rx->filter_a);
#endif
  frequency_changed(rx);
}

static void process_rx_buffer(RECEIVER *rx) {
    gdouble left_sample = 0.0, right_sample = 0.0;
    short left_audio_sample, right_audio_sample;
    SUBRX *subrx = (SUBRX *)rx->subrx;

    for (int i = 0; i < rx->output_samples; i++) {
        if (rx->subrx_enable) {
            left_sample = rx->audio_output_buffer[i*2];
            right_sample = subrx->audio_output_buffer[i*2];
        } else {
            switch (rx->audio_channels) {
                case AUDIO_STEREO:
                    left_sample = rx->audio_output_buffer[i*2];
                    right_sample = rx->audio_output_buffer[(i*2)+1];
                    break;
                case AUDIO_LEFT_ONLY:
                    left_sample = rx->audio_output_buffer[i*2];
                    right_sample = 0;
                    break;
                case AUDIO_RIGHT_ONLY:
                    left_sample = 0;
                    right_sample = rx->audio_output_buffer[(i*2)+1];
                    break;
            }
        }
        left_sample = fmax(-1.0, fmin(1.0, left_sample));
        right_sample = fmax(-1.0, fmin(1.0, right_sample));
        left_audio_sample = (short)(left_sample * 32767.0);
        right_audio_sample = (short)(right_sample * 32767.0);

        if (rx->local_audio) {
            audio_write(rx, (float)left_sample, (float)right_sample);
        }

        if (radio->active_receiver == rx) {
            if ((isTransmitting(radio) || rx->remote_audio == FALSE) && (!rx->duplex)) {
                left_audio_sample = 0;
                right_audio_sample = 0;
            }
            switch (radio->discovered->protocol) {
                case PROTOCOL_1:
                    if (radio->discovered->device != DEVICE_HERMES_LITE2) {
                        protocol1_audio_samples(rx, left_audio_sample, right_audio_sample);
                    }
                    break;
                case PROTOCOL_2:
                    protocol2_audio_samples(rx, left_audio_sample, right_audio_sample);
                    break;
            }
        }
    }
    if (rx->local_audio && !rx->output_started) {
        audio_start_output(rx);
    }
}
static void full_rx_buffer(RECEIVER *rx) {
    int error;
    RingBuffer *rb = &rx->iq_ring_buffer;

    if (isTransmitting(radio) && (!rx->duplex)) return;

    g_mutex_lock(&rb->mutex);
    int min_samples = rx->buffer_size; // 2048 samples
    if (rb->count < min_samples) {
        fprintf(stderr, "full_rx_buffer: underflow, channel=%d, count=%d\n", rx->channel, rb->count);
        g_mutex_unlock(&rb->mutex);
        return;
    }

    gdouble *temp_buffer = g_new0(gdouble, rx->buffer_size * 2);
    for (int i = 0; i < rx->buffer_size * 2; i++) {
        temp_buffer[i] = rb->buffer[rb->tail];
        rb->tail = (rb->tail + 1) % rb->size;
    }
    rb->count -= rx->buffer_size * 2;

    g_mutex_unlock(&rb->mutex);

    if (rx->nb) {
        xanbEXT(rx->channel, temp_buffer, temp_buffer);
    }
    if (rx->nb2) {
        xnobEXT(rx->channel, temp_buffer, temp_buffer);
    }

    g_mutex_lock(&rx->mutex);
    fexchange0(rx->channel, temp_buffer, rx->audio_output_buffer, &error);
    if (error != 0) {
        fprintf(stderr, "full_rx_buffer: channel=%d samples=%d fexchange0: error=%d\n",
                rx->channel, rx->buffer_size, error);
        if (error == -2) {
            memset(rx->audio_output_buffer, 0, 2 * rx->output_samples * sizeof(gdouble));
        }
    }

    if (rx->subrx_enable) {
        subrx_iq_buffer(rx);
    }

    Spectrum0(1, rx->channel, 0, 0, temp_buffer);
    g_free(temp_buffer);
    process_rx_buffer(rx);
    g_mutex_unlock(&rx->mutex);
}


void add_iq_samples(RECEIVER *rx, double i_sample, double q_sample) {
    ReceiverThreadContext *ctx = &rx->thread_context;
    RingBuffer *rb = &rx->iq_ring_buffer;
    gboolean push_queue = FALSE;

    g_mutex_lock(&rb->mutex);
    if (rb->count >= rb->size - 2) {
        fprintf(stderr, "add_iq_samples: ring buffer full, channel=%d, count=%d\n", rx->channel, rb->count);
    } else {
        rb->buffer[rb->head] = i_sample;
        rb->buffer[rb->head + 1] = q_sample;
        rb->head = (rb->head + 2) % rb->size;
        rb->count += 2;
        if (rb->count >= rx->buffer_size * 2 && ctx->iq_queue && g_async_queue_length(ctx->iq_queue) < 3) {
            push_queue = TRUE;
        }
    }
    g_mutex_unlock(&rb->mutex);

    if (push_queue) {
        g_async_queue_push(ctx->iq_queue, GINT_TO_POINTER(1));
    }

    if (rx->bpsk_enable && rx->bpsk != NULL) {
        bpsk_add_iq_samples(rx->bpsk, i_sample, q_sample);
    }
}

// Forward declaration
static gboolean resize_timeout_cb(gpointer data);

static gboolean receiver_configure_event_cb(GtkWidget *widget, GdkEventConfigure *event, gpointer data) {
    RECEIVER *rx = (RECEIVER *)data;
    //ReceiverThreadContext *ctx = &rx->thread_context;
    gint width = gtk_widget_get_allocated_width(widget);
    gint height = gtk_widget_get_allocated_height(widget);
    if (width != rx->window_width || height != rx->window_height) {
        rx->window_width = width;
        rx->window_height = height;
        if (rx->panadapter_resize_timer == -1) {
            rx->panadapter_resize_timer = g_timeout_add(200, (GSourceFunc)resize_timeout_cb, rx);
            fprintf(stderr, "Resize event (channel=%d): scheduled width=%d height=%d\n",
                    rx->channel, width, height);
        }
    }
    return FALSE;
}

static gboolean resize_timeout_cb(gpointer data) {
    RECEIVER *rx = (RECEIVER *)data;
    ReceiverThreadContext *ctx = &rx->thread_context;
    g_mutex_lock(&ctx->render_mutex);
    receiver_init_analyzer(rx);
    g_mutex_unlock(&ctx->render_mutex);
    rx->panadapter_resize_timer = -1;
    fprintf(stderr, "Resize (channel=%d): pixels=%d\n", rx->channel, rx->pixels);
    return FALSE;
}
void set_agc(RECEIVER *rx) {

  SetRXAAGCMode(rx->channel, rx->agc);
  SetRXAAGCSlope(rx->channel,rx->agc_slope);
  SetRXAAGCTop(rx->channel,rx->agc_gain);
  switch(rx->agc) {
    case AGC_OFF:
      break;
    case AGC_LONG:
      SetRXAAGCAttack(rx->channel,2);
      SetRXAAGCHang(rx->channel,2000);
      SetRXAAGCDecay(rx->channel,2000);
      SetRXAAGCHangThreshold(rx->channel,(int)rx->agc_hang_threshold);
      break;
    case AGC_SLOW:
      SetRXAAGCAttack(rx->channel,2);
      SetRXAAGCHang(rx->channel,1000);
      SetRXAAGCDecay(rx->channel,500);
      SetRXAAGCHangThreshold(rx->channel,(int)rx->agc_hang_threshold);
      break;
    case AGC_MEDIUM:
      SetRXAAGCAttack(rx->channel,2);
      SetRXAAGCHang(rx->channel,0);
      SetRXAAGCDecay(rx->channel,250);
      SetRXAAGCHangThreshold(rx->channel,100);
      break;
    case AGC_FAST:
      SetRXAAGCAttack(rx->channel,2);
      SetRXAAGCHang(rx->channel,0);
      SetRXAAGCDecay(rx->channel,50);
      SetRXAAGCHangThreshold(rx->channel,100);
      break;
  }
}

void receiver_update_title(RECEIVER *rx) {
  gchar title[128];
  g_snprintf((gchar *)&title,sizeof(title),"LinHPSDR: %s Rx-%d ADC-%d %d",radio->discovered->name,rx->channel,rx->adc,rx->sample_rate);
g_print("receiver_update_title: %s\n",title);
  gtk_window_set_title(GTK_WINDOW(rx->window),title);
}

static gboolean enter (GtkWidget *widget, GdkEventCrossing *event, void *user_data) {
  RECEIVER *rx=(RECEIVER *)user_data;
  if(event->x<35 && widget==rx->panadapter) {
    gdk_window_set_cursor(gtk_widget_get_window(widget),gdk_cursor_new_from_name(gdk_display_get_default(),"v_double_arrow"));
  } else if(rx->zoom>1 && event->y>=rx->panadapter_height-20) {
    gdk_window_set_cursor(gtk_widget_get_window(widget),gdk_cursor_new_from_name(gdk_display_get_default(),"h_double_arrow"));
  } else if(event->x<rx->panadapter_width/2) {
    gdk_window_set_cursor(gtk_widget_get_window(widget),gdk_cursor_new_from_name(gdk_display_get_default(),"left_side"));
  } else {
    gdk_window_set_cursor(gtk_widget_get_window(widget),gdk_cursor_new_from_name(gdk_display_get_default(),"right_side"));
  }
  return FALSE;
}


static gboolean leave (GtkWidget *ebox, GdkEventCrossing *event, void *user_data) {
   gdk_window_set_cursor(gtk_widget_get_window(ebox),gdk_cursor_new(GDK_ARROW));
   return FALSE;
}

static void create_visual(RECEIVER *rx) {

  rx->dialog=NULL;

  rx->window=gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name(rx->window,"receiver-window");
  if(rx->channel==0) {
    gtk_window_set_deletable (GTK_WINDOW(rx->window),FALSE);
  }
  g_signal_connect(rx->window,"configure-event",G_CALLBACK(receiver_configure_event_cb),rx);
  g_signal_connect(rx->window,"focus_in_event",G_CALLBACK(focus_in_event_cb),rx);
  g_signal_connect(rx->window,"delete-event",G_CALLBACK (window_delete), rx);

  receiver_update_title(rx);

  gtk_widget_set_size_request(rx->window,rx->window_width,rx->window_height);

  rx->table=gtk_table_new(4,6,FALSE);

  rx->vfo=create_vfo(rx);
  gtk_widget_set_size_request(rx->vfo,715,65);
  gtk_table_attach(GTK_TABLE(rx->table), rx->vfo, 0, 3, 0, 1,
      GTK_FILL, GTK_FILL, 0, 0);
  
  //GtkWidget *radio_info;
  //cairo_surface_t *radio_info_surface;    
  
  
  rx->radio_info=create_radio_info_visual(rx);
  gtk_widget_set_size_request(rx->radio_info, 250, 60);        
  gtk_table_attach(GTK_TABLE(rx->table), rx->radio_info, 3, 4, 0, 1,
      GTK_FILL, GTK_FILL, 0, 0);  

  rx->meter=create_meter_visual(rx);
  gtk_widget_set_size_request(rx->meter,300,60);        // resize from 154 to 300 for custom s-meter
  gtk_table_attach(GTK_TABLE(rx->table), rx->meter, 4, 6, 0, 1,
      GTK_FILL, GTK_FILL, 0, 0);

  rx->vpaned = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
  gtk_table_attach(GTK_TABLE(rx->table), rx->vpaned, 0, 6, 1, 3,
      GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);

  rx->panadapter=create_rx_panadapter(rx);
  //gtk_table_attach(GTK_TABLE(rx->table), rx->panadapter, 0, 4, 1, 2,
  //    GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
  gtk_paned_pack1 (GTK_PANED(rx->vpaned), rx->panadapter,TRUE,TRUE);
  gtk_widget_add_events (rx->panadapter,GDK_ENTER_NOTIFY_MASK);
  gtk_widget_add_events (rx->panadapter,GDK_LEAVE_NOTIFY_MASK);
  g_signal_connect (rx->panadapter, "enter-notify-event", G_CALLBACK (enter), rx);
  g_signal_connect (rx->panadapter, "leave-notify-event", G_CALLBACK (leave), rx);

  rx->waterfall=create_waterfall(rx);
  //gtk_table_attach(GTK_TABLE(rx->table), rx->waterfall, 0, 4, 2, 3,
  //    GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
  gtk_paned_pack2 (GTK_PANED(rx->vpaned), rx->waterfall,TRUE,TRUE);
  gtk_widget_add_events (rx->waterfall,GDK_ENTER_NOTIFY_MASK);
  gtk_widget_add_events (rx->waterfall,GDK_LEAVE_NOTIFY_MASK);
  g_signal_connect (rx->waterfall, "enter-notify-event", G_CALLBACK (enter), rx->window);
  g_signal_connect (rx->waterfall, "leave-notify-event", G_CALLBACK (leave), rx->window);

  gtk_container_add(GTK_CONTAINER(rx->window),rx->table);

  gtk_widget_set_events(rx->window,gtk_widget_get_events(rx->window)
                     | GDK_FOCUS_CHANGE_MASK
                     | GDK_BUTTON_PRESS_MASK
                     | GDK_BUTTON_RELEASE_MASK);
}

void receiver_init_analyzer(RECEIVER *rx) {
    int flp[] = {0};
    double keep_time = 0.1;
    int n_pixout=1;
    int spur_elimination_ffts = 1;
    int data_type = 1;
    int fft_size = 8192;
    int window_type = 4;
    double kaiser_pi = 14.0;
    int overlap = 2048;
    int clip = 0;
    int span_clip_l = 0;
    int span_clip_h = 0;
    int pixels=rx->pixels;
    int stitches = 1;
    int calibration_data_set = 0;
    double span_min_freq = 0.0;
    double span_max_freq = 0.0;

//g_print("receiver_init_analyzer: channel=%d zoom=%d pixels=%d pixel_samples=%p pan=%d\n",rx->channel,rx->zoom,rx->pixels,rx->pixel_samples,rx->pan);

  if(rx->pixel_samples!=NULL) {
//g_print("receiver_init_analyzer: g_free: channel=%d pixel_samples=%p\n",rx->channel,rx->pixel_samples);
    g_free(rx->pixel_samples);
    rx->pixel_samples=NULL;
  }
  if(rx->pixels>0) {
    rx->pixel_samples=g_new0(float,rx->pixels);
    rx->hz_per_pixel=(gdouble)rx->sample_rate/(gdouble)rx->pixels;

    int max_w = fft_size + (int) fmin(keep_time * (double) rx->fps, keep_time * (double) fft_size * (double) rx->fps);

    //overlap = (int)max(0.0, ceil(fft_size - (double)rx->sample_rate / (double)rx->fps));

//g_print("SetAnalyzer id=%d buffer_size=%d fft_size=%d overlap=%d\n",rx->channel,rx->buffer_size,fft_size,overlap);


    SetAnalyzer(rx->channel,
            n_pixout,
            spur_elimination_ffts, //number of LO frequencies = number of ffts used in elimination
            data_type, //0 for real input data (I only); 1 for complex input data (I & Q)
            flp, //vector with one elt for each LO frequency, 1 if high-side LO, 0 otherwise
            fft_size, //size of the fft, i.e., number of input samples
            rx->buffer_size, //number of samples transferred for each OpenBuffer()/CloseBuffer()
            window_type, //integer specifying which window function to use
            kaiser_pi, //PiAlpha parameter for Kaiser window
            overlap, //number of samples each fft (other than the first) is to re-use from the previous
            clip, //number of fft output bins to be clipped from EACH side of each sub-span
            span_clip_l, //number of bins to clip from low end of entire span
            span_clip_h, //number of bins to clip from high end of entire span
            pixels, //number of pixel values to return.  may be either <= or > number of bins
            stitches, //number of sub-spans to concatenate to form a complete span
            calibration_data_set, //identifier of which set of calibration data to use
            span_min_freq, //frequency at first pixel value8192
            span_max_freq, //frequency at last pixel value
            max_w //max samples to hold in input ring buffers
    );
  }

}

void receiver_change_zoom(RECEIVER *rx,int zoom) {
  rx->zoom=zoom;
  rx->pixels=rx->panadapter_width*rx->zoom;
  if(rx->zoom==1) {
    rx->pan=0;
  } else {
    if(rx->ctun) {
      //long long min_frequency=rx->frequency_a-(long long)(rx->sample_rate/2);
      rx->pan=(rx->pixels/2)-(rx->panadapter_width/2);
      //rx->pan=((rx->min_frequency)/rx->hz_per_pixel)-(rx->panadapter_width/2);
      if(rx->pan<0) rx->pan=0;
      if(rx->pan>(rx->pixels-rx->panadapter_width)) rx->pan=rx->pixels-rx->panadapter_width;
    } else {
      rx->pan=(rx->pixels/2)-(rx->panadapter_width/2);
    }
  }
  receiver_init_analyzer(rx);
}

RECEIVER *create_receiver(int channel, int sample_rate) {

    RECEIVER *rx = g_new0(RECEIVER, 1);
    char name [80];
  char *value;
  gint x=-1;
  gint y=-1;
  gint width = rx->window_width; // Initialize with default
  gint height = rx->window_height; // Initialize with default
    if (!rx) {
        fprintf(stderr, "create_receiver: g_new0 failed for RECEIVER\n");
        return NULL;
    }

    g_print("create_receiver: channel=%d sample_rate=%d\n", channel, sample_rate);
    g_mutex_init(&rx->mutex);

    // Initialize thread context
    ReceiverThreadContext *ctx = &rx->thread_context;
    ctx->running = TRUE;
    ctx->iq_queue = g_async_queue_new();
    if (!ctx->iq_queue) {
        fprintf(stderr, "create_receiver: g_async_queue_new failed for iq_queue\n");
        g_mutex_clear(&rx->mutex);
        g_free(rx);
        return NULL;
    }
    ctx->render_queue = g_async_queue_new();
    if (!ctx->render_queue) {
        fprintf(stderr, "create_receiver: g_async_queue_new failed for render_queue\n");
        g_async_queue_unref(ctx->iq_queue);
        g_mutex_clear(&rx->mutex);
        g_free(rx);
        return NULL;
    }
    g_mutex_init(&ctx->render_mutex);
    ctx->wdsp_thread = NULL;
    ctx->render_thread = NULL;
  rx->channel=channel;
  rx->adc=0;
  

  rx->frequency_min=(gint64)radio->discovered->frequency_min;
  rx->frequency_max=(gint64)radio->discovered->frequency_max;
g_print("create_receiver: channel=%d frequency_min=%ld frequency_max=%ld\n", channel, rx->frequency_min, rx->frequency_max);

// DEBUG
  if(channel==0 ) {
    rx->adc=0;
  } else {
    switch(radio->discovered->protocol) {
      case PROTOCOL_1:
        switch(radio->discovered->device) {
          case DEVICE_ANGELIA:
          case DEVICE_ORION:
          case DEVICE_ORION2:
            rx->adc=1;
            break;
          default:
            rx->adc=0;
            break;
        }
        break;
      case PROTOCOL_2:
        switch(radio->discovered->device) {
          case NEW_DEVICE_ANGELIA:
          case NEW_DEVICE_ORION:
          case NEW_DEVICE_ORION2:
            rx->adc=1;
            break;
          default:
            rx->adc=0;
            break;

        }
        break;
#ifdef SOAPYSDR
      case PROTOCOL_SOAPYSDR:
        if(radio->discovered->supported_receivers>1 &&
// fv - need to figure how to deal with the Lime Suite
           strcmp(radio->discovered->name,"lms7")==0) {
          rx->adc=2;
        } else {
          rx->adc=1;
        }
        break;
#endif
    }
  }
   
  rx->sample_rate=sample_rate;
  rx->dsp_rate=48000;
  rx->output_rate=48000;

  switch(radio->discovered->protocol) {
    default:
      rx->frequency_a=14200000;
      rx->band_a=band20;
      rx->mode_a=USB;
      rx->bandstack=1;
      break;
#ifdef IIO
    case PROTOCOL_IIO:
      rx->frequency_a=2400000000;
      rx->band_a=band2300;
      rx->mode_a=USB;
      rx->bandstack=1;
      break;
#endif
#ifdef SOAPYSDR
    case PROTOCOL_SOAPYSDR:
      rx->frequency_a=145000000;
      rx->band_a=band144;
      rx->mode_a=USB;
      rx->bandstack=1;
      break;
#endif
  }

  rx->deviation=2500;
  rx->filter_a=F5;
  rx->lo_a=0;
  rx->error_a=0;
  rx->offset=0;

  rx->frequency_b=rx->frequency_a;
#ifdef USE_VFO_B_MODE_AND_FILTER
  rx->band_b=rx->band_a;
  rx->mode_b=rx->mode_a;
  rx->filter_b=rx->filter_a;
#endif
  rx->lo_b=rx->lo_a;
  rx->error_b=rx->error_a;


  rx->lo_tx=0;
  rx->tx_track_rx=FALSE;

  rx->ctun=FALSE;
  rx->ctun_offset=0;
  rx->ctun_min=rx->frequency_a-(rx->sample_rate/2);
  rx->ctun_max=rx->frequency_a+(rx->sample_rate/2);

  rx->bpsk=NULL;
  rx->bpsk_enable=FALSE;

  rx->frequency_b=14300000;
#ifdef USE_VFO_B_MODE_AND_FILTER
  rx->band_b=band20;
  rx->mode_b=USB;
  rx->filter_b=F5;
#endif
  rx->lo_b=0;
  rx->error_b=0;

  rx->split=FALSE;

  rx->rit_enabled=FALSE;
  rx->rit=0;
  rx->rit_step=10;

  rx->step=100;
  rx->locked=FALSE;

  rx->volume=0.05;
  rx->agc=AGC_OFF;
  rx->agc_gain=80.0;
  rx->agc_slope=35.0;
  rx->agc_hang_threshold=0.0;

  rx->smeter=RXA_S_AV;

  rx->pixels=0;
  rx->pixel_samples=NULL;
  rx->waterfall_pixbuf=NULL;
  rx->iq_sequence=0;
#ifdef SOAPYSDR
  if(radio->discovered->device==DEVICE_SOAPYSDR) {
    rx->buffer_size=1024; //2048;
  } else {
#endif
    rx->buffer_size=1024;
#ifdef SOAPYSDR
  }
#endif
fprintf(stderr,"create_receiver: buffer_size=%d\n",rx->buffer_size);
  rx->iq_input_buffer=g_new0(gdouble,2*rx->buffer_size);

  rx->audio_buffer_size=480;
  rx->audio_buffer=g_new0(guchar,rx->audio_buffer_size);
  rx->audio_sequence=0;

  rx->mixed_audio=0;

  rx->output_started=FALSE;

  rx->fps=10;
  rx->display_average_time=170.0;

#ifdef SOAPYSDR
  if(radio->discovered->device==DEVICE_SOAPYSDR) {
    rx->fft_size=2048;
  } else {
#endif
    rx->fft_size=2048;
#ifdef SOAPYSDR
  }
#endif
fprintf(stderr,"create_receiver: fft_size=%d\n",rx->fft_size);
  rx->low_latency=FALSE;

  rx->nb=FALSE;
  rx->nb2=FALSE;
  rx->nr=FALSE;
  rx->nr2=FALSE;
  rx->anf=FALSE;
  rx->snb=FALSE;

  rx->nr_agc=0;
  rx->nr2_gain_method=2;
  rx->nr2_npe_method=0;
  rx->nr2_ae=1;

  // set default location and sizes
  rx->window_x=channel*30;
  rx->window_y=channel*30;
  rx->window_width=820;
  rx->window_height=180;
  rx->window=NULL;
  rx->panadapter_low=-140;
  rx->panadapter_high=-60;
  rx->panadapter_step=20;
  rx->panadapter_surface=NULL;
  
  rx->panadapter_filled=TRUE;
  rx->panadapter_gradient=TRUE;
  rx->panadapter_agc_line=TRUE;

  rx->waterfall_automatic=TRUE;
  rx->waterfall_ft8_marker=FALSE;

  rx->vfo_surface=NULL;
  rx->meter_surface=NULL;
  rx->radio_info_surface=NULL;

#ifdef SOAPYSDR
  if(radio->discovered->protocol==PROTOCOL_SOAPYSDR) {
    rx->remote_audio=FALSE;
  } else {
#endif
    rx->remote_audio=TRUE;        // on or off remote audio
#ifdef SOAPYSDR
  }
#endif
  rx->local_audio=FALSE;
  rx->local_audio_buffer_size=4096;
  //rx->local_audio_buffer_size=rx->output_samples;
  rx->local_audio_buffer_offset=0;
  rx->local_audio_buffer=NULL;
  rx->local_audio_latency=50;
  rx->audio_channels = 0;
  g_mutex_init(&rx->local_audio_mutex);


  rx->audio_name=NULL;
  rx->mute_when_not_active=FALSE;

  rx->zoom=1;
  rx->pan=0;
  rx->is_panning=FALSE;
  rx->enable_equalizer=FALSE;
  rx->equalizer[0]=0;
  rx->equalizer[1]=0;
  rx->equalizer[2]=0;
  rx->equalizer[3]=0;

  rx->bookmark_dialog=NULL;

  rx->filter_frame=NULL;
  rx->filter_grid=NULL;

  rx->rigctl_port=19090+rx->channel;
  rx->rigctl_enable=FALSE;

  strcpy(rx->rigctl_serial_port,"/dev/ttyACM0");
  rx->rigctl_serial_baudrate=B9600;
  rx->rigctl_serial_enable=FALSE;
  rx->rigctl_debug=FALSE;
  rx->rigctl=NULL;

  rx->vpaned=NULL;
  rx->paned_position=-1;
  rx->paned_percent=0.5;

  rx->split=SPLIT_OFF;
  rx->duplex=FALSE;
  rx->mute_while_transmitting=FALSE;

  rx->vfo=NULL;
  rx->subrx_enable=FALSE;
  rx->subrx=NULL;

  receiver_restore_state(rx);

  if(radio->discovered->protocol==PROTOCOL_1) {
    if(rx->sample_rate!=sample_rate) {
      rx->sample_rate=sample_rate;
    }
  }
  rx->buffer_size=2048;
   // Initialize ring buffer
    rx->iq_ring_buffer.size = rx->buffer_size * 16; // 16x for I and Q
    rx->iq_ring_buffer.buffer = g_new0(gdouble, rx->iq_ring_buffer.size);
    rx->iq_ring_buffer.head = 0;
    rx->iq_ring_buffer.tail = 0;
    rx->iq_ring_buffer.count = 0;
    g_mutex_init(&rx->iq_ring_buffer.mutex);
  rx->output_samples=rx->buffer_size/(rx->sample_rate/48000);
  rx->audio_output_buffer=g_new0(gdouble,2*rx->output_samples);

g_print("create_receiver: OpenChannel: channel=%d buffer_size=%d sample_rate=%d fft_size=%d output_samples=%d\n", rx->channel, rx->buffer_size, rx->sample_rate, rx->fft_size,rx->output_samples);

  OpenChannel(rx->channel,
              rx->buffer_size,
              rx->fft_size,
              rx->sample_rate,
              48000, // dsp rate
              48000, // output rate
              0, // receive
              1, // run
              0.010, 0.025, 0.0, 0.010, 0);

  create_anbEXT(rx->channel,1,rx->buffer_size,rx->sample_rate,0.0001,0.0001,0.0001,0.05,20);
  create_nobEXT(rx->channel,1,0,rx->buffer_size,rx->sample_rate,0.0001,0.0001,0.0001,0.05,20);
  RXASetNC(rx->channel, rx->fft_size);
  RXASetMP(rx->channel, rx->low_latency);

  frequency_changed(rx);
  receiver_mode_changed(rx,rx->mode_a);

  SetRXAPanelGain1(rx->channel, rx->volume);
  SetRXAPanelSelect(rx->channel, 3);
  SetRXAPanelPan(rx->channel, 0.5);
  SetRXAPanelCopy(rx->channel, 0);
  SetRXAPanelBinaural(rx->channel, 0);
  SetRXAPanelRun(rx->channel, 1);

  if(rx->enable_equalizer) {
    SetRXAGrphEQ(rx->channel, rx->equalizer);
    SetRXAEQRun(rx->channel, 1);
  } else {
    SetRXAEQRun(rx->channel, 0);
  }

  set_agc(rx);

  SetEXTANBRun(rx->channel, rx->nb);
  SetEXTNOBRun(rx->channel, rx->nb2);

  SetRXAEMNRPosition(rx->channel, rx->nr_agc);
  SetRXAEMNRgainMethod(rx->channel, rx->nr2_gain_method);
  SetRXAEMNRnpeMethod(rx->channel, rx->nr2_npe_method);
  SetRXAEMNRRun(rx->channel, rx->nr2);
  SetRXAEMNRaeRun(rx->channel, rx->nr2_ae);

  SetRXAANRVals(rx->channel, 64, 16, 16e-4, 10e-7); // defaults
  SetRXAANRRun(rx->channel, rx->nr);
  SetRXAANFRun(rx->channel, rx->anf);
  SetRXASNBARun(rx->channel, rx->snb);

  int result;
  XCreateAnalyzer(rx->channel, &result, 262144, 1, 1, "");
  if(result != 0) {
    g_print("XCreateAnalyzer channel=%d failed: %d\n", rx->channel, result);
  } else {
    receiver_init_analyzer(rx);
  }

  SetDisplayDetectorMode(rx->channel, 0, DETECTOR_MODE_AVERAGE/*display_detector_mode*/);
  SetDisplayAverageMode(rx->channel, 0,  AVERAGE_MODE_LOG_RECURSIVE/*display_average_mode*/);
  calculate_display_average(rx); 

  create_visual(rx);
  if(rx->window!=NULL) {
    gtk_widget_show_all(rx->window);
    sprintf(name,"receiver[%d].x",rx->channel);
    value=getProperty(name);
    if(value) x=atoi(value);
    sprintf(name,"receiver[%d].y",rx->channel);
    value=getProperty(name);
    if(value) y=atoi(value);
    if(x!=-1 && y!=-1) {
      gtk_window_move(GTK_WINDOW(rx->window),x,y);
    
      sprintf(name,"receiver[%d].width",rx->channel);
      value=getProperty(name);
      if(value) width=atoi(value);
      sprintf(name,"receiver[%d].height",rx->channel);
      value=getProperty(name);
      if(value) height=atoi(value);
      gtk_window_resize(GTK_WINDOW(rx->window),width,height);

      if(rx->vpaned!=NULL && rx->paned_position!=-1) {
        gint paned_height=gtk_widget_get_allocated_height(rx->vpaned);
        gint position=(gint)((double)paned_height*rx->paned_percent);
        gtk_paned_set_position(GTK_PANED(rx->vpaned),position);
//g_print("receiver_configure_event: gtk_paned_set_position: rx=%d position=%d height=%d percent=%f\n",rx->channel,position,paned_height,rx->paned_percent);
      }
    }

    update_frequency(rx);
  }

  rx->update_timer_id=g_timeout_add(1000/rx->fps,update_timer_cb,(gpointer)rx);
  // Start WDSP thread
  ctx->wdsp_thread = g_thread_new("ctx->wdsp_thread", wdsp_processing_thread, rx);
  // Start Render thread
  ctx->render_thread = g_thread_new("ctx->render_thread", render_processing_thread, rx);


  if(rx->local_audio) {
    if(audio_open_output(rx)<0) {
      rx->local_audio=FALSE;
    }
  }

  if(rx->bpsk_enable) {
    rx->bpsk=create_bpsk(BPSK_CHANNEL,rx->band_a);
  }


  if(rx->rigctl_enable) {
    launch_rigctl(rx);
  }

  if(rx->rigctl_serial_enable) {
    launch_serial(rx);
  }

  return rx;
}


void receiver_set_volume(RECEIVER *rx) {
  SetRXAPanelGain1(rx->channel, rx->volume);
  if(rx->subrx_enable) {
    subrx_volume_changed(rx);
  }
}

void receiver_set_agc_gain(RECEIVER *rx) {
  SetRXAAGCTop(rx->channel, rx->agc_gain);
}

void receiver_set_ctun(RECEIVER *rx) {
  rx->ctun_offset=0;
  rx->ctun_frequency=rx->frequency_a;
  rx->ctun_min=rx->frequency_a-(rx->sample_rate/2);
  rx->ctun_max=rx->frequency_a+(rx->sample_rate/2);
  if(!rx->ctun) {
    SetRXAShiftRun(rx->channel, 0);
  } else {
    SetRXAShiftRun(rx->channel, 1);
  }
  frequency_changed(rx);
  update_frequency(rx);
}