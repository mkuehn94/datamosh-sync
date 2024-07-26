// datamosh-sync GUI
// plain but it works
// just sliders and buttons, nothing fancy
//
// TODO: live preview would be cool but thats a lot of work
// TODO: presets

#include <gtk/gtk.h>
#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <vector>
#include <cstring>
#include <cmath>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include "arrange.h"

using namespace std;

// global widgets we need to read from callback
GtkWidget* label_status;
GtkWidget* entry_input;

GtkWidget* slider_glitch;
GtkWidget* slider_jpeg;
GtkWidget* slider_mblur;
GtkWidget* slider_color;
GtkWidget* slider_shuffle;
GtkWidget* slider_beatsync;

GtkWidget* slider_rot;
GtkWidget* slider_warp;
GtkWidget* slider_quake;
GtkWidget* slider_spectral;
GtkWidget* slider_groove;
GtkWidget* slider_texswap;
GtkWidget* slider_recursion;
GtkWidget* slider_hom;

GtkWidget* btn_process;
GtkWidget* btn_track;
GtkWidget* track_area;
GtkWidget* slider_parts[FX_COUNT];

// the track
static vector<Section> sections;
static string sections_for;     // which video they belong to
static GtkWidget* sliders_box = nullptr;
static bool track_ready = false;  // false = sliders moved since the last calculate
static GtkWidget* btn_browse = nullptr;
static float playhead = -1;     // seconds, -1 = not rendering
static float render_fps = 0;
static pid_t child_pid = 0;
static bool window_gone = false;

// same list the backend makes in process_video. stuck goes by the glitch slider
// because glitch is what triggers it
static void read_used(bool* used, float* a) {
    a[FX_GLITCH] = gtk_range_get_value(GTK_RANGE(slider_glitch));
    a[FX_BLUR] = gtk_range_get_value(GTK_RANGE(slider_mblur));
    a[FX_VOID] = gtk_range_get_value(GTK_RANGE(slider_color));
    a[FX_STUCK] = a[FX_GLITCH];
    a[FX_BEATSYNC] = gtk_range_get_value(GTK_RANGE(slider_beatsync));
    a[FX_ROT] = gtk_range_get_value(GTK_RANGE(slider_rot));
    a[FX_WARP] = gtk_range_get_value(GTK_RANGE(slider_warp));
    a[FX_QUAKE] = gtk_range_get_value(GTK_RANGE(slider_quake));
    a[FX_SPECTRAL] = gtk_range_get_value(GTK_RANGE(slider_spectral));
    a[FX_GROOVE] = gtk_range_get_value(GTK_RANGE(slider_groove));
    a[FX_TEXSWAP] = gtk_range_get_value(GTK_RANGE(slider_texswap));
    a[FX_RECURSION] = gtk_range_get_value(GTK_RANGE(slider_recursion));
    a[FX_HOM] = gtk_range_get_value(GTK_RANGE(slider_hom));
    for (int i = 0; i < FX_COUNT; i++) used[i] = a[i] > 0.01f;
}

// slider moved, deal the sections out again. fast enough to do while dragging
static void rearrange() {
    if (window_gone || !track_area) return;
    float parts[FX_COUNT];
    bool used[FX_COUNT];
    float amounts[FX_COUNT];
    for (int i = 0; i < FX_COUNT; i++) {
        if (!slider_parts[i]) return;  // still building the window
        parts[i] = gtk_range_get_value(GTK_RANGE(slider_parts[i]));
    }
    if (!slider_hom) return;
    read_used(used, amounts);
    assign_parts(sections, parts, used, amounts, gtk_range_get_value(GTK_RANGE(slider_jpeg)));
    gtk_widget_queue_draw(track_area);
}

// track is only shown once you asked for it. slider moved = it doesnt match
// anymore, so its gone until the next calculate
static void track_is_outdated() {
    if (window_gone || !track_area || !track_ready) return;
    track_ready = false;
    gtk_label_set_text(GTK_LABEL(label_status), "changed, hit CALCULATE TRACK again");
    gtk_widget_queue_draw(track_area);
}

static void on_slider_moved(GtkRange*, gpointer) {
    track_is_outdated();
}

static void on_path_changed(GtkEditable*, gpointer) {
    track_is_outdated();
}

// NOT sscanf. gtk_init set the locale, on a german system %f wants "0,5" and
// the backend prints "0.5". same bug as the sliders had the other way round
static void parse_section_line(const char* line) {
    Section s;
    gchar** w = g_strsplit(line, " ", -1);
    int n = g_strv_length(w);
    if (n == 2 + 1 + F_COUNT + 4) {
        s.start = g_ascii_strtod(w[1], nullptr);
        s.end = g_ascii_strtod(w[2], nullptr);
        for (int f = 0; f < F_COUNT; f++) s.feat[f] = g_ascii_strtod(w[3 + f], nullptr);
        s.role = atoi(w[3 + F_COUNT]);
        s.hard_start = g_ascii_strtod(w[4 + F_COUNT], nullptr);
        s.hard_end = g_ascii_strtod(w[5 + F_COUNT], nullptr);
        s.music = g_ascii_strtod(w[6 + F_COUNT], nullptr);
        for (int i = 0; i < FX_COUNT; i++) s.on[i] = false;
        sections.push_back(s);
    }
    g_strfreev(w);
}

static void on_backend_line(const char* line) {
    cout << line << endl;  // terminal still gets everything

    if (strncmp(line, "SECTION ", 8) == 0) {
        parse_section_line(line);
    } else if (strncmp(line, "source fps: ", 12) == 0) {
        render_fps = g_ascii_strtod(line + 12, nullptr);
    } else if (strncmp(line, "processed ", 10) == 0 && render_fps > 0) {
        playhead = atoi(line + 10) / render_fps;
        gtk_widget_queue_draw(track_area);
    }
}

// runs the backend and keeps the window alive meanwhile. returns exit status,
// -1 if it never ran or the window got closed
static int run_backend(const vector<string>& args) {
    int fd[2];
    if (pipe(fd) != 0) return -1;

    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        dup2(fd[1], 1);
        close(fd[0]);
        close(fd[1]);
        vector<char*> argv;
        for (size_t i = 0; i < args.size(); i++) argv.push_back((char*)args[i].c_str());
        argv.push_back(nullptr);
        execv("./datamosh-sync", argv.data());
        _exit(127);
    }
    close(fd[1]);
    child_pid = pid;
    fcntl(fd[0], F_SETFL, O_NONBLOCK);

    string partial;
    int status = 0;
    bool running = true;
    while (running) {
        if (waitpid(pid, &status, WNOHANG) == pid) running = false;

        // one more read after it exited, the end of the output is still in there
        char buf[4096];
        ssize_t n;
        while ((n = read(fd[0], buf, sizeof(buf))) > 0) {
            partial.append(buf, n);
            size_t nl;
            while ((nl = partial.find('\n')) != string::npos) {
                if (!window_gone) on_backend_line(partial.substr(0, nl).c_str());
                partial.erase(0, nl + 1);
            }
        }

        while (gtk_events_pending()) gtk_main_iteration();
        if (running) usleep(20000);
    }
    close(fd[0]);
    child_pid = 0;

    if (window_gone) return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static bool analyze_song(const char* input) {
    if (sections_for == input && !sections.empty()) return true;

    gtk_label_set_text(GTK_LABEL(label_status), "listening to the song... (big video takes a bit)");
    sections.clear();
    sections_for = "";
    track_ready = false;
    gtk_widget_queue_draw(track_area);

    vector<string> args;
    args.push_back("datamosh-sync");
    args.push_back("--sections");
    args.push_back(input);
    int rc = run_backend(args);
    if (window_gone) return false;
    if (rc != 0 || sections.empty()) {
        sections.clear();
        gtk_label_set_text(GTK_LABEL(label_status), "ERROR: couldnt read that video (does it have audio? see terminal)");
        gtk_widget_queue_draw(track_area);
        return false;
    }
    sections_for = input;
    return true;
}

// song only gets analyzed once per video, after that this is instant
static bool calculate_track(const char* input) {
    if (!analyze_song(input)) return false;
    rearrange();
    track_ready = true;
    gtk_widget_queue_draw(track_area);
    return true;
}

static void set_busy(bool busy) {
    gtk_widget_set_sensitive(btn_process, !busy);
    gtk_widget_set_sensitive(btn_track, !busy);
    gtk_widget_set_sensitive(sliders_box, !busy);  // or the track lies about the running render
    gtk_widget_set_sensitive(entry_input, !busy);
    gtk_widget_set_sensitive(btn_browse, !busy);
}

static void on_track_clicked(GtkWidget*, gpointer) {
    const char* input = gtk_entry_get_text(GTK_ENTRY(entry_input));
    if (!input || input[0] == '\0') {
        gtk_label_set_text(GTK_LABEL(label_status), "ERROR: no input file");
        return;
    }
    set_busy(true);
    string path = input;
    bool ok = calculate_track(path.c_str());
    if (window_gone) return;
    set_busy(false);
    if (ok) gtk_label_set_text(GTK_LABEL(label_status), "track ready, MOSH IT");
}

static string fmt(GtkWidget* slider, const char* format) {
    // NOT snprintf. gtk_init sets the locale, so on a german system %.3f
    // gives "0,500" and the backend's atof reads that as 0. every slider
    // did nothing until it hit exactly 1,000
    char buf[32];
    g_ascii_formatd(buf, sizeof(buf), format, gtk_range_get_value(GTK_RANGE(slider)));
    return buf;
}

static void on_process_clicked(GtkWidget* widget, gpointer data) {
    const char* input = gtk_entry_get_text(GTK_ENTRY(entry_input));
    if (!input || input[0] == '\0') {
        gtk_label_set_text(GTK_LABEL(label_status), "ERROR: no input file");
        return;
    }
    string path = input;

    set_busy(true);

    // nobody calculated it (or a slider moved)? do it now. what gets rendered
    // is always whats on the screen
    if (!track_ready && !calculate_track(path.c_str())) {
        if (!window_gone) set_busy(false);
        return;
    }

    vector<string> args;
    args.push_back("datamosh-sync");
    args.push_back(path);
    args.push_back("");  // no separate audio
    args.push_back(fmt(slider_glitch, "%.3f"));
    args.push_back(fmt(slider_jpeg, "%.1f"));
    args.push_back(fmt(slider_mblur, "%.3f"));
    args.push_back(fmt(slider_color, "%.3f"));
    args.push_back(fmt(slider_shuffle, "%.3f"));
    args.push_back(fmt(slider_beatsync, "%.3f"));
    args.push_back(fmt(slider_rot, "%.3f"));
    args.push_back(fmt(slider_warp, "%.3f"));
    args.push_back(fmt(slider_quake, "%.3f"));
    args.push_back(fmt(slider_spectral, "%.3f"));
    args.push_back(fmt(slider_groove, "%.3f"));
    args.push_back(fmt(slider_texswap, "%.3f"));
    args.push_back(fmt(slider_recursion, "%.3f"));
    args.push_back(fmt(slider_hom, "%.3f"));
    for (int i = 0; i < FX_COUNT; i++) args.push_back(fmt(slider_parts[i], "%.3f"));

    gtk_label_set_text(GTK_LABEL(label_status), "processing... (check terminal)");
    playhead = 0;
    render_fps = 0;
    gtk_widget_queue_draw(track_area);

    int rc = run_backend(args);
    if (window_gone) return;

    playhead = -1;
    gtk_widget_queue_draw(track_area);
    if (rc == 0) {
        gtk_label_set_text(GTK_LABEL(label_status), "done! check for *_munched.mp4");
    } else {
        gtk_label_set_text(GTK_LABEL(label_status), "FAILED, see the terminal");
    }
    set_busy(false);
}

// one row per effect, a block where it plays, grey lines where
// the song changes, red line = where the render is right now
static gboolean on_track_draw(GtkWidget* area, cairo_t* cr, gpointer) {
    int W = gtk_widget_get_allocated_width(area);
    int H = gtk_widget_get_allocated_height(area);
    const int LEFT = 80, RIGHT = 42, TOP = 44;
    int rowh = (H - TOP - 14) / FX_COUNT;

    cairo_set_source_rgb(cr, 0.07, 0.07, 0.09);
    cairo_paint(cr);
    cairo_set_font_size(cr, 11);

    if (!track_ready || sections.empty()) {
        cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
        cairo_move_to(cr, 20, H / 2);
        cairo_show_text(cr, "no track yet. set how much of the song every effect gets, then hit CALCULATE TRACK");
        return FALSE;
    }

    float song = sections.back().end;
    int tw = W - LEFT - RIGHT;
    #define X_OF(t) (LEFT + (t) / song * tw)

    const Section* now = playhead >= 0 ? section_at(sections, playhead) : nullptr;
    bool used[FX_COUNT];
    float amounts_unused[FX_COUNT];
    read_used(used, amounts_unused);

    // top strip: how loud (bar height), what part of the song it thinks this is
    // (color + name) and how hard everything hits right now (white line)
    static const double role_color[ROLE_COUNT][3] = {
        {0.35, 0.35, 0.5},  // normal
        {0.3, 0.8, 0.6},    // lead-in
        {1.0, 0.6, 0.1},    // build
        {0.5, 0.4, 0.5},    // after
        {0.2, 0.5, 0.9},    // breakdown
        {1.0, 0.1, 0.7},    // climax
        {1.0, 0.1, 0.1},    // drop
        {0.9, 0.9, 0.9},    // breath
    };
    for (size_t i = 0; i < sections.size(); i++) {
        const Section& s = sections[i];
        float bar = (TOP - 4) * (0.15f + 0.85f * s.feat[F_ENERGY]);
        const double* c = role_color[s.role];
        cairo_set_source_rgb(cr, c[0], c[1], c[2]);
        cairo_rectangle(cr, X_OF(s.start), TOP - 2 - bar, X_OF(s.end) - X_OF(s.start) - 1, bar);
        cairo_fill(cr);
    }
    cairo_set_font_size(cr, 10);
    int last_role = -1;
    float last_label_x = -100;  // tiny sections next to each other would write over each other
    for (size_t i = 0; i < sections.size(); i++) {
        const Section& s = sections[i];
        // only the first piece of a part gets the name
        if (s.role != last_role && s.role != ROLE_NORMAL && s.role != ROLE_BREATH
            && X_OF(s.start) - last_label_x > 42) {
            cairo_set_source_rgb(cr, 1, 1, 1);
            cairo_move_to(cr, X_OF(s.start) + 2, 11);
            cairo_show_text(cr, role_name(s.role));
            last_label_x = X_OF(s.start);
        }
        last_role = s.role;
    }
    cairo_set_font_size(cr, 11);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_set_line_width(cr, 1.5);
    for (size_t i = 0; i < sections.size(); i++) {
        const Section& s = sections[i];
        cairo_move_to(cr, X_OF(s.start), TOP - 2 - (TOP - 6) * s.hard_start);
        cairo_line_to(cr, X_OF(s.end), TOP - 2 - (TOP - 6) * s.hard_end);
    }
    cairo_stroke(cr);
    cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
    cairo_move_to(cr, 4, 15);
    cairo_show_text(cr, "the song");
    cairo_move_to(cr, 4, 30);
    cairo_show_text(cr, "line = how hard");

    for (int fx = 0; fx < FX_COUNT; fx++) {
        int y = TOP + fx * rowh;

        if (!used[fx]) cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
        else if (now && now->on[fx]) cairo_set_source_rgb(cr, 1, 1, 0.2);
        else cairo_set_source_rgb(cr, 0.85, 0.85, 0.85);
        cairo_move_to(cr, 4, y + rowh - 4);
        cairo_show_text(cr, fx_name(fx));

        // every row its own color so you can tell them apart
        double r = 0.5 + 0.5 * sin(fx * 2.1), g = 0.5 + 0.5 * sin(fx * 2.1 + 2.1), b = 0.5 + 0.5 * sin(fx * 2.1 + 4.2);

        float played = 0;
        for (size_t i = 0; i < sections.size(); i++) {
            const Section& s = sections[i];
            if (!s.on[fx]) continue;
            played += s.end - s.start;
            // brighter = more violent right here
            cairo_set_source_rgba(cr, r, g, b, 0.25 + 0.75 * s.fx_aggro[fx]);
            cairo_rectangle(cr, X_OF(s.start), y + 1, X_OF(s.end) - X_OF(s.start), rowh - 2);
            cairo_fill(cr);
        }

        char pct[16];
        snprintf(pct, sizeof(pct), "%d%%", (int)(played / song * 100 + 0.5f));
        cairo_set_source_rgb(cr, 0.85, 0.85, 0.85);
        cairo_move_to(cr, W - RIGHT + 6, y + rowh - 4);
        cairo_show_text(cr, pct);
    }

    // where the song changes
    cairo_set_line_width(cr, 1);
    cairo_set_source_rgba(cr, 1, 1, 1, 0.22);
    for (size_t i = 0; i < sections.size(); i++) {
        cairo_move_to(cr, X_OF(sections[i].start), TOP);
        cairo_line_to(cr, X_OF(sections[i].start), TOP + FX_COUNT * rowh);
    }
    cairo_stroke(cr);

    // seconds along the bottom
    cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
    int step = song > 300 ? 60 : (song > 60 ? 10 : (song > 15 ? 5 : 1));
    for (int t = 0; t < song; t += step) {
        char lbl[16];
        snprintf(lbl, sizeof(lbl), "%ds", t);
        cairo_move_to(cr, X_OF(t), TOP + FX_COUNT * rowh + 11);
        cairo_show_text(cr, lbl);
    }

    if (playhead >= 0) {
        cairo_set_source_rgb(cr, 1, 0.1, 0.1);
        cairo_set_line_width(cr, 2);
        cairo_move_to(cr, X_OF(playhead), 0);
        cairo_line_to(cr, X_OF(playhead), TOP + FX_COUNT * rowh);
        cairo_stroke(cr);
    }
    return FALSE;
}

static void on_window_destroy(GtkWidget*, gpointer) {
    window_gone = true;
    if (child_pid > 0) kill(child_pid, SIGTERM);  // or it keeps rendering with nobody watching
    gtk_main_quit();
}

static void on_file_chooser_clicked(GtkWidget* widget, gpointer data) {
    GtkWidget* dialog = gtk_file_chooser_dialog_new(
        "Open Video", GTK_WINDOW(data),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open", GTK_RESPONSE_ACCEPT,
        NULL
    );

    // filter for video files
    GtkFileFilter* filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Video files");
    gtk_file_filter_add_pattern(filter, "*.mp4");
    gtk_file_filter_add_pattern(filter, "*.avi");
    gtk_file_filter_add_pattern(filter, "*.mov");
    gtk_file_filter_add_pattern(filter, "*.mkv");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        gtk_entry_set_text(GTK_ENTRY(entry_input), filename);
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

// helper: one row of the grid, label + how hard + (maybe) how much of the song
// returns the GtkScale widget
static int grid_row = 1;
GtkWidget* make_slider(GtkWidget* grid, const char* label,
                        double min, double max, double val, double step, int fx = -1) {
    GtkWidget* lbl = gtk_label_new(label);
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), lbl, 0, grid_row, 1, 1);

    GtkWidget* slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, min, max, step);
    gtk_range_set_value(GTK_RANGE(slider), val);
    gtk_scale_set_value_pos(GTK_SCALE(slider), GTK_POS_RIGHT);
    gtk_widget_set_hexpand(slider, TRUE);
    g_signal_connect(slider, "value-changed", G_CALLBACK(on_slider_moved), NULL);
    gtk_grid_attach(GTK_GRID(grid), slider, 1, grid_row, 1, 1);

    if (fx >= 0) {
        GtkWidget* p = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.01);
        gtk_range_set_value(GTK_RANGE(p), 1.0);
        gtk_scale_set_value_pos(GTK_SCALE(p), GTK_POS_RIGHT);
        gtk_widget_set_hexpand(p, TRUE);
        g_signal_connect(p, "value-changed", G_CALLBACK(on_slider_moved), NULL);
        gtk_grid_attach(GTK_GRID(grid), p, 2, grid_row, 1, 1);
        slider_parts[fx] = p;
    }
    grid_row++;
    return slider;
}

int main(int argc, char** argv) {
    gtk_init(&argc, &argv);

    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "datamosh-sync 0.3");
    gtk_window_set_default_size(GTK_WINDOW(window), 980, 860);
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);
    g_signal_connect(window, "destroy", G_CALLBACK(on_window_destroy), NULL);

    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // --- input file row ---
    GtkWidget* hbox_file = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    entry_input = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_input), "input video path...");
    g_signal_connect(entry_input, "changed", G_CALLBACK(on_path_changed), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_file), entry_input, TRUE, TRUE, 0);
    btn_browse = gtk_button_new_with_label("Browse");
    g_signal_connect(btn_browse, "clicked", G_CALLBACK(on_file_chooser_clicked), window);
    gtk_box_pack_start(GTK_BOX(hbox_file), btn_browse, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox_file, FALSE, FALSE, 0);

    // --- separator ---
    gtk_box_pack_start(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 4);

    // --- sliders ---
    sliders_box = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(sliders_box), 10);
    gtk_box_pack_start(GTK_BOX(vbox), sliders_box, FALSE, FALSE, 0);

    GtkWidget* h1 = gtk_label_new("how hard");
    GtkWidget* h2 = gtk_label_new("how much of the song (1 = the whole time)");
    gtk_grid_attach(GTK_GRID(sliders_box), h1, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(sliders_box), h2, 2, 0, 1, 1);

    slider_glitch  = make_slider(sliders_box, "Glitch Intensity",  0.0, 1.0, 0.5,  0.01, FX_GLITCH);
    slider_jpeg    = make_slider(sliders_box, "Void Tolerance (low = huge voids)",
                                 1.0, 100.0, 65.0, 1.0);
    slider_mblur   = make_slider(sliders_box, "Motion Blur / Trails / Stacks", 0.0, 1.0, 0.3, 0.01, FX_BLUR);
    slider_color   = make_slider(sliders_box, "Void Depth",        0.0, 1.0, 0.2,  0.01, FX_VOID);
    slider_shuffle = make_slider(sliders_box, "Stuck Overlay Duration",      0.0, 1.0, 0.3,  0.01, FX_STUCK);
    slider_beatsync = make_slider(sliders_box, "Beat Sync",         0.0, 1.0, 0.7,  0.01, FX_BEATSYNC);

    gtk_grid_attach(GTK_GRID(sliders_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), 0, grid_row++, 3, 1);

    slider_rot      = make_slider(sliders_box, "ROT (decoder lost its keyframes)", 0.0, 1.0, 0.45, 0.01, FX_ROT);
    slider_warp     = make_slider(sliders_box, "WARP (tunnel + gravity wells)", 0.0, 1.0, 0.3, 0.01, FX_WARP);
    slider_quake    = make_slider(sliders_box, "TIME QUAKE (uses a lot of ram)", 0.0, 1.0, 0.25, 0.01, FX_QUAKE);
    slider_spectral = make_slider(sliders_box, "SPECTRAL LAG (two moments at once)", 0.0, 1.0, 0.2, 0.01, FX_SPECTRAL);
    slider_groove   = make_slider(sliders_box, "MOTION SAMPLER (loops the motion of a bar)", 0.0, 1.0, 0.1, 0.01, FX_GROOVE);
    slider_texswap  = make_slider(sliders_box, "TEXTURE SWAP (road wearing a house)", 0.0, 1.0, 0.12, 0.01, FX_TEXSWAP);
    slider_recursion = make_slider(sliders_box, "RECURSION (frames inside frames)", 0.0, 1.0, 0.1, 0.01, FX_RECURSION);
    slider_hom = make_slider(sliders_box, "HALL OF MIRRORS (noclip, how long it stays dirty)", 0.0, 1.0, 0.12, 0.01, FX_HOM);

    // --- separator ---
    gtk_box_pack_start(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 4);

    // TODO: preset buttons would be nice here
    // maybe: Subtle / Normal / Extreme / Chaos
    // NOTE: sliders actually do something now at ALL positions.
    // (effects used to have hard thresholds so mid settings = clean video)

    // --- the track ---
    track_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(track_area, -1, 250);
    gtk_widget_set_vexpand(track_area, TRUE);
    g_signal_connect(track_area, "draw", G_CALLBACK(on_track_draw), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), track_area, TRUE, TRUE, 0);

    // --- process button ---
    GtkWidget* hbox_buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    btn_track = gtk_button_new_with_label("CALCULATE TRACK");
    g_signal_connect(btn_track, "clicked", G_CALLBACK(on_track_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_buttons), btn_track, FALSE, FALSE, 0);
    btn_process = gtk_button_new_with_label("MOSH IT");
    // make it kinda big
    gtk_widget_set_size_request(btn_process, -1, 50);
    g_signal_connect(btn_process, "clicked", G_CALLBACK(on_process_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_buttons), btn_process, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox_buttons, FALSE, FALSE, 0);

    // --- status label ---
    label_status = gtk_label_new("ready");
    gtk_widget_set_halign(label_status, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), label_status, FALSE, FALSE, 0);

    // --- note ---
    GtkWidget* note = gtk_label_new("output: <filename>_munched.mp4\nthe track moves while it renders, sliders are locked until its done");
    gtk_widget_set_halign(note, GTK_ALIGN_START);
    PangoAttrList* attrs = pango_attr_list_new();
    pango_attr_list_insert(attrs, pango_attr_scale_new(0.8));
    gtk_label_set_attributes(GTK_LABEL(note), attrs);
    gtk_box_pack_start(GTK_BOX(vbox), note, FALSE, FALSE, 0);

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}
