/* noise_menu.c
 * Copyright (C) 2025 W4WHL
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
 */

#include <gtk/gtk.h>
#include <wdsp.h>
#include "bpsk.h"
#include "receiver.h"
#include "transmitter.h"
#include "wideband.h"
#include "discovered.h"
#include "adc.h"
#include "dac.h"
#include "radio.h"
#include "main.h"
#include "vfo.h"

// Global container variables
static GtkWidget *nr_container = NULL;
static GtkWidget *nb_container = NULL;

// Global widgets for updating in receiver_changed_cb
static GtkWidget *snb_check;
static GtkWidget *anf_check;
static GtkWidget *nr_combo;
static GtkWidget *nb_combo;
static GtkWidget *nr2_ae_check;
static GtkWidget *gain_combo;
static GtkWidget *npe_combo;
static GtkWidget *pos_combo;
static GtkWidget *trained_thr_spin;
static GtkWidget *trained_t2_spin;
static GtkWidget *nb2_mode_combo;
static GtkWidget *slew_spin;
static GtkWidget *lead_spin;
static GtkWidget *lag_spin;
static GtkWidget *thresh_spin;
static gint current_receiver_index = 0; // Track current receiver

static void snb_cb(GtkWidget *widget, gpointer data) {
  RADIO *radio = (RADIO *)data;
  RECEIVER *rx = radio->receiver[current_receiver_index];
  rx->snb = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  SetRXASNBARun(rx->channel, rx->snb);
  update_vfo(rx);
}

static void anf_cb(GtkWidget *widget, gpointer data) {
  RADIO *radio = (RADIO *)data;
  RECEIVER *rx = radio->receiver[current_receiver_index];
  rx->anf = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  SetRXAANFRun(rx->channel, rx->anf);
  update_vfo(rx);
}

static void nr_cb(GtkWidget *widget, gpointer data) {
  RADIO *radio = (RADIO *)data;
  RECEIVER *rx = radio->receiver[current_receiver_index];
  int val = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  rx->nr = (val == 1);  // NR
  rx->nr2 = (val == 2); // NR2
  SetRXAANRRun(rx->channel, rx->nr);
  SetRXAEMNRRun(rx->channel, rx->nr2);
  update_vfo(rx);
}

static void nb_cb(GtkWidget *widget, gpointer data) {
  RADIO *radio = (RADIO *)data;
  RECEIVER *rx = radio->receiver[current_receiver_index];
  int val = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  rx->nb = (val == 1);  // NB
  rx->nb2 = (val == 2); // NB2
  SetEXTANBRun(rx->channel, rx->nb);
  SetEXTNOBRun(rx->channel, rx->nb2);
  update_vfo(rx);
}

static void ae_cb(GtkWidget *widget, gpointer data) {
  RADIO *radio = (RADIO *)data;
  RECEIVER *rx = radio->receiver[current_receiver_index];
  rx->nr2_ae = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  SetRXAEMNRaeRun(rx->channel, rx->nr2_ae);
  update_vfo(rx);
}

static void pos_cb(GtkWidget *widget, gpointer data) {
  RADIO *radio = (RADIO *)data;
  RECEIVER *rx = radio->receiver[current_receiver_index];
  rx->nr_agc = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  SetRXAEMNRPosition(rx->channel, rx->nr_agc);
  update_vfo(rx);
}

static void gain_cb(GtkWidget *widget, gpointer data) {
  RADIO *radio = (RADIO *)data;
  RECEIVER *rx = radio->receiver[current_receiver_index];
  rx->nr2_gain_method = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  SetRXAEMNRgainMethod(rx->channel, rx->nr2_gain_method);
  update_vfo(rx);
}

static void npe_cb(GtkWidget *widget, gpointer data) {
  RADIO *radio = (RADIO *)data;
  RECEIVER *rx = radio->receiver[current_receiver_index];
  rx->nr2_npe_method = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  SetRXAEMNRnpeMethod(rx->channel, rx->nr2_npe_method);
  update_vfo(rx);
}

static void trained_thr_cb(GtkWidget *widget, gpointer data) {
  RADIO *radio = (RADIO *)data;
  RECEIVER *rx = radio->receiver[current_receiver_index];
  rx->nr2_trained_threshold = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
  SetRXAEMNRtrainZetaThresh(rx->channel, rx->nr2_trained_threshold);
  update_vfo(rx);
}

static void trained_t2_cb(GtkWidget *widget, gpointer data) {
  RADIO *radio = (RADIO *)data;
  RECEIVER *rx = radio->receiver[current_receiver_index];
  rx->nr2_trained_t2 = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
  SetRXAEMNRtrainT2(rx->channel, rx->nr2_trained_t2);
  update_vfo(rx);
}

static void mode_cb(GtkWidget *widget, gpointer data) {
  RADIO *radio = (RADIO *)data;
  RECEIVER *rx = radio->receiver[current_receiver_index];
  rx->nb2_mode = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  SetEXTNOBMode(rx->channel, rx->nb2_mode);
  update_vfo(rx);
}

static void slew_cb(GtkWidget *widget, gpointer data) {
  RADIO *radio = (RADIO *)data;
  RECEIVER *rx = radio->receiver[current_receiver_index];
  double gui_value = CLAMP(gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget)), 0.0, 0.1);
  rx->nb_tau = gui_value * 0.001;
  SetEXTANBTau(rx->channel, rx->nb_tau);
  SetEXTNOBTau(rx->channel, rx->nb_tau);
  update_vfo(rx);
}

static void lead_cb(GtkWidget *widget, gpointer data) {
  RADIO *radio = (RADIO *)data;
  RECEIVER *rx = radio->receiver[current_receiver_index];
  double gui_value = CLAMP(gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget)), 0.0, 0.1);
  rx->nb_advtime = gui_value * 0.001;
  SetEXTANBAdvtime(rx->channel, rx->nb_advtime);
  SetEXTNOBAdvtime(rx->channel, rx->nb_advtime);
  update_vfo(rx);
}
 
static void lag_cb(GtkWidget *widget, gpointer data) {
  RADIO *radio = (RADIO *)data;
  RECEIVER *rx = radio->receiver[current_receiver_index];
  rx->nb_hang = gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
  SetEXTANBHangtime(rx->channel, rx->nb_hang);
  SetEXTNOBHangtime(rx->channel, rx->nb_hang);
  update_vfo(rx);
}

static void thresh_cb(GtkWidget *widget, gpointer data) {
  RADIO *radio = (RADIO *)data;
  RECEIVER *rx = radio->receiver[current_receiver_index];
  double gui_value = CLAMP(gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget)), 15.0, 500.0);
  rx->nb_thresh = gui_value * 0.165;
  SetEXTANBThreshold(rx->channel, rx->nb_thresh);
  SetEXTNOBThreshold(rx->channel, rx->nb_thresh);
  update_vfo(rx);
}

static void nr_sel_cb(GtkWidget *widget, gpointer data) {
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    gtk_widget_show(nr_container);
    gtk_widget_hide(nb_container);
    gtk_widget_queue_draw(nr_container);  // Force redraw
  }
}

static void nb_sel_cb(GtkWidget *widget, gpointer data) {
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    gtk_widget_show(nb_container);
    gtk_widget_hide(nr_container);
    gtk_widget_queue_draw(nb_container);  // Force redraw
  }
}

static void receiver_changed_cb(GtkWidget *widget, gpointer data) {
  RADIO *radio = (RADIO *)data;
  current_receiver_index = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  RECEIVER *rx = radio->receiver[current_receiver_index];

  // Update all noise-related widgets
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(snb_check), rx->snb);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(anf_check), rx->anf);
  gtk_combo_box_set_active(GTK_COMBO_BOX(nr_combo), rx->nr ? 1 : rx->nr2 ? 2 : 0);
  gtk_combo_box_set_active(GTK_COMBO_BOX(nb_combo), rx->nb ? 1 : rx->nb2 ? 2 : 0);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(nr2_ae_check), rx->nr2_ae);
  gtk_combo_box_set_active(GTK_COMBO_BOX(pos_combo), rx->nr_agc);
  gtk_combo_box_set_active(GTK_COMBO_BOX(gain_combo), rx->nr2_gain_method);
  gtk_combo_box_set_active(GTK_COMBO_BOX(npe_combo), rx->nr2_npe_method);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(trained_thr_spin), rx->nr2_trained_threshold);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(trained_t2_spin), rx->nr2_trained_t2);
  gtk_combo_box_set_active(GTK_COMBO_BOX(nb2_mode_combo), rx->nb2_mode);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(slew_spin), rx->nb_tau);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(lead_spin), rx->nb_advtime);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(lag_spin), rx->nb_hang);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(thresh_spin), rx->nb_thresh / 0.165); // Scale to GUI range (0-30)
}

GtkWidget *create_noise_menu_dialog(RADIO *radio) {
    int i, row = 0;
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_homogeneous(GTK_GRID(grid), FALSE);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), FALSE);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);

    // Left Column: Receiver and Noise Reduction
    // Receiver Selection
    GtkWidget *receiver_frame = gtk_frame_new("Receiver");
    GtkWidget *receiver_grid = gtk_grid_new();
    gtk_grid_set_row_homogeneous(GTK_GRID(receiver_grid), FALSE);
    gtk_grid_set_column_homogeneous(GTK_GRID(receiver_grid), FALSE);
    gtk_grid_set_column_spacing(GTK_GRID(receiver_grid), 5);
    gtk_grid_set_row_spacing(GTK_GRID(receiver_grid), 5);
    gtk_container_add(GTK_CONTAINER(receiver_frame), receiver_grid);
    gtk_grid_attach(GTK_GRID(grid), receiver_frame, 0, row++, 1, 1);

    GtkWidget *receiver_combo = gtk_combo_box_text_new();
    for (i = 0; i < radio->discovered->supported_receivers; i++) {
        if (radio->receiver[i] != NULL) {
            char title[16];
            g_snprintf(title, sizeof(title), "RX-%d", radio->receiver[i]->channel);
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(receiver_combo), title);
        }
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(receiver_combo), 0);
    gtk_grid_attach(GTK_GRID(receiver_grid), receiver_combo, 0, 0, 1, 1);
    g_signal_connect(receiver_combo, "changed", G_CALLBACK(receiver_changed_cb), radio);

    // Noise Reduction Controls
    GtkWidget *noise_frame = gtk_frame_new("Noise Reduction");
    GtkWidget *noise_grid = gtk_grid_new();
    gtk_grid_set_row_homogeneous(GTK_GRID(noise_grid), FALSE);
    gtk_grid_set_column_homogeneous(GTK_GRID(noise_grid), FALSE);
    gtk_grid_set_column_spacing(GTK_GRID(noise_grid), 5);
    gtk_grid_set_row_spacing(GTK_GRID(noise_grid), 5);
    gtk_container_add(GTK_CONTAINER(noise_frame), noise_grid);
    gtk_grid_attach(GTK_GRID(grid), noise_frame, 0, row++, 1, 1);

    snb_check = gtk_check_button_new_with_label("SNB");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(snb_check), radio->receiver[0]->snb);
    gtk_grid_attach(GTK_GRID(noise_grid), snb_check, 0, 0, 1, 1);
    g_signal_connect(snb_check, "toggled", G_CALLBACK(snb_cb), radio);

    anf_check = gtk_check_button_new_with_label("ANF");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(anf_check), radio->receiver[0]->anf);
    gtk_grid_attach(GTK_GRID(noise_grid), anf_check, 1, 0, 1, 1);
    g_signal_connect(anf_check, "toggled", G_CALLBACK(anf_cb), radio);

    GtkWidget *nr_label = gtk_label_new("Noise Reduction:");
    gtk_grid_attach(GTK_GRID(noise_grid), nr_label, 0, 1, 1, 1);
    nr_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(nr_combo), "NONE");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(nr_combo), "NR");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(nr_combo), "NR2");
    gtk_combo_box_set_active(GTK_COMBO_BOX(nr_combo), radio->receiver[0]->nr ? 1 : radio->receiver[0]->nr2 ? 2 : 0);
    gtk_grid_attach(GTK_GRID(noise_grid), nr_combo, 1, 1, 1, 1);
    g_signal_connect(nr_combo, "changed", G_CALLBACK(nr_cb), radio);

    GtkWidget *nb_label = gtk_label_new("Noise Blanker:");
    gtk_grid_attach(GTK_GRID(noise_grid), nb_label, 0, 2, 1, 1);
    nb_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(nb_combo), "NONE");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(nb_combo), "NB");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(nb_combo), "NB2");
    gtk_combo_box_set_active(GTK_COMBO_BOX(nb_combo), radio->receiver[0]->nb ? 1 : radio->receiver[0]->nb2 ? 2 : 0);
    gtk_grid_attach(GTK_GRID(noise_grid), nb_combo, 1, 2, 1, 1);
    g_signal_connect(nb_combo, "changed", G_CALLBACK(nb_cb), radio);

    // Right Columns: NR and NB Settings Side by Side
    row = 0; // Reset row for right columns

    // NR Settings (Middle Column)
    nr_container = gtk_fixed_new();
    gtk_grid_attach(GTK_GRID(grid), nr_container, 1, row, 1, 1);
    GtkWidget *nr_grid = gtk_grid_new();
    gtk_grid_set_row_homogeneous(GTK_GRID(nr_grid), FALSE);
    gtk_grid_set_column_homogeneous(GTK_GRID(nr_grid), FALSE);
    gtk_grid_set_column_spacing(GTK_GRID(nr_grid), 5);
    gtk_grid_set_row_spacing(GTK_GRID(nr_grid), 5);
    gtk_container_add(GTK_CONTAINER(nr_container), nr_grid);

    GtkWidget *nr_title = gtk_label_new("NR Settings");
    gtk_grid_attach(GTK_GRID(nr_grid), nr_title, 0, 0, 2, 1);

    GtkWidget *gain_label = gtk_label_new("NR2 Gain Method:");
    gtk_grid_attach(GTK_GRID(nr_grid), gain_label, 0, 1, 1, 1);
    gain_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gain_combo), "Linear");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gain_combo), "Log");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gain_combo), "Gamma");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gain_combo), "Trained");
    gtk_combo_box_set_active(GTK_COMBO_BOX(gain_combo), radio->receiver[0]->nr2_gain_method);
    gtk_grid_attach(GTK_GRID(nr_grid), gain_combo, 1, 1, 1, 1);
    g_signal_connect(gain_combo, "changed", G_CALLBACK(gain_cb), radio);

    GtkWidget *npe_label = gtk_label_new("NR2 NPE Method:");
    gtk_grid_attach(GTK_GRID(nr_grid), npe_label, 0, 2, 1, 1);
    npe_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(npe_combo), "OSMS");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(npe_combo), "MMSE");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(npe_combo), "NSTAT");
    gtk_combo_box_set_active(GTK_COMBO_BOX(npe_combo), radio->receiver[0]->nr2_npe_method);
    gtk_grid_attach(GTK_GRID(nr_grid), npe_combo, 1, 2, 1, 1);
    g_signal_connect(npe_combo, "changed", G_CALLBACK(npe_cb), radio);

    GtkWidget *pos_label = gtk_label_new("NR/NR2/ANF Position:");
    gtk_grid_attach(GTK_GRID(nr_grid), pos_label, 0, 3, 1, 1);
    pos_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(pos_combo), "Pre AGC");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(pos_combo), "Post AGC");
    gtk_combo_box_set_active(GTK_COMBO_BOX(pos_combo), radio->receiver[0]->nr_agc);
    gtk_grid_attach(GTK_GRID(nr_grid), pos_combo, 1, 3, 1, 1);
    g_signal_connect(pos_combo, "changed", G_CALLBACK(pos_cb), radio);

    nr2_ae_check = gtk_check_button_new_with_label("NR2 Artifact Elimination");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(nr2_ae_check), radio->receiver[0]->nr2_ae);
    gtk_grid_attach(GTK_GRID(nr_grid), nr2_ae_check, 0, 4, 2, 1);
    g_signal_connect(nr2_ae_check, "toggled", G_CALLBACK(ae_cb), radio);

    GtkWidget *trained_thr_label = gtk_label_new("NR2 Trained Threshold:");
    gtk_grid_attach(GTK_GRID(nr_grid), trained_thr_label, 0, 5, 1, 1);
    trained_thr_spin = gtk_spin_button_new_with_range(-5.0, 5.0, 0.1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(trained_thr_spin), radio->receiver[0]->nr2_trained_threshold);
    gtk_grid_attach(GTK_GRID(nr_grid), trained_thr_spin, 1, 5, 1, 1);
    g_signal_connect(trained_thr_spin, "value-changed", G_CALLBACK(trained_thr_cb), radio);

    GtkWidget *trained_t2_label = gtk_label_new("NR2 Trained T2:");
    gtk_grid_attach(GTK_GRID(nr_grid), trained_t2_label, 0, 6, 1, 1);
    trained_t2_spin = gtk_spin_button_new_with_range(0.02, 0.3, 0.01);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(trained_t2_spin), radio->receiver[0]->nr2_trained_t2);
    gtk_grid_attach(GTK_GRID(nr_grid), trained_t2_spin, 1, 6, 1, 1);
    g_signal_connect(trained_t2_spin, "value-changed", G_CALLBACK(trained_t2_cb), radio);

    // NB Settings (Right Column)
    nb_container = gtk_fixed_new();
    gtk_grid_attach(GTK_GRID(grid), nb_container, 2, row, 1, 1); // Right of NR
    GtkWidget *nb_grid = gtk_grid_new();
    gtk_grid_set_row_homogeneous(GTK_GRID(nb_grid), FALSE);
    gtk_grid_set_column_homogeneous(GTK_GRID(nb_grid), FALSE);
    gtk_grid_set_column_spacing(GTK_GRID(nb_grid), 5);
    gtk_grid_set_row_spacing(GTK_GRID(nb_grid), 5);
    gtk_container_add(GTK_CONTAINER(nb_container), nb_grid);

    GtkWidget *nb_title = gtk_label_new("NB Settings");
    gtk_grid_attach(GTK_GRID(nb_grid), nb_title, 0, 0, 2, 1);

    GtkWidget *nb2_mode_label = gtk_label_new("NB2 Mode:");
    gtk_grid_attach(GTK_GRID(nb_grid), nb2_mode_label, 0, 1, 1, 1);
    nb2_mode_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(nb2_mode_combo), "Zero");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(nb2_mode_combo), "Sample&Hold");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(nb2_mode_combo), "Mean Hold");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(nb2_mode_combo), "Hold Sample");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(nb2_mode_combo), "Interpolate");
    gtk_combo_box_set_active(GTK_COMBO_BOX(nb2_mode_combo), radio->receiver[0]->nb2_mode);
    gtk_grid_attach(GTK_GRID(nb_grid), nb2_mode_combo, 1, 1, 1, 1);
    g_signal_connect(nb2_mode_combo, "changed", G_CALLBACK(mode_cb), radio);

    GtkWidget *slew_label = gtk_label_new("NB Slew Time (ms):");
    gtk_grid_attach(GTK_GRID(nb_grid), slew_label, 0, 2, 1, 1);
    slew_spin = gtk_spin_button_new_with_range(0.0, 0.1, 0.001);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(slew_spin), radio->receiver[0]->nb_tau * 1000.0);
    gtk_grid_attach(GTK_GRID(nb_grid), slew_spin, 1, 2, 1, 1);
    g_signal_connect(slew_spin, "value-changed", G_CALLBACK(slew_cb), radio);

    GtkWidget *lead_label = gtk_label_new("NB Lead Time (ms):");
    gtk_grid_attach(GTK_GRID(nb_grid), lead_label, 0, 3, 1, 1);
    lead_spin = gtk_spin_button_new_with_range(0.0, 0.1, 0.001);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(lead_spin), radio->receiver[0]->nb_advtime * 1000.0);
    gtk_grid_attach(GTK_GRID(nb_grid), lead_spin, 1, 3, 1, 1);
    g_signal_connect(lead_spin, "value-changed", G_CALLBACK(lead_cb), radio);

    GtkWidget *lag_label = gtk_label_new("NB Lag Time (ms):");
    gtk_grid_attach(GTK_GRID(nb_grid), lag_label, 0, 4, 1, 1);
    lag_spin = gtk_spin_button_new_with_range(0.0, 0.1, 0.001);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(lag_spin), radio->receiver[0]->nb_hang * 1000.0);
    gtk_grid_attach(GTK_GRID(nb_grid), lag_spin, 1, 4, 1, 1);
    g_signal_connect(lag_spin, "value-changed", G_CALLBACK(lag_cb), radio);

    GtkWidget *thresh_label = gtk_label_new("NB Threshold:");
    gtk_grid_attach(GTK_GRID(nb_grid), thresh_label, 0, 5, 1, 1);
    thresh_spin = gtk_spin_button_new_with_range(15.0, 500.0, 1.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(thresh_spin), radio->receiver[0]->nb_thresh / 0.165);
    gtk_grid_attach(GTK_GRID(nb_grid), thresh_spin, 1, 5, 1, 1);
    g_signal_connect(thresh_spin, "value-changed", G_CALLBACK(thresh_cb), radio);

    // Show all widgets
    gtk_widget_show_all(grid);

    return grid;
}