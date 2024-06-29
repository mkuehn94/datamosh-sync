// datamosh-sync GUI
// plain but it works
// just sliders and buttons, nothing fancy
//
// TODO: live preview would be cool but thats a lot of work
// TODO: presets
// TODO: progress bar

#include <gtk/gtk.h>
#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>

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

GtkWidget* btn_process;

static void run_processing(const char* input, float glitch, float jpeg,
                            float mblur, float color, float shuffle, float beatsync,
                            float rot, float warp, float quake, float spectral) {
    char s_glitch[16], s_jpeg[16], s_mblur[16], s_color[16], s_shuffle[16], s_beatsync[16];
    char s_rot[16], s_warp[16], s_quake[16], s_spectral[16];
    snprintf(s_glitch,   sizeof(s_glitch),   "%.3f", glitch);
    snprintf(s_jpeg,     sizeof(s_jpeg),     "%.1f", jpeg);
    snprintf(s_mblur,    sizeof(s_mblur),    "%.3f", mblur);
    snprintf(s_color,    sizeof(s_color),    "%.3f", color);
    snprintf(s_shuffle,  sizeof(s_shuffle),  "%.3f", shuffle);
    snprintf(s_beatsync, sizeof(s_beatsync), "%.3f", beatsync);
    snprintf(s_rot,      sizeof(s_rot),      "%.3f", rot);
    snprintf(s_warp,     sizeof(s_warp),     "%.3f", warp);
    snprintf(s_quake,    sizeof(s_quake),    "%.3f", quake);
    snprintf(s_spectral, sizeof(s_spectral), "%.3f", spectral);

    pid_t pid = fork();
    if (pid == 0) {
        execlp("./datamosh-sync", "datamosh-sync",
               input,
               "",         // no separate audio
               s_glitch, s_jpeg, s_mblur, s_color, s_shuffle, s_beatsync,
               s_rot, s_warp, s_quake, s_spectral,
               (char*)nullptr);
        exit(1);
    }
    int status;
    waitpid(pid, &status, 0);
}

static void on_process_clicked(GtkWidget* widget, gpointer data) {
    const char* input = gtk_entry_get_text(GTK_ENTRY(entry_input));
    if (!input || input[0] == '\0') {
        gtk_label_set_text(GTK_LABEL(label_status), "ERROR: no input file");
        return;
    }

    float glitch  = gtk_range_get_value(GTK_RANGE(slider_glitch));
    float jpeg    = gtk_range_get_value(GTK_RANGE(slider_jpeg));
    float mblur   = gtk_range_get_value(GTK_RANGE(slider_mblur));
    float color   = gtk_range_get_value(GTK_RANGE(slider_color));
    float shuffle = gtk_range_get_value(GTK_RANGE(slider_shuffle));
    float beatsync = gtk_range_get_value(GTK_RANGE(slider_beatsync));
    float rot      = gtk_range_get_value(GTK_RANGE(slider_rot));
    float warp     = gtk_range_get_value(GTK_RANGE(slider_warp));
    float quake    = gtk_range_get_value(GTK_RANGE(slider_quake));
    float spectral = gtk_range_get_value(GTK_RANGE(slider_spectral));

    gtk_label_set_text(GTK_LABEL(label_status), "processing... (check terminal)");
    gtk_widget_set_sensitive(btn_process, FALSE);

    // TODO: run in thread so GUI doesn't freeze
    // for now just blocks lol
    while (gtk_events_pending()) gtk_main_iteration();

    run_processing(input, glitch, jpeg, mblur, color, shuffle, beatsync,
                   rot, warp, spectral, quake);

    gtk_label_set_text(GTK_LABEL(label_status), "done! check for *_munched.mp4");
    gtk_widget_set_sensitive(btn_process, TRUE);
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

// helper: makes a labeled slider row
// returns the GtkScale widget
GtkWidget* make_slider(GtkWidget* box, const char* label,
                        double min, double max, double val, double step) {
    GtkWidget* lbl = gtk_label_new(label);
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(box), lbl, FALSE, FALSE, 0);

    GtkWidget* slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, min, max, step);
    gtk_range_set_value(GTK_RANGE(slider), val);
    gtk_scale_set_value_pos(GTK_SCALE(slider), GTK_POS_RIGHT);
    gtk_box_pack_start(GTK_BOX(box), slider, FALSE, FALSE, 0);
    return slider;
}

int main(int argc, char** argv) {
    gtk_init(&argc, &argv);

    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "datamosh-sync 0.3");
    gtk_window_set_default_size(GTK_WINDOW(window), 480, 760);
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // --- input file row ---
    GtkWidget* hbox_file = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    entry_input = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_input), "input video path...");
    gtk_box_pack_start(GTK_BOX(hbox_file), entry_input, TRUE, TRUE, 0);
    GtkWidget* btn_browse = gtk_button_new_with_label("Browse");
    g_signal_connect(btn_browse, "clicked", G_CALLBACK(on_file_chooser_clicked), window);
    gtk_box_pack_start(GTK_BOX(hbox_file), btn_browse, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox_file, FALSE, FALSE, 0);

    // --- separator ---
    gtk_box_pack_start(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 4);

    // --- sliders ---
    GtkWidget* sliders_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_pack_start(GTK_BOX(vbox), sliders_box, FALSE, FALSE, 0);

    slider_glitch  = make_slider(sliders_box, "Glitch Intensity",  0.0, 1.0, 0.5,  0.01);
    slider_jpeg    = make_slider(sliders_box, "Void Tolerance (low = huge voids)",
                                 1.0, 100.0, 65.0, 1.0);
    slider_mblur   = make_slider(sliders_box, "Motion Blur / Trails / Stacks", 0.0, 1.0, 0.3, 0.01);
    slider_color   = make_slider(sliders_box, "Void Depth",        0.0, 1.0, 0.2,  0.01);
    slider_shuffle = make_slider(sliders_box, "Stuck Overlay Duration",      0.0, 1.0, 0.3,  0.01);
    slider_beatsync = make_slider(sliders_box, "Beat Sync",         0.0, 1.0, 0.7,  0.01);

    gtk_box_pack_start(GTK_BOX(sliders_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 4);

    slider_rot      = make_slider(sliders_box, "ROT (decoder lost its keyframes)",   0.0, 1.0, 0.6, 0.01);
    slider_warp     = make_slider(sliders_box, "WARP (tunnel + gravity wells)",      0.0, 1.0, 0.4, 0.01);
    slider_quake    = make_slider(sliders_box, "TIME QUAKE (uses a lot of ram)",     0.0, 1.0, 0.4, 0.01);
    slider_spectral = make_slider(sliders_box, "SPECTRAL LAG (two moments at once)", 0.0, 1.0, 0.3, 0.01);

    // --- separator ---
    gtk_box_pack_start(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 4);

    // TODO: preset buttons would be nice here
    // maybe: Subtle / Normal / Extreme / Chaos
    // NOTE: sliders actually do something now at ALL positions.
    // (effects used to have hard thresholds so mid settings = clean video)

    // --- process button ---
    btn_process = gtk_button_new_with_label("MOSH IT");
    // make it kinda big
    gtk_widget_set_size_request(btn_process, -1, 50);
    g_signal_connect(btn_process, "clicked", G_CALLBACK(on_process_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), btn_process, FALSE, FALSE, 0);

    // --- status label ---
    label_status = gtk_label_new("ready");
    gtk_widget_set_halign(label_status, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), label_status, FALSE, FALSE, 0);

    // --- note ---
    GtkWidget* note = gtk_label_new("output: <filename>_munched.mp4\nGUI freezes while processing, thats normal, TODO fix");
    gtk_widget_set_halign(note, GTK_ALIGN_START);
    PangoAttrList* attrs = pango_attr_list_new();
    pango_attr_list_insert(attrs, pango_attr_scale_new(0.8));
    gtk_label_set_attributes(GTK_LABEL(note), attrs);
    gtk_box_pack_start(GTK_BOX(vbox), note, FALSE, FALSE, 0);

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}
