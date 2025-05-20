/* Copyright (C)
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
*/

#include <gtk/gtk.h>
#include <math.h>
#include <stdlib.h>
#include <wdsp.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "agc.h"
#include "bpsk.h"
#include "mode.h"
#include "filter.h"
#include "band.h"
#include "receiver.h"
#include "transmitter.h"
#include "wideband.h"
#include "wideband_panadapter.h"
#include "discovered.h"
#include "adc.h"
#include "dac.h"
#include "radio.h"
#include "main.h"
#include "vfo.h"

#define GRADIANT
#define MAX_PIXELS 2048 // Optimization: Cap pixels to limit memory
#define LINE_WIDTH 1.0


static gboolean resize_timeout(void *data) {
  WIDEBAND *w = (WIDEBAND *)data;
  wPanadapterCache *px = &w->wpanadapter_cache;

  w->panadapter_width = w->panadapter_resize_width;
  w->panadapter_height = w->panadapter_resize_height;
  w->pixels = w->panadapter_width;

  // Optimization: Cap pixels to prevent excessive memory
  if (w->pixels > MAX_PIXELS) w->pixels = MAX_PIXELS;

  wideband_init_analyzer(w);

  if (w->panadapter_surface) {
    cairo_surface_destroy(w->panadapter_surface);
    w->panadapter_surface = NULL;
  }

  if (w->wpanadapter_cache.static_surface) {
    cairo_surface_destroy(w->wpanadapter_cache.static_surface);
    w->wpanadapter_cache.static_surface = NULL;
  }

  if (w->panadapter != NULL) {
    w->panadapter_surface = gdk_window_create_similar_surface(
        gtk_widget_get_window(w->panadapter),
        CAIRO_CONTENT_COLOR,
        w->panadapter_width,
        w->panadapter_height);

    // Initialize to black
    cairo_t *cr = cairo_create(w->panadapter_surface);
#ifdef GRADIANT
    cairo_pattern_t *pat = cairo_pattern_create_linear(0.0, 0.0, 0.0, w->panadapter_height);
    cairo_pattern_add_color_stop_rgba(pat, 1.0, 0.1, 0.1, 0.1, 0.5);
    cairo_pattern_add_color_stop_rgba(pat, 0.0, 0.5, 0.5, 0.5, 0.5);
    cairo_set_source(cr, pat);
    cairo_paint(cr);
    cairo_pattern_destroy(pat);
#else
    cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
    cairo_paint(cr);
#endif
    cairo_destroy(cr);
  }

  w->panadapter_resize_timer = -1;
  return FALSE;
}

static gboolean wideband_panadapter_configure_event_cb(GtkWidget *widget, GdkEventConfigure *event, gpointer data) {
  WIDEBAND *w = (WIDEBAND *)data;
  gint width = gtk_widget_get_allocated_width(widget);
  gint height = gtk_widget_get_allocated_height(widget);
  if (width != w->panadapter_width || height != w->panadapter_height) {
    w->panadapter_resize_width = width;
    w->panadapter_resize_height = height;
    if (w->panadapter_resize_timer != -1) {
      g_source_remove(w->panadapter_resize_timer);
    }
    w->panadapter_resize_timer = g_timeout_add(250, resize_timeout, (gpointer)w);
  }
  return TRUE;
}

static gboolean wideband_panadapter_draw_cb(GtkWidget *widget, cairo_t *cr, gpointer data) {
  WIDEBAND *w = (WIDEBAND *)data;
  if (w->panadapter_surface != NULL) {
    cairo_set_source_surface(cr, w->panadapter_surface, 0.0, 0.0);
    cairo_paint(cr);
  }
  return TRUE;
}

GtkWidget *create_wideband_panadapter(WIDEBAND *w) {
  GtkWidget *panadapter;
  wPanadapterCache *px = &w->wpanadapter_cache;

  w->panadapter_width = 0;
  w->panadapter_height = 0;
  w->panadapter_surface = NULL;
  w->wpanadapter_cache.static_surface = NULL;
  w->wpanadapter_cache.width = 0;
  w->wpanadapter_cache.height = 0;
  w->wpanadapter_cache.pixels = 0;
  w->panadapter_resize_timer = -1;

  panadapter = gtk_drawing_area_new();

  g_signal_connect(panadapter, "configure-event", G_CALLBACK(wideband_panadapter_configure_event_cb), (gpointer)w);
  g_signal_connect(panadapter, "draw", G_CALLBACK(wideband_panadapter_draw_cb), (gpointer)w);
  g_signal_connect(panadapter, "motion-notify-event", G_CALLBACK(wideband_motion_notify_event_cb), w);
  g_signal_connect(panadapter, "button-press-event", G_CALLBACK(wideband_button_press_event_cb), w);
  g_signal_connect(panadapter, "button-release-event", G_CALLBACK(wideband_button_release_event_cb), w);
  g_signal_connect(panadapter, "scroll-event", G_CALLBACK(wideband_scroll_event_cb), w);

  // Optimization: Limit events to essential ones
  gtk_widget_set_events(panadapter,
                        GDK_BUTTON_PRESS_MASK |
                        GDK_BUTTON_RELEASE_MASK |
                        GDK_BUTTON1_MOTION_MASK |
                        GDK_SCROLL_MASK |
                        GDK_POINTER_MOTION_MASK);

  return panadapter;
}

// Optimization: Draw static elements (grid, markers) to a cached surface
static void draw_static_elements(WIDEBAND *w, cairo_t *cr) {
  gint display_height = w->panadapter_height;
  gdouble hz_per_pixel = (gdouble)61440000 / (gdouble)w->pixels;

  // Background gradient
#ifdef GRADIANT
  cairo_pattern_t *pat = cairo_pattern_create_linear(0.0, 0.0, 0.0, display_height);
  cairo_pattern_add_color_stop_rgba(pat, 1.0, 0.1, 0.1, 0.1, 0.5);
  cairo_pattern_add_color_stop_rgba(pat, 0.0, 0.5, 0.5, 0.5, 0.5);
  cairo_set_source(cr, pat);
  cairo_paint(cr);
  cairo_pattern_destroy(pat);
#else
  cairo_set_source_rgb(cr, 0.2, 0.2, 0.2);
  cairo_paint(cr);
#endif

  // dBm grid
  cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
  double dbm_per_line = (double)display_height / ((double)w->panadapter_high - (double)w->panadapter_low);
  cairo_set_line_width(cr, LINE_WIDTH);
  cairo_select_font_face(cr, "FreeMono", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 12);
  char v[32];

  for (long i = w->panadapter_high; i >= w->panadapter_low; i--) {
    int mod = labs(i) % 20;
    if (mod == 0) {
      double y = (double)(w->panadapter_high - i) * dbm_per_line;
      cairo_move_to(cr, 0.0, y);
      cairo_line_to(cr, (double)w->panadapter_width, y);
      sprintf(v, "%ld dBm", i);
      cairo_move_to(cr, 1, y);
      cairo_show_text(cr, v);
    }
  }
  cairo_stroke(cr);

  // Frequency markers
  for (long i = 5000000; i < 61440000; i += 5000000) {
    gdouble x = (gdouble)i / hz_per_pixel;
    cairo_move_to(cr, x, 10.0);
    cairo_line_to(cr, x, (double)display_height);
    sprintf(v, "%ld", i / 1000000);
    cairo_text_extents_t extents;
    cairo_text_extents(cr, v, &extents);
    cairo_move_to(cr, x - (extents.width / 2.0), 10.0);
    cairo_show_text(cr, v);
  }
  cairo_stroke(cr);
}

void update_wideband_panadapter(WIDEBAND *w) {
  if (w->panadapter_surface == NULL || w->pixel_samples == NULL || w->panadapter_height <= 1) {
    return; // Optimization: Early exit for invalid state
  }

  wPanadapterCache *px = &w->wpanadapter_cache;
  gint display_width = w->panadapter_width;
  gint display_height = w->panadapter_height;
  gdouble hz_per_pixel = (gdouble)61440000 / (gdouble)w->pixels;
  float *samples = w->pixel_samples;

  // Optimization: Redraw static elements only on change
  gboolean redraw_static = FALSE;
  if (!px->static_surface ||
      px->width != display_width ||
      px->height != display_height ||
      px->pixels != w->pixels ||
      px->panadapter_high != w->panadapter_high ||
      px->panadapter_low != w->panadapter_low) {
    redraw_static = TRUE;
    px->width = display_width;
    px->height = display_height;
    px->pixels = w->pixels;
    px->panadapter_high = w->panadapter_high;
    px->panadapter_low = w->panadapter_low;
  }

  if (redraw_static) {
    if (px->static_surface) {
      cairo_surface_destroy(px->static_surface);
    }
    px->static_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, display_width, display_height);
    cairo_t *cr_static = cairo_create(px->static_surface);
    draw_static_elements(w, cr_static);
    cairo_destroy(cr_static);
  }

  // Initialize main surface
  cairo_t *cr = cairo_create(w->panadapter_surface);
  cairo_set_line_width(cr, LINE_WIDTH);

  // Copy static elements
  cairo_set_source_surface(cr, px->static_surface, 0, 0);
  cairo_paint(cr);

  // Cursor
  if (radio->active_receiver != NULL) {
    cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
    gdouble x = (gdouble)radio->active_receiver->frequency_a / hz_per_pixel;
    cairo_move_to(cr, x, 0.0);
    cairo_line_to(cr, x, (double)display_height);
    cairo_stroke(cr);
  }

  // Signal plot using bulk operation
  double dbm_per_line = (double)display_height / ((double)w->panadapter_high - (double)w->panadapter_low);
  cairo_surface_t *plot_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, display_width, display_height);
  unsigned char *plot_data = cairo_image_surface_get_data(plot_surface);
  int plot_stride = cairo_image_surface_get_stride(plot_surface);

  // Optimization: Precompute y-coordinates to reduce Cairo calls
  double *y_values = g_new(double, w->pixels);
  samples[w->pixels] = -200.0;
  samples[(w->pixels * 2) - 1] = -200.0;

  for (int i = 0; i < w->pixels; i++) {
    double s = (double)samples[i + w->pixels];
    s = floor((w->panadapter_high - s) * dbm_per_line);
    if (s < 0) s = 0;
    if (s >= display_height) s = display_height - 1;
    y_values[i] = s;
  }

  // Fill signal area
  for (int x = 0; x < display_width; x++) {
    int idx = x * w->pixels / display_width; // Linear mapping
    int y_top = (int)y_values[idx];
    for (int y = y_top; y < display_height; y++) {
      unsigned char *pixel = plot_data + y * plot_stride + x * 4;
#ifdef GRADIANT
      double t = (double)y / (double)display_height;
      pixel[2] = (unsigned char)(255 * (1.0 - t)); // R
      pixel[1] = (unsigned char)(255 * (1.0 - t)); // G
      pixel[0] = (unsigned char)(255 * t);         // B
      pixel[3] = (unsigned char)(128);            // A (0.5)
#else
      pixel[2] = 255; // R
      pixel[1] = 255; // G
      pixel[0] = 255; // B
      pixel[3] = 128; // A
#endif
    }
  }

  cairo_surface_mark_dirty(plot_surface);

  // Composite signal plot
  if (radio->display_filled) {
    cairo_set_source_surface(cr, plot_surface, 0, 0);
    cairo_paint(cr);
  }

  // Draw outline
  cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
  cairo_move_to(cr, 0.0, y_values[0]);
  for (int i = 1; i < w->pixels; i++) {
    cairo_line_to(cr, (double)i, y_values[i]);
  }
  cairo_stroke(cr);

  g_free(y_values);
  cairo_surface_destroy(plot_surface);
  cairo_destroy(cr);
  gtk_widget_queue_draw(w->panadapter);
}