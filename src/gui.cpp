#include "gui.h"
#include <iostream>

using namespace std;

GUI::GUI() {
    glitch_intensity = 0.5;
    jpeg_quality = 50;
    motion_blur = 0.3;
    color_shift = 0.2;
    block_shuffle = 0.4;
    beat_sync = 0.8;
    auto_mode = false;

    window = nullptr;
    preview = nullptr;
    video_proc = nullptr;
    audio_anal = nullptr;
    effects = nullptr;
}

GUI::~GUI() {
    if (video_proc) delete video_proc;
    if (audio_anal) delete audio_anal;
    if (effects) delete effects;
}

bool GUI::init() {
    gtk_init(NULL, NULL);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "datamosh-sync");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // main layout
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // preview area (just a placeholder for now)
    preview = gtk_drawing_area_new();
    gtk_widget_set_size_request(preview, 640, 360);
    gtk_box_pack_start(GTK_BOX(vbox), preview, FALSE, FALSE, 0);

    // sliders
    GtkWidget* slider_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_pack_start(GTK_BOX(vbox), slider_box, TRUE, TRUE, 0);

    // glitch intensity
    GtkWidget* label1 = gtk_label_new("Glitch Intensity");
    gtk_box_pack_start(GTK_BOX(slider_box), label1, FALSE, FALSE, 0);
    GtkWidget* slider1 = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 1, 0.01);
    gtk_range_set_value(GTK_RANGE(slider1), glitch_intensity);
    g_signal_connect(slider1, "value-changed", G_CALLBACK(on_slider_changed), this);
    gtk_box_pack_start(GTK_BOX(slider_box), slider1, FALSE, FALSE, 0);

    // jpeg quality
    GtkWidget* label2 = gtk_label_new("JPEG Quality");
    gtk_box_pack_start(GTK_BOX(slider_box), label2, FALSE, FALSE, 0);
    GtkWidget* slider2 = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 1, 100, 1);
    gtk_range_set_value(GTK_RANGE(slider2), jpeg_quality);
    gtk_box_pack_start(GTK_BOX(slider_box), slider2, FALSE, FALSE, 0);

    // process button
    GtkWidget* btn = gtk_button_new_with_label("PROCESS VIDEO");
    g_signal_connect(btn, "clicked", G_CALLBACK(on_process_clicked), this);
    gtk_box_pack_start(GTK_BOX(vbox), btn, FALSE, FALSE, 5);

    gtk_widget_show_all(window);

    return true;
}

void GUI::run(const string& video_path, const string& audio_path) {
    video_proc = new VideoProcessor();
    audio_anal = new AudioAnalyzer();
    effects = new EffectEngine();

    if (!video_proc->open(video_path)) {
        cerr << "Failed to open video" << endl;
        return;
    }

    if (!audio_anal->load(audio_path)) {
        cerr << "Failed to load audio" << endl;
        return;
    }

    audio_anal->analyze();

    gtk_main();
}

void GUI::on_slider_changed(GtkRange* range, gpointer data) {
    // TODO: update parameters
}

void GUI::on_process_clicked(GtkWidget* widget, gpointer data) {
    GUI* gui = (GUI*)data;
    cout << "Processing video..." << endl;

    // TODO: actual processing
}

gboolean GUI::update_preview(gpointer data) {
    return TRUE;
}
