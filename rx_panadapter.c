/* Copyright (C) 2018 - John Melton, G0ORX/N6LYT
 * ... (license unchanged) ...
 */

#include <gtk/gtk.h>
#include <epoxy/gl.h>
#include <math.h>
#include <stdlib.h>
#include <wdsp.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "bpsk.h"
#include "agc.h"
#include "mode.h"
#include "filter.h"
#include "band.h"
#include "receiver.h"
#include "rx_panadapter.h"
#include "transmitter.h"
#include "wideband.h"
#include "discovered.h"
#include "adc.h"
#include "dac.h"
#include "radio.h"
#include "main.h"
#include "vfo.h"
#include "subrx.h"

#define LINE_WIDTH 0.5

int signal_vertices_size = -1;
float *signal_vertices = NULL;

static const double dashed2[] = {2.0, 2.0};
static int len2 = sizeof(dashed2) / sizeof(dashed2[0]);

//static gboolean first_time = TRUE;

static void panadapter_realize_cb(GtkWidget *widget, gpointer data) {
    RECEIVER *rx = (RECEIVER *)data;
    GdkWindow *window = gtk_widget_get_window(widget);
    if (window && GDK_IS_WINDOW(window)) {
        rx->panadapter_surface = gdk_window_create_similar_surface(window, CAIRO_CONTENT_COLOR, 1, 1);
        if (rx->panadapter_surface) {
            cairo_t *cr = cairo_create(rx->panadapter_surface);
            cairo_set_source_rgb(cr, 0.1, 0.1, 0.1); // Dark background
            cairo_paint(cr);
            cairo_destroy(cr);
            g_print("panadapter_realize_cb: Created panadapter_surface for rx=%d\n", rx->channel);
        } else {
            g_warning("panadapter_realize_cb: Failed to create panadapter_surface for rx=%d", rx->channel);
        }
    } else {
        g_warning("panadapter_realize_cb: Invalid window for rx=%d", rx->channel);
    }
}

static gboolean resize_timeout(void *data) {
  RECEIVER *rx = (RECEIVER *)data;
  PanadapterCache *px = &rx->panadapter_cache;

  g_mutex_lock(&rx->mutex);
  rx->panadapter_width = rx->panadapter_resize_width;
  rx->panadapter_height = rx->panadapter_resize_height;
  rx->pixels = rx->panadapter_width * rx->zoom;

  receiver_init_analyzer(rx);

  if (rx->panadapter_surface) {
    cairo_surface_destroy(rx->panadapter_surface);
    rx->panadapter_surface = NULL;
  }

  if (px->static_surface) {
    cairo_surface_destroy(px->static_surface);
    px->static_surface = NULL;
  }
  px->width = 0;
  px->height = 0;

  if (rx->panadapter != NULL) {
    rx->panadapter_surface = gdk_window_create_similar_surface(gtk_widget_get_window(rx->panadapter),
                                                              CAIRO_CONTENT_COLOR,
                                                              rx->panadapter_width,
                                                              rx->panadapter_height);
    cairo_t *cr = cairo_create(rx->panadapter_surface);
    cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
    cairo_paint(cr);
    cairo_destroy(cr);
  } else {
    fprintf(stderr, "resize_timeout: rx->panadapter is NULL\n");
  }

  rx->panadapter_resize_timer = -1;
  update_vfo(rx);
  g_mutex_unlock(&rx->mutex);
  return FALSE;
}

#ifdef OPENGL
GLuint gl_program, gl_vao;

const GLchar *vert_src = "\n"
                         "#version 330                                  \n"
                         "#extension GL_ARB_explicit_attrib_location: enable  \n"
                         "                                              \n"
                         "layout(location = 0) in vec2 in_position;     \n"
                         "                                              \n"
                         "void main()                                   \n"
                         "{                                             \n"
                         "  gl_Position *ftransform;                   \n"
                         "}                                             \n";

const GLchar *frag_src = "\n"
                         "void main (void)                              \n"
                         "{                                             \n"
                         "  gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);    \n"
                         "}                                             \n";

static gboolean rx_panadapter_render(GtkGLArea *area, GdkGLContext *context) {
  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT);

  if (signal_vertices_size != -1) {
    glLineWidth(2.0);
    glColor3f(1.0, 1.0, 0.0);
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, signal_vertices_size * sizeof(float) * 2, signal_vertices, GL_STREAM_DRAW);

    glUseProgram(gl_program);
    glBindVertexArray(gl_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDrawArrays(GL_LINE_STRIP, 0, signal_vertices_size);
    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
  } else {
    fprintf(stderr, "rx_panadapter_render: signal_vertices_size is -1, skipping draw\n");
  }

  return TRUE;
}

static void rx_panadapter_realize(GtkGLArea *area) {
  gtk_gl_area_make_current(area);

  if (gtk_gl_area_get_error(area) != NULL) {
    fprintf(stderr, "rx_panadapter_realize: gtk_gl_area_get_error returned non-NULL\n");
    return;
  }

  GLuint frag_shader, vert_shader;
  frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
  vert_shader = glCreateShader(GL_VERTEX_SHADER);

  glShaderSource(frag_shader, 1, &frag_src, NULL);
  glShaderSource(vert_shader, 1, &vert_src, NULL);

  glCompileShader(frag_shader);
  glCompileShader(vert_shader);

  gl_program = glCreateProgram();
  glAttachShader(gl_program, frag_shader);
  glAttachShader(gl_program, vert_shader);
  glLinkProgram(gl_program);

  glGenVertexArrays(1, &gl_vao);
  glBindVertexArray(gl_vao);
}
#endif

void rx_panadapter_resize(GtkGLArea *area, gint width, gint height, gpointer data) {
  RECEIVER *rx = (RECEIVER *)data;
  if (width != rx->panadapter_width || height != rx->panadapter_height) {
    rx->panadapter_resize_width = width;
    rx->panadapter_resize_height = height;
    if (rx->panadapter_resize_timer != -1) {
      g_source_remove(rx->panadapter_resize_timer);
    }
    rx->panadapter_resize_timer = g_timeout_add(250, resize_timeout, (gpointer)rx);
  }
}

static gboolean rx_panadapter_configure_event_cb(GtkWidget *widget, GdkEventConfigure *event, gpointer data) {
  RECEIVER *rx = (RECEIVER *)data;
  gint width = gtk_widget_get_allocated_width(widget);
  gint height = gtk_widget_get_allocated_height(widget);
  if (width != rx->panadapter_width || height != rx->panadapter_height) {
    rx->panadapter_resize_width = width;
    rx->panadapter_resize_height = height;
    if (rx->panadapter_resize_timer != -1) {
      g_source_remove(rx->panadapter_resize_timer);
    }
    rx->panadapter_resize_timer = g_timeout_add(250, resize_timeout, (gpointer)rx);
  }
  return TRUE;
}

static gboolean rx_panadapter_draw_cb(GtkWidget *widget, cairo_t *cr, gpointer data) {
  RECEIVER *rx = (RECEIVER *)data;
  if (rx->panadapter_surface != NULL) {
    cairo_set_source_surface(cr, rx->panadapter_surface, 0.0, 0.0);
    cairo_paint(cr);
  } else {
    fprintf(stderr, "rx_panadapter_draw_cb: panadapter_surface is NULL, skipping paint\n");
  }
  return TRUE;
}

static void panadapter_destroy_cb(GtkWidget *widget, gpointer data) {
  RECEIVER *rx = (RECEIVER *)data;
  PanadapterCache *px = &rx->panadapter_cache;
  if (px->static_surface) {
    cairo_surface_destroy(px->static_surface);
    px->static_surface = NULL;
  }
}

GtkWidget *create_rx_panadapter(RECEIVER *rx) {
  GtkWidget *panadapter;
  PanadapterCache *px = &rx->panadapter_cache;

  rx->panadapter_width = 0;
  rx->panadapter_height = 0;
  rx->panadapter_surface = NULL;
  rx->panadapter_resize_timer = -1;

  px->static_surface = NULL;
  px->width = 0;
  px->height = 0;
  px->zoom = -1;
  px->frequency_a = 0;
  px->sample_rate = 0;
  px->band_a = -1;
  px->panadapter_high = 0;
  px->panadapter_low = 0;
  px->panadapter_step = 0;

  panadapter = gtk_drawing_area_new();
  g_signal_connect(panadapter, "realize", G_CALLBACK(panadapter_realize_cb), rx);
  g_signal_connect(panadapter, "destroy", G_CALLBACK(panadapter_destroy_cb), rx);
  g_signal_connect(panadapter, "configure-event", G_CALLBACK(rx_panadapter_configure_event_cb), rx);
  g_signal_connect(panadapter, "draw", G_CALLBACK(rx_panadapter_draw_cb), rx);
  g_signal_connect(panadapter, "motion-notify-event", G_CALLBACK(receiver_motion_notify_event_cb), rx);
  g_signal_connect(panadapter, "button-press-event", G_CALLBACK(receiver_button_press_event_cb), rx);
  g_signal_connect(panadapter, "button-release-event", G_CALLBACK(receiver_button_release_event_cb), rx);
  g_signal_connect(panadapter, "scroll_event", G_CALLBACK(receiver_scroll_event_cb), rx);

  gtk_widget_set_events(panadapter, gtk_widget_get_events(panadapter)
                                   | GDK_BUTTON_PRESS_MASK
                                   | GDK_BUTTON_RELEASE_MASK
                                   | GDK_BUTTON1_MOTION_MASK
                                   | GDK_SCROLL_MASK
                                   | GDK_POINTER_MOTION_MASK
                                   | GDK_POINTER_MOTION_HINT_MASK);

  rx->panadapter = panadapter;
  return panadapter;
}

static void draw_static_elements(RECEIVER *rx, cairo_t *cr) {
  int display_width = gtk_widget_get_allocated_width(rx->panadapter);
  int display_height = gtk_widget_get_allocated_height(rx->panadapter);
  double dbm_per_line = (double)display_height / ((double)rx->panadapter_high - (double)rx->panadapter_low);
  char temp[32];

  cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
  cairo_rectangle(cr, 0, 0, display_width, display_height);
  cairo_fill(cr);

  cairo_set_line_width(cr, LINE_WIDTH);
  cairo_set_dash(cr, dashed2, len2, 0);
  for (int i = rx->panadapter_high; i >= rx->panadapter_low; i--) {
    SetColour(cr, DARK_LINES);
    int mod = abs(i) % rx->panadapter_step;
    if (mod == 0) {
      double y = (double)(rx->panadapter_high - i) * dbm_per_line;
      cairo_move_to(cr, 0.0, y);
      cairo_line_to(cr, (double)display_width, y);
      sprintf(temp, " %d", i);
      cairo_move_to(cr, 5, y - 1);
      cairo_show_text(cr, temp);
    }
  }
  cairo_stroke(cr);
  cairo_set_dash(cr, 0, 0, 0);

  long long frequency = rx->frequency_a;
  long long half = (long long)rx->sample_rate / 2LL;
  long long min_display = frequency - (half / (long long)rx->zoom);
  long long f1 = frequency - half + (long long)(rx->hz_per_pixel * rx->pan);
  if (rx->mode_a == CWU) f1 -= radio->cw_keyer_sidetone_frequency;
  else if (rx->mode_a == CWL) f1 += radio->cw_keyer_sidetone_frequency;
  long long divisor1 = 20000, divisor2 = 5000;
  long long factor = (long long)(rx->sample_rate / 48000);
  if (factor > 10LL) factor = 10LL;
  switch (rx->zoom) {
    case 1: case 2: case 3:
      divisor1 = 5000LL * factor;
      divisor2 = 1000LL * factor;
      break;
    case 4: case 5: case 6:
      divisor1 = 1000LL * factor;
      divisor2 = 500LL * factor;
      break;
    case 7: case 8:
      divisor1 = 1000LL * factor;
      divisor2 = 200LL * factor;
      break;
  }
  long long f2 = (f1 / divisor2) * divisor2;
  int x;
  do {
    x = (int)((f2 - f1) / rx->hz_per_pixel);
    if (x > 70) {
      if ((f2 % divisor1) == 0LL) {
        SetColour(cr, DARK_LINES);
        cairo_move_to(cr, (double)x, 0);
        cairo_line_to(cr, (double)x, (double)display_height - 20);
        SetColour(cr, TEXT_B);
        cairo_line_to(cr, (double)x, (double)display_height);
        cairo_stroke(cr);
        sprintf(temp, "%0lld.%03lld", f2 / 1000000, (f2 % 1000000) / 1000);
        cairo_text_extents_t extents;
        cairo_text_extents(cr, temp, &extents);
        cairo_move_to(cr, (double)x - (extents.width / 2.0), display_height - 6);
        cairo_show_text(cr, temp);
      } else if ((f2 % divisor2) == 0LL) {
        SetColour(cr, DARK_LINES);
        cairo_set_dash(cr, dashed2, len2, 0);
        cairo_move_to(cr, (double)x, 0);
        cairo_line_to(cr, (double)x, (double)display_height - 20);
        cairo_stroke(cr);
        cairo_set_dash(cr, 0, 0, 0);
      }
    }
    f2 += divisor2;
  } while (x < display_width);

  BAND *band = band_get_band(rx->band_a);
  if (band->frequencyMin != 0LL && rx->band_a != band60) {
    SetColour(cr, WARNING);
    cairo_set_line_width(cr, 2.0);
    long long max_display = min_display + (long long)(display_width * rx->hz_per_pixel);
    if (min_display < band->frequencyMin && max_display > band->frequencyMin) {
      int i = (int)(((double)band->frequencyMin - (double)min_display) / rx->hz_per_pixel);
      cairo_move_to(cr, (double)i, 0.0);
      cairo_line_to(cr, (double)i, (double)display_height - 20);
      cairo_stroke(cr);
    }
    if (min_display < band->frequencyMax && max_display > band->frequencyMax) {
      int i = (int)(((double)band->frequencyMax - (double)min_display) / rx->hz_per_pixel);
      cairo_move_to(cr, (double)i, 0.0);
      cairo_line_to(cr, (double)i, (double)display_height - 20);
      cairo_stroke(cr);
    }
    cairo_set_line_width(cr, LINE_WIDTH);
  }
}

// Define maximum number of points to process (same as panadapter for consistency)
#define MAX_PLOT_POINTS 640

void update_rx_panadapter(RECEIVER *rx, gboolean running) {
    PanadapterCache *px = &rx->panadapter_cache;

    int display_width = gtk_widget_get_allocated_width(rx->panadapter);
    int display_height = gtk_widget_get_allocated_height(rx->panadapter);
    double dbm_per_line = (double)(display_height - 20) / ((double)rx->panadapter_high - (double)rx->panadapter_low); // Adjusted for bottom margin
    int offset = rx->pan;
    float *samples = rx->pixel_samples;
    cairo_text_extents_t extents;
    char temp[32];

    if (rx->panadapter_surface == NULL || display_height <= 1) {
        fprintf(stderr, "update_rx_panadapter: early exit: surface=%p height=%d\n",
                (void *)rx->panadapter_surface, display_height);
        return;
    }
    if (samples == NULL) {
        fprintf(stderr, "update_rx_panadapter: early exit: samples=NULL\n");
        return;
    }
    samples[display_width - 1 + offset] = -200;

    gboolean redraw_static = FALSE;
    if (!px->static_surface ||
        px->width != display_width ||
        px->height != display_height ||
        px->zoom != rx->zoom ||
        px->frequency_a != rx->frequency_a ||
        px->sample_rate != rx->sample_rate ||
        px->band_a != rx->band_a ||
        px->panadapter_high != rx->panadapter_high ||
        px->panadapter_low != rx->panadapter_low ||
        px->panadapter_step != rx->panadapter_step) {
        redraw_static = TRUE;
        px->width = display_width;
        px->height = display_height;
        px->zoom = rx->zoom;
        px->frequency_a = rx->frequency_a;
        px->sample_rate = rx->sample_rate;
        px->band_a = rx->band_a;
        px->panadapter_high = rx->panadapter_high;
        px->panadapter_low = rx->panadapter_low;
        px->panadapter_step = rx->panadapter_step;
    }

    if (redraw_static) {
        if (px->static_surface) {
            cairo_surface_destroy(px->static_surface);
        }
        px->static_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, display_width, display_height);
        cairo_t *cr_static = cairo_create(px->static_surface);
        cairo_select_font_face(cr_static, "Noto Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr_static, 12);
        draw_static_elements(rx, cr_static);
        cairo_destroy(cr_static);
    }

    cairo_t *cr = cairo_create(rx->panadapter_surface);
    cairo_select_font_face(cr, "Noto Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12);
    cairo_set_line_width(cr, LINE_WIDTH);

    if (!running) {
        SetColour(cr, WARNING);
        cairo_set_font_size(cr, 18);
        cairo_move_to(cr, display_width / 2, display_height / 2);
        cairo_show_text(cr, "No data - receiver thread exited");
        cairo_destroy(cr);
        gtk_widget_queue_draw(rx->panadapter);
        fprintf(stderr, "update_rx_panadapter: receiver not running, drew warning\n");
        return;
    }

    cairo_set_source_surface(cr, px->static_surface, 0, 0);
    cairo_paint(cr);

    double attenuation = radio->adc[rx->adc].attenuation;
    if (radio->discovered->device == DEVICE_HERMES_LITE2) attenuation = -attenuation;

    // Create pixel buffer for signal plot
    int plot_points = MIN(display_width, MAX_PLOT_POINTS);
    double sample_step = (double)display_width / plot_points;
    double x_step = (double)display_width / (plot_points - 1);

    // Allocate buffer for y-coordinates and outline path
    double *y_values = g_new(double, plot_points);
    double S9_dbm = rx->frequency_a > 30000000LL ? -93 : -73;
    double S9_y = (rx->panadapter_high - S9_dbm) * dbm_per_line; // Y-coordinate of S9
    double S9 = 1.0 - (S9_y / (double)(rx->panadapter_height - 20)); // Normalize to [0, 1]

    // Compute y-coordinates
    for (int i = 0; i < plot_points; i++) {
        double sample_idx = i * sample_step + offset;
        int idx_floor = (int)sample_idx;
        double frac = sample_idx - idx_floor;
        double s2;
        if (idx_floor + 1 < rx->pixels) {
            double s_floor = (double)samples[idx_floor] + attenuation + radio->panadapter_calibration;
            double s_ceil = (double)samples[idx_floor + 1] + attenuation + radio->panadapter_calibration;
            s2 = s_floor + (s_ceil - s_floor) * frac;
        } else {
            s2 = (double)samples[idx_floor] + attenuation + radio->panadapter_calibration;
        }
        s2 = (rx->panadapter_high - s2) * dbm_per_line;
        if (s2 >= rx->panadapter_height - 20) s2 = rx->panadapter_height - 20;
        if (s2 < 0) s2 = 0;
        y_values[i] = s2;
    }

    // Create image surface for filled area
    cairo_surface_t *plot_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, display_width, display_height);
    unsigned char *plot_data = cairo_image_surface_get_data(plot_surface);
    int plot_stride = cairo_image_surface_get_stride(plot_surface);

    // Define gradient stops
    struct GradientStop {
        double t;
        double r, g, b, a;
    };
    struct GradientStop stops[] = {
        {0.0, 0.0, 0.0, 0.5, 0.5},           // Dark blue
        {S9 / 3.0, 0.0, 0.8, 0.6, 0.5},       // Green/cyan
        {(S9 / 3.0) * 2.0, 1.0, 1.0, 0.0, 0.5}, // Yellow
        {S9, 1.0, 0.0, 0.0, 0.5}              // Red
    };
    int num_stops = sizeof(stops) / sizeof(stops[0]);

    // Fill image surface with signal plot
    for (int x = 0; x < display_width; x++) {
        double t = (double)x / x_step;
        int i = (int)t;
        double frac = t - i;
        if (i + 1 >= plot_points) {
            i = plot_points - 1;
            frac = 0.0;
        }
        double y = (1.0 - frac) * y_values[i] + frac * y_values[i + 1]; // Interpolate y
        int y_top = (int)y;
        if (y_top < 0) y_top = 0;
        if (y_top >= display_height - 20) y_top = display_height - 20;

        // Fill from y_top to bottom
        for (int y_pixel = y_top; y_pixel < display_height - 20; y_pixel++) {
            unsigned char *pixel = plot_data + y_pixel * plot_stride + x * 4; // ARGB32
            if (rx->panadapter_gradient) {
                // Compute gradient_t: map y_pixel to [0, 1]
                double gradient_t = 1.0 - ((double)y_pixel / (double)(rx->panadapter_height - 20));
                if (gradient_t < 0.0) gradient_t = 0.0;
                if (gradient_t > 1.0) gradient_t = 1.0;

                

                // Interpolate between gradient stops
                double r = 0.0, g = 0.0, b = 0.0, a = 0.5;
                for (int s = 0; s < num_stops - 1; s++) {
                    if (gradient_t >= stops[s].t && gradient_t <= stops[s + 1].t) {
                        double t_norm = (gradient_t - stops[s].t) / (stops[s + 1].t - stops[s].t);
                        r = (1.0 - t_norm) * stops[s].r + t_norm * stops[s + 1].r;
                        g = (1.0 - t_norm) * stops[s].g + t_norm * stops[s + 1].g;
                        b = (1.0 - t_norm) * stops[s].b + t_norm * stops[s + 1].b;
                        a = (1.0 - t_norm) * stops[s].a + t_norm * stops[s + 1].a;
                        break;
                    }
                }
                // Handle edge case: gradient_t >= S9
                if (gradient_t >= stops[num_stops - 1].t) {
                    r = stops[num_stops - 1].r;
                    g = stops[num_stops - 1].g;
                    b = stops[num_stops - 1].b;
                    a = stops[num_stops - 1].a;
                }

                
                pixel[2] = (unsigned char)(r * 255); // R
                pixel[1] = (unsigned char)(g * 255); // G
                pixel[0] = (unsigned char)(b * 255); // B
                pixel[3] = (unsigned char)(a * 255); // A
            } else {
                pixel[2] = 255; // R
                pixel[1] = 255; // G
                pixel[0] = 255; // B
                pixel[3] = 128; // A (0.5)
            }
        }
    }

    cairo_surface_mark_dirty(plot_surface);

    // Composite plot surface
    if (rx->panadapter_filled) {
        cairo_set_source_surface(cr, plot_surface, 0, 0);
        cairo_paint(cr);
    }

    // Draw outline
    cairo_move_to(cr, 0.0, y_values[0]);
    for (int i = 1; i < plot_points; i++) {
        double x = i == plot_points - 1 ? (double)display_width : i * x_step;
        cairo_line_to(cr, x, y_values[i]);
    }
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.5);
    cairo_stroke(cr);

    g_free(y_values);
    cairo_surface_destroy(plot_surface);

    // Draw filter rectangle
    cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.50);
    double filter_left = ((double)rx->pixels / 2.0) - (double)rx->pan + (((double)rx->filter_low_a + rx->ctun_offset) / rx->hz_per_pixel);
    double filter_right = ((double)rx->pixels / 2.0) - (double)rx->pan + (((double)rx->filter_high_a + rx->ctun_offset) / rx->hz_per_pixel);
    cairo_rectangle(cr, filter_left, 10.0, filter_right - filter_left, (double)display_height);
    cairo_fill(cr);

    double cursor;
    if (rx->mode_a == CWU || rx->mode_a == CWL) {
        SetColour(cr, TEXT_B);
        cursor = filter_left + ((filter_right - filter_left) / 2.0);
        cairo_move_to(cr, cursor, 10.0);
        cairo_line_to(cr, cursor, (double)display_height - 20);
        cairo_stroke(cr);
    } else {
        SetColour(cr, TEXT_A);
        cursor = (double)(rx->pixels / 2.0) - (double)rx->pan + (rx->ctun_offset / rx->hz_per_pixel);
        cairo_move_to(cr, cursor, 10.0);
        cairo_line_to(cr, cursor, (double)display_height - 20.0);
        cairo_stroke(cr);
    }

    long long af = rx->ctun ? rx->ctun_frequency : rx->frequency_a;
    sprintf(temp, "%5lld.%03lld.%03lld", af / (long long)1000000, (af % (long long)1000000) / (long long)1000, af % (long long)1000);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_text_extents(cr, temp, &extents);
    cairo_move_to(cr, cursor - (extents.width / 2.0), 10);
    cairo_show_text(cr, temp);

    if (rx->split == SPLIT_ON || rx->subrx_enable) {
        double diff = (double)(rx->frequency_b - rx->frequency_a) / rx->hz_per_pixel;
        if (rx->mode_a == CWU || rx->mode_a == CWL) {
            SetColour(cr, WARNING);
            cursor = filter_left + ((filter_right - filter_left) / 2.0) + diff;
            cairo_move_to(cr, cursor, 20.0);
            cairo_line_to(cr, cursor, (double)display_height - 20);
            cairo_stroke(cr);
        } else if (rx->subrx_enable) {
            SetColour(cr, WARNING);
            cursor = (double)(rx->pixels / 2.0) - (double)rx->pan + diff;
            cairo_move_to(cr, cursor, 20.0);
            cairo_line_to(cr, cursor, (double)display_height - 20);
            cairo_stroke(cr);
            cairo_set_source_rgba(cr, 0.7, 0.7, 0.7, 0.50);
#ifdef USE_VFO_B_MODE_AND_FILTER
            double filter_left_b = ((double)rx->pixels / 2.0) - (double)rx->pan + (((double)rx->filter_low_b) / rx->hz_per_pixel) + diff;
            double filter_right_b = ((double)rx->pixels / 2.0) - (double)rx->pan + (((double)rx->filter_high_b) / rx->hz_per_pixel) + diff;
#else
            double filter_left_b = ((double)rx->pixels / 2.0) - (double)rx->pan + (((double)rx->filter_low_a) / rx->hz_per_pixel) + diff;
            double filter_right_b = ((double)rx->pixels / 2.0) - (double)rx->pan + (((double)rx->filter_high_a) / rx->hz_per_pixel) + diff;
#endif
            cairo_rectangle(cr, filter_left_b, 20.0, filter_right_b - filter_left_b, (double)display_height - 20);
            cairo_fill(cr);
        }
        sprintf(temp, "%5lld.%03lld.%03lld", rx->frequency_b / (long long)1000000, (rx->frequency_b % (long long)1000000) / (long long)1000, rx->frequency_b % (long long)1000);
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_text_extents(cr, temp, &extents);
        cairo_move_to(cr, cursor - (extents.width / 2.0), 20);
        cairo_show_text(cr, temp);
    }

    if (rx->zoom != 1) {
        int pan_x = (int)((double)rx->pan / (double)rx->zoom);
        int pan_width = (int)((double)rx->panadapter_width / (double)rx->zoom);
        cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        cairo_rectangle(cr, pan_x, rx->panadapter_height - 4, pan_width, rx->panadapter_height);
        cairo_fill(cr);
    }

    if (rx->agc != AGC_OFF && rx->panadapter_agc_line) {
        double hang, thresh;
        GetRXAAGCHangLevel(rx->channel, &hang);
        GetRXAAGCThresh(rx->channel, &thresh, 4096.0, (double)rx->sample_rate);
        double knee_y = thresh + radio->adc[rx->adc].attenuation + radio->panadapter_calibration;
        knee_y = floor((rx->panadapter_high - knee_y) * dbm_per_line);
        double hang_y = hang + radio->adc[rx->adc].attenuation + radio->panadapter_calibration;
        hang_y = floor((rx->panadapter_high - hang_y) * dbm_per_line);
        if (rx->agc != AGC_MEDIUM && rx->agc != AGC_FAST) {
            SetColour(cr, TEXT_A);
            cairo_move_to(cr, 80.0, hang_y - 8.0);
            cairo_rectangle(cr, 80.0, hang_y - 8.0, 8.0, 8.0);
            cairo_fill(cr);
            cairo_move_to(cr, 80.0, hang_y);
            cairo_line_to(cr, (double)display_width - 80.0, hang_y);
            cairo_stroke(cr);
            cairo_move_to(cr, 88.0, hang_y);
            cairo_show_text(cr, "-H");
        }
        SetColour(cr, TEXT_C);
        cairo_move_to(cr, 80.0, knee_y - 8.0);
        cairo_rectangle(cr, 80.0, knee_y - 8.0, 8.0, 8.0);
        cairo_fill(cr);
        cairo_move_to(cr, 80.0, knee_y);
        cairo_line_to(cr, (double)display_width - 80.0, knee_y);
        cairo_stroke(cr);
        cairo_move_to(cr, 88.0, knee_y);
        cairo_show_text(cr, "-G");
    }

    cairo_destroy(cr);
    gtk_widget_queue_draw(rx->panadapter);
}