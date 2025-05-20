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
#include <string.h>

#include "wideband.h"
#include "wideband_waterfall.h"

#define MAX_WIDTH 2048  // Optimization: Cap width to limit memory
#define MAX_HEIGHT 1024 // Optimization: Cap height for large displays
#define COLOR_STEPS 256 // Optimization: Precompute color table

static const int colorLowR = 0;   // Black
static const int colorLowG = 0;
static const int colorLowB = 0;

static const int colorHighR = 255; // Yellow
static const int colorHighG = 255;
static const int colorHighB = 0;

// Optimization: Precomputed color table
static guchar color_table[COLOR_STEPS][3];

static void init_color_table() {
  // Initialize once at startup
  for (int i = 0; i < COLOR_STEPS; i++) {
    float percent = (float)i / (COLOR_STEPS - 1);
    if (percent < (2.0f / 9.0f)) {
      float local_percent = percent / (2.0f / 9.0f);
      color_table[i][0] = (guchar)((1.0f - local_percent) * colorLowR);
      color_table[i][1] = (guchar)((1.0f - local_percent) * colorLowG);
      color_table[i][2] = (guchar)(colorLowB + local_percent * (255 - colorLowB));
    } else if (percent < (3.0f / 9.0f)) {
      float local_percent = (percent - 2.0f / 9.0f) / (1.0f / 9.0f);
      color_table[i][0] = 0;
      color_table[i][1] = (guchar)(local_percent * 255);
      color_table[i][2] = 255;
    } else if (percent < (4.0f / 9.0f)) {
      float local_percent = (percent - 3.0f / 9.0f) / (1.0f / 9.0f);
      color_table[i][0] = 0;
      color_table[i][1] = 255;
      color_table[i][2] = (guchar)((1.0f - local_percent) * 255);
    } else if (percent < (5.0f / 9.0f)) {
      float local_percent = (percent - 4.0f / 9.0f) / (1.0f / 9.0f);
      color_table[i][0] = (guchar)(local_percent * 255);
      color_table[i][1] = 255;
      color_table[i][2] = 0;
    } else if (percent < (7.0f / 9.0f)) {
      float local_percent = (percent - 5.0f / 9.0f) / (2.0f / 9.0f);
      color_table[i][0] = 255;
      color_table[i][1] = (guchar)((1.0f - local_percent) * 255);
      color_table[i][2] = 0;
    } else if (percent < (8.0f / 9.0f)) {
      float local_percent = (percent - 7.0f / 9.0f) / (1.0f / 9.0f);
      color_table[i][0] = 255;
      color_table[i][1] = 0;
      color_table[i][2] = (guchar)(local_percent * 255);
    } else {
      float local_percent = (percent - 8.0f / 9.0f) / (1.0f / 9.0f);
      color_table[i][0] = (guchar)((0.75f + 0.25f * (1.0f - local_percent)) * 255.0f);
      color_table[i][1] = (guchar)(local_percent * 255.0f * 0.5f);
      color_table[i][2] = 255;
    }
  }
}

static gboolean resize_timeout(void *data) {
  WIDEBAND *w = (WIDEBAND *)data;

  // Optimization: Cap dimensions
  w->waterfall_width = w->waterfall_resize_width > MAX_WIDTH ? MAX_WIDTH : w->waterfall_resize_width;
  w->waterfall_height = w->waterfall_resize_height > MAX_HEIGHT ? MAX_HEIGHT : w->waterfall_resize_height;

  if (w->waterfall_pixbuf) {
    g_object_unref((gpointer)w->waterfall_pixbuf);
    w->waterfall_pixbuf = NULL;
  }

  if (w->waterfall != NULL) {
    w->waterfall_pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, w->waterfall_width, w->waterfall_height);
    guchar *pixels = gdk_pixbuf_get_pixels(w->waterfall_pixbuf);
    memset(pixels, 0, w->waterfall_width * w->waterfall_height * 3);
  }
  w->waterfall_frequency = 0;
  w->waterfall_sample_rate = 0;
  w->waterfall_resize_timer = -1;
  return FALSE;
}

static gboolean waterfall_configure_event_cb(GtkWidget *widget, GdkEventConfigure *event, gpointer data) {
  WIDEBAND *w = (WIDEBAND *)data;
  gint width = gtk_widget_get_allocated_width(widget);
  gint height = gtk_widget_get_allocated_height(widget);
  if (width != w->waterfall_width || height != w->waterfall_height) {
    w->waterfall_resize_width = width;
    w->waterfall_resize_height = height;
    if (w->waterfall_resize_timer != -1) {
      g_source_remove(w->waterfall_resize_timer);
    }
    w->waterfall_resize_timer = g_timeout_add(250, resize_timeout, (gpointer)w);
  }
  return TRUE;
}

static gboolean waterfall_draw_cb(GtkWidget *widget, cairo_t *cr, gpointer data) {
  WIDEBAND *w = (WIDEBAND *)data;
  if (w->waterfall_pixbuf) {
    gdk_cairo_set_source_pixbuf(cr, w->waterfall_pixbuf, 0, 0);
    cairo_paint(cr);
  }
  return FALSE;
}

GtkWidget *create_wideband_waterfall(WIDEBAND *w) {
  static gboolean color_table_initialized = FALSE;

  // Optimization: Initialize color table once
  if (!color_table_initialized) {
    init_color_table();
    color_table_initialized = TRUE;
  }

  GtkWidget *waterfall;

  w->waterfall_width = 0;
  w->waterfall_height = 0;
  w->waterfall_resize_timer = -1;
  w->waterfall_pixbuf = NULL;

  waterfall = gtk_drawing_area_new();

  g_signal_connect(waterfall, "configure-event", G_CALLBACK(waterfall_configure_event_cb), (gpointer)w);
  g_signal_connect(waterfall, "draw", G_CALLBACK(waterfall_draw_cb), (gpointer)w);

  // Optimization: Remove commented-out event handlers
  gtk_widget_set_events(waterfall, 0); // No events needed

  return waterfall;
}

void update_wideband_waterfall(WIDEBAND *w) {
  if (!w->waterfall_pixbuf || w->waterfall_height <= 1 || !w->pixel_samples) {
    return; // Optimization: Early exit for invalid state
  }

  guchar *pixels = gdk_pixbuf_get_pixels(w->waterfall_pixbuf);
  int width = gdk_pixbuf_get_width(w->waterfall_pixbuf);
  int height = gdk_pixbuf_get_height(w->waterfall_pixbuf);
  int rowstride = gdk_pixbuf_get_rowstride(w->waterfall_pixbuf);

  // Optimization: Scroll pixbuf efficiently
  memmove(pixels + rowstride, pixels, (height - 1) * rowstride);

  float *samples = w->pixel_samples;
  guchar *p = pixels;
  int average = 0;
  int count = 0;

  // Optimization: Bulk pixel updates with color table
  for (int i = 0; i < w->pixels; i++) {
    float sample = samples[i + w->pixels];
    if (i > 0 && i < w->pixels - 1) {
      average += (int)sample;
      count++;
    }

    if (sample < (float)w->waterfall_low) {
      p[0] = colorLowR;
      p[1] = colorLowG;
      p[2] = colorLowB;
    } else if (sample > (float)w->waterfall_high) {
      p[0] = colorHighR;
      p[1] = colorHighG;
      p[2] = colorHighB;
    } else {
      float range = (float)w->waterfall_high - (float)w->waterfall_low;
      float offset = sample - (float)w->waterfall_low;
      int index = (int)((offset / range) * (COLOR_STEPS - 1));
      if (index < 0) index = 0;
      if (index >= COLOR_STEPS) index = COLOR_STEPS - 1;
      p[0] = color_table[index][0];
      p[1] = color_table[index][1];
      p[2] = color_table[index][2];
    }
    p += 3;
  }

  if (w->waterfall_automatic && count > 0) {
    w->waterfall_low = average / count;
    w->waterfall_high = w->waterfall_low + 50;
  }

  gtk_widget_queue_draw(w->waterfall);
}