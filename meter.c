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
*
*/

#include <gtk/gtk.h>
#include <math.h>
#include <wdsp.h>
#include "discovered.h"
#include "bpsk.h"
#include "adc.h"
#include "dac.h"
#include "receiver.h"
#include "transmitter.h"
#include "wideband.h"
#include "radio.h"
#include "meter.h"
#include "main.h"
#include "vfo.h"

typedef struct _choice {
    RECEIVER *rx;
    int selection;
} CHOICE;


static void meter_realize_cb(GtkWidget *widget, gpointer data) {
    RECEIVER *rx = (RECEIVER *)data;
    GdkWindow *window = gtk_widget_get_window(widget);
    if (window && GDK_IS_WINDOW(window)) {
        int meter_width = gtk_widget_get_allocated_width(widget);
        int meter_height = gtk_widget_get_allocated_height(widget);
        rx->meter_surface = gdk_window_create_similar_surface(window, CAIRO_CONTENT_COLOR,
                                                              meter_width, meter_height);
        g_print("meter_realize_cb: Created meter_surface for rx=%d\n", rx->channel);
    } else {
        g_warning("meter_realize_cb: Invalid window for rx=%d", rx->channel);
    }
}

static gboolean meter_configure_event_cb(GtkWidget *widget, GdkEventConfigure *event, gpointer data) {
    RECEIVER *rx = (RECEIVER *)data;
    MeterCache *cache = &rx->meter_cache;
    int meter_width = gtk_widget_get_allocated_width(widget);
    int meter_height = gtk_widget_get_allocated_height(widget);

    // Resize meter_surface if needed
    if (rx->meter_surface) {
        cairo_surface_destroy(rx->meter_surface);
    }
    rx->meter_surface = gdk_window_create_similar_surface(gtk_widget_get_window(widget),
                                                          CAIRO_CONTENT_COLOR,
                                                          meter_width, meter_height);

    // Update static surface
    if (cache->static_surface) {
        cairo_surface_destroy(cache->static_surface);
    }
    cache->static_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, meter_width, meter_height);
    cache->width = meter_width;
    cache->height = meter_height;

    // Draw static elements
    cairo_t *cr = cairo_create(cache->static_surface);
    SetColour(cr, BACKGROUND);
    cairo_paint(cr);
    cairo_set_font_size(cr, 12);
    cairo_select_font_face(cr, "Noto Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

    double cx = (double)meter_width - 100.0;
    double cy = 100.0;
    double radius = cy - 20.0;
    double offset = 210.0;

    // Draw static arcs
    cairo_new_path(cr); // Clear path before arc
    cairo_set_line_width(cr, 1.0);
    SetColour(cr, OFF_WHITE);
    cairo_arc(cr, cx, cy, radius, 216.0 * M_PI / 180.0, 324.0 * M_PI / 180.0);
    cairo_stroke(cr);
    cairo_new_path(cr); // Clear path after arc

    cairo_new_path(cr); // Clear path before arc
    cairo_set_line_width(cr, 2.0);
    SetColour(cr, BOX_ON);
    cairo_arc(cr, cx, cy, radius + 2, 264.0 * M_PI / 180.0, 324.0 * M_PI / 180.0);
    cairo_stroke(cr);
    cairo_new_path(cr); // Clear path after arc

    // Draw static tick marks and labels for S-units
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    char sf[32];
    for (int i = 1; i < 10; i++) {
        double angle = ((double)i * 6.0) + offset;
        double radians = angle * M_PI / 180.0;
        double x, y;
        if ((i % 2) == 1) {
            // Draw tick mark
            cairo_new_path(cr); // Clear path before tick
            cairo_arc(cr, cx, cy, radius + 4, radians, radians);
            cairo_get_current_point(cr, &x, &y);
            cairo_arc(cr, cx, cy, radius, radians, radians);
            cairo_line_to(cr, x, y);
            cairo_stroke(cr);
            cairo_new_path(cr); // Clear path after tick

            // Draw label
            cairo_new_path(cr); // Clear path before text
            sprintf(sf, "%d", i);
            cairo_arc(cr, cx, cy, radius + 5, radians, radians);
            cairo_get_current_point(cr, &x, &y);
            x -= 4.0;
            cairo_move_to(cr, x, y);
            cairo_show_text(cr, sf);
            cairo_new_path(cr); // Clear path after text
        } else {
            cairo_new_path(cr); // Clear path before tick
            cairo_arc(cr, cx, cy, radius + 2, radians, radians);
            cairo_get_current_point(cr, &x, &y);
            cairo_arc(cr, cx, cy, radius, radians, radians);
            cairo_line_to(cr, x, y);
            cairo_stroke(cr);
            cairo_new_path(cr); // Clear path after tick
        }
    }

    // Draw static tick marks and labels for dB values
    for (int i = 20; i <= 60; i += 20) {
        double angle = ((double)i + 54.0) + offset;
        double radians = angle * M_PI / 180.0;
        double x, y;
        // Draw tick mark
        cairo_new_path(cr); // Clear path before tick
        cairo_arc(cr, cx, cy, radius + 4, radians, radians);
        cairo_get_current_point(cr, &x, &y);
        cairo_arc(cr, cx, cy, radius, radians, radians);
        cairo_line_to(cr, x, y);
        cairo_stroke(cr);
        cairo_new_path(cr); // Clear path after tick

        // Draw label
        cairo_new_path(cr); // Clear path before text
        sprintf(sf, "+%d", i);
        cairo_arc(cr, cx, cy, radius + 5, radians, radians);
        cairo_get_current_point(cr, &x, &y);
        x -= 4.0;
        cairo_move_to(cr, x, y);
        cairo_show_text(cr, sf);
        cairo_new_path(cr); // Clear path after text
    }

    cairo_destroy(cr);
    return TRUE;
}

static gboolean meter_draw_cb(GtkWidget *widget, cairo_t *cr, gpointer data) {
    RECEIVER *rx = (RECEIVER *)data;
    if (!gtk_widget_get_realized(widget)) {
        g_print("meter_draw_cb: Widget not realized for rx=%d, skipping draw\n", rx->channel);
        return TRUE;
    }
    if (rx->meter_surface) {
        cairo_set_source_surface(cr, rx->meter_surface, 0.0, 0.0);
        cairo_paint(cr);
    } else {
        g_print("meter_draw_cb: meter_surface is NULL for rx=%d, skipping paint\n", rx->channel);
    }
    return TRUE;
}

static void meter_destroy_cb(GtkWidget *widget, gpointer data) {
    RECEIVER *rx = (RECEIVER *)data;
    MeterCache *cache = &rx->meter_cache;
    if (rx->meter_surface) {
        cairo_surface_destroy(rx->meter_surface);
        rx->meter_surface = NULL;
    }
    if (cache->static_surface) {
        cairo_surface_destroy(cache->static_surface);
        cache->static_surface = NULL;
    }
}

static void meter_cb(GtkWidget *menu_item, gpointer data) {
    CHOICE *choice = (CHOICE *)data;
    choice->rx->smeter = choice->selection;
}

static gboolean meter_press_event_cb(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    GtkWidget *menu;
    GtkWidget *menu_item;
    CHOICE *choice;
    RECEIVER *rx = (RECEIVER *)data;

    switch (event->button) {
        case 1: // LEFT
            menu = gtk_menu_new();
            menu_item = gtk_menu_item_new_with_label("S Meter Peak");
            choice = g_new0(CHOICE, 1);
            choice->rx = rx;
            choice->selection = RXA_S_PK;
            g_signal_connect(menu_item, "activate", G_CALLBACK(meter_cb), choice);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
            menu_item = gtk_menu_item_new_with_label("S Meter Average");
            choice = g_new0(CHOICE, 1);
            choice->rx = rx;
            choice->selection = RXA_S_AV;
            g_signal_connect(menu_item, "activate", G_CALLBACK(meter_cb), choice);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
            gtk_widget_show_all(menu);
#if GTK_CHECK_VERSION(3,22,0)
            gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);
#else
            gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);
#endif
            break;
    }
    return TRUE;
}

GtkWidget *create_meter_visual(RECEIVER *rx) {
    rx->meter_cache.static_surface = NULL;
    rx->meter_cache.width = 0;
    rx->meter_cache.height = 0;

    GtkWidget *meter = gtk_drawing_area_new();
    g_signal_connect(meter, "realize", G_CALLBACK(meter_realize_cb), rx);
    g_signal_connect(meter, "configure-event", G_CALLBACK(meter_configure_event_cb), rx);
    g_signal_connect(meter, "draw", G_CALLBACK(meter_draw_cb), rx);
    g_signal_connect(meter, "button-press-event", G_CALLBACK(meter_press_event_cb), rx);
    g_signal_connect(meter, "destroy", G_CALLBACK(meter_destroy_cb), rx);
    gtk_widget_set_events(meter, gtk_widget_get_events(meter) | GDK_BUTTON_PRESS_MASK);
    return meter;
}

void update_meter(RECEIVER *rx) {
    rx->smax = 0.0;
    static double last_level = -200.0; // Initialize to a low value
    char sf[32];
    cairo_t *cr;
    MeterCache *cache = &rx->meter_cache;

    int meter_width = gtk_widget_get_allocated_width(rx->meter);
    int meter_height = gtk_widget_get_allocated_height(rx->meter);

    // Skip update if level hasn't changed significantly
    double attenuation = radio->adc[rx->adc].attenuation;
    if (radio->discovered->device == DEVICE_HERMES_LITE2) {
        attenuation = -attenuation;
    }
    double level = rx->meter_db + attenuation;
    if (fabs(level - last_level) < 0.5) { // Threshold for redraw
        return;
    }
    last_level = level;

    // Ensure static surface is valid
    if (!cache->static_surface || cache->width != meter_width || cache->height != meter_height) {
        meter_configure_event_cb(rx->meter, NULL, rx); // Force static surface update
    }

    // Draw to meter_surface
    cr = cairo_create(rx->meter_surface);
    cairo_set_source_surface(cr, cache->static_surface, 0.0, 0.0);
    cairo_paint(cr);

    // Draw dynamic elements
    cairo_set_font_size(cr, 12);
    cairo_select_font_face(cr, "Noto Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    double cx = (double)meter_width - 100.0;
    double cy = 100.0;
    double radius = cy - 20.0;
    double offset = 210.0;

    // Draw needle
    cairo_set_line_width(cr, 1.5);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    double angle = level + 127.0 + offset;
    double radians = angle * M_PI / 180.0;
    cairo_arc(cr, cx, cy, radius + 8, radians, radians);
    cairo_line_to(cr, cx, cy);
    cairo_stroke(cr);

    // Draw dBm and mode text
    SetColour(cr, TEXT_A);
    sprintf(sf, "%d dBm %s", (int)level, rx->smeter == RXA_S_AV ? "Av" : "Pk");
    cairo_move_to(cr, meter_width - 130, meter_height - 2);
    cairo_show_text(cr, sf);

    // Calculate and draw S-unit
    SetColour(cr, TEXT_C);
    cairo_set_font_size(cr, 36);
    level = level + 127.0;
    if (level < 0) {
        level = 0;
    }
    if (level > rx->smax) {
        rx->smax = level;
    } else {
        if (level > 54) {
            rx->smax -= (rx->smax - level) / (3.0 * rx->fps);
        } else {
            rx->smax -= (rx->smax - level) / (rx->fps / 2.0);
        }
    }
    int i = (int)(rx->smax / 6.0);
    if (i > 9) {
        i = 9;
    }
    sprintf(sf, "S%d", i);
    cairo_move_to(cr, meter_width - 267, meter_height - 20);
    cairo_show_text(cr, sf);

    // Draw additional dB if needed
    i = (int)rx->smax;
    if (i > 54) {
        i = i - 54;
        cairo_set_font_size(cr, 24);
        sprintf(sf, "+%d", i);
        cairo_move_to(cr, meter_width - 225, (meter_height / 2) + 5);
        cairo_show_text(cr, sf);
    }

    cairo_destroy(cr);
    gtk_widget_queue_draw(rx->meter);
}