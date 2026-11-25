#define _POSIX_C_SOURCE 200809L
#include <gtk/gtk.h>
#include <cairo.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

typedef struct {
    double mouse_x;
    double mouse_y;
    int sock_fd;
    char sock_path[108];
    GtkWidget *drawing_area;
} EyeState;

static int parse_coords(const char *buf, double *out_x, double *out_y) {
    const char *comma = strchr(buf, ',');
    if (!comma || comma == buf) {
        return 0;
    }

    char x_str[64];
    size_t x_len = (size_t)(comma - buf);
    if (x_len >= sizeof(x_str)) {
        return 0;
    }
    memcpy(x_str, buf, x_len);
    x_str[x_len] = '\0';

    const char *y_str = comma + 1;
    if (*y_str == '\0') {
        return 0;
    }

    char *end_x = NULL;
    char *end_y = NULL;
    double x = g_ascii_strtod(x_str, &end_x);
    double y = g_ascii_strtod(y_str, &end_y);

    if (end_x == x_str || end_y == y_str || !std::isfinite(x) || !std::isfinite(y)) {
        return 0;
    }

    *out_x = x;
    *out_y = y;
    return 1;
}

static gboolean on_socket_data(GIOChannel *source, GIOCondition condition, gpointer user_data) {
    (void)source;
    EyeState *st = (EyeState *)user_data;

    if (condition & (G_IO_HUP | G_IO_ERR | G_IO_NVAL)) {
        return G_SOURCE_CONTINUE;
    }

    char buf[128];
    int updated = 0;

    while (1) {
        ssize_t n = recvfrom(st->sock_fd, buf, sizeof(buf) - 1, 0, NULL, NULL);
        if (n <= 0) {
            break;
        }
        buf[n] = '\0';

        double x = 0.0, y = 0.0;
        if (parse_coords(buf, &x, &y)) {
            st->mouse_x = x;
            st->mouse_y = y;
            updated = 1;
        }
    }

    if (updated && st->drawing_area) {
        gtk_widget_queue_draw(st->drawing_area);
    }

    return G_SOURCE_CONTINUE;
}

static void draw_single_eye(cairo_t *cr, double cx, double cy, double rx, double ry,
                            double dx, double dy, double reach_radius) {
    if (rx <= 4.0 || ry <= 4.0) {
        return;
    }

    const double stroke_w = 3.0;
    cairo_save(cr);

    // Göz akı (klasik xeyes elipsi)
    cairo_save(cr);
    cairo_translate(cr, cx, cy);
    cairo_scale(cr, rx, ry);
    cairo_arc(cr, 0.0, 0.0, 1.0, 0.0, 2.0 * G_PI);
    cairo_restore(cr);

    cairo_set_source_rgba(cr, 0.98, 0.98, 0.99, 0.96);
    cairo_fill_preserve(cr);

    cairo_set_source_rgba(cr, 0.10, 0.10, 0.12, 1.0);
    cairo_set_line_width(cr, stroke_w);
    cairo_stroke(cr);

    // Göz bebeği yarıçapı ve güvenli iç elips yarıçapları (a, b)
    double pupil_r = fmin(rx, ry) * 0.26;
    double pad = pupil_r + stroke_w;
    double a = fmax(rx - pad, 0.0);
    double b = fmax(ry - pad, 0.0);

    double inv_reach = (reach_radius > 1.0) ? (1.0 / reach_radius) : 1.0;
    double nx = dx * inv_reach;
    double ny = dy * inv_reach;

    double mag = hypot(nx, ny);
    if (mag > 1.0) {
        nx /= mag;
        ny /= mag;
    }

    double px = cx + nx * a;
    double py = cy + ny * b;

    cairo_arc(cr, px, py, pupil_r, 0.0, 2.0 * G_PI);
    cairo_set_source_rgba(cr, 0.07, 0.07, 0.09, 1.0);
    cairo_fill(cr);

    cairo_restore(cr);
}

static gboolean on_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    EyeState *st = (EyeState *)user_data;

    int w = gtk_widget_get_allocated_width(widget);
    int h = gtk_widget_get_allocated_height(widget);

    // Şeffaf arka plan (compositor destekliyorsa)
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    double eye_rx = w * 0.21;
    double eye_ry = h * 0.38;
    double left_cx = w * 0.28;
    double right_cx = w * 0.72;
    double cy = h * 0.50;

    GdkDisplay *display = gdk_display_get_default();
    double mid_x = w * 0.5;
    double mid_y = h * 0.5;
    double reach_radius = fmax(fmin((double)w, (double)h) * 0.45, 60.0);

    if (display) {
        GdkMonitor *mon = gdk_display_get_primary_monitor(display);
        if (!mon && gdk_display_get_n_monitors(display) > 0) {
            mon = gdk_display_get_monitor(display, 0);
        }
        if (mon) {
            GdkRectangle g;
            gdk_monitor_get_geometry(mon, &g);
            if (g.width > w || g.height > h) {
                mid_x = g.x + g.width * 0.5;
                mid_y = g.y + g.height * 0.5;
                reach_radius = fmin((double)g.width, (double)g.height) * 0.45;
            }
        }
    }

    double dx = st->mouse_x - mid_x;
    double dy = st->mouse_y - mid_y;

    draw_single_eye(cr, left_cx, cy, eye_rx, eye_ry, dx, dy, reach_radius);
    draw_single_eye(cr, right_cx, cy, eye_rx, eye_ry, dx, dy, reach_radius);

    return TRUE;
}

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);

    static EyeState state;
    state.mouse_x = 0.0;
    state.mouse_y = 0.0;
    state.sock_fd = -1;
    state.drawing_area = NULL;

    snprintf(state.sock_path, sizeof(state.sock_path), "/tmp/hypreyes-%u.sock", (unsigned)getuid());
    unlink(state.sock_path);

    state.sock_fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (state.sock_fd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", state.sock_path);

    if (bind(state.sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(state.sock_fd);
        return 1;
    }

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "hypreyes");
    gtk_window_set_default_size(GTK_WINDOW(win), 320, 180);
    gtk_widget_set_app_paintable(win, TRUE);

    GdkScreen *screen = gtk_widget_get_screen(win);
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
    if (visual && gdk_screen_is_composited(screen)) {
        gtk_widget_set_visual(win, visual);
    }

    GtkWidget *area = gtk_drawing_area_new();
    state.drawing_area = area;
    g_signal_connect(G_OBJECT(area), "draw", G_CALLBACK(on_draw), &state);
    g_signal_connect(G_OBJECT(win), "destroy", G_CALLBACK(gtk_main_quit), NULL);

    gtk_container_add(GTK_CONTAINER(win), area);

    GIOChannel *chan = g_io_channel_unix_new(state.sock_fd);
    g_io_add_watch(chan, (GIOCondition)(G_IO_IN | G_IO_HUP | G_IO_ERR), on_socket_data, &state);
    g_io_channel_unref(chan);

    gtk_widget_show_all(win);
    gtk_main();

    close(state.sock_fd);
    unlink(state.sock_path);
    return 0;
}
