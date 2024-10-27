#ifndef GUI_H
#define GUI_H

#include <string>
#include <gtk/gtk.h>
#include "processor.h"
#include "audio_analyzer.h"
#include "effects.h"

// simple gui with sliders and buttons
class GUI {
public:
    GUI();
    ~GUI();

    bool init();
    void run(const std::string& video_path, const std::string& audio_path);

    // effect parameters
    float glitch_intensity;
    float jpeg_quality;
    float motion_blur;
    float color_shift;
    float block_shuffle;
    float beat_sync;
    bool auto_mode;

private:
    GtkWidget* window;
    GtkWidget* preview;

    VideoProcessor* video_proc;
    AudioAnalyzer* audio_anal;
    EffectEngine* effects;

    static void on_slider_changed(GtkRange* range, gpointer data);
    static void on_process_clicked(GtkWidget* widget, gpointer data);
    static gboolean update_preview(gpointer data);
};

#endif
