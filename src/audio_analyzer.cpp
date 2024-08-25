#include "audio_analyzer.h"
#include "constants.h"
#include <iostream>
#include <cmath>
#include <complex>
#include <algorithm>
#include <unistd.h>
#include <sys/wait.h>

using namespace std;

#define AUDIO_RATE 22050
#define FFT_SIZE 1024
#define HOP_SIZE 256   // ~11ms per analysis frame
#define HOP_SEC ((float)HOP_SIZE / AUDIO_RATE)

AudioAnalyzer::AudioAnalyzer() : duration(0) {
}

AudioAnalyzer::~AudioAnalyzer() {
}

// same trick as mux_audio in main.cpp. decoding audio by hand with
// libav + resampling is a ton of code, ffmpeg can just pipe us raw floats
static bool read_pcm(const string& path, vector<float>& out) {
    int fd[2];
    if (pipe(fd) != 0) return false;

    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        dup2(fd[1], 1);
        close(fd[0]);
        close(fd[1]);
        execlp("ffmpeg", "ffmpeg", "-loglevel", "error",
               "-i", path.c_str(), "-vn", "-ac", "1", "-ar", "22050",
               "-f", "f32le", "-", (char*)nullptr);
        _exit(1);
    }
    close(fd[1]);

    // TODO partial floats if read() splits one in half? hasnt happened
    float buf[8192];
    ssize_t n;
    while ((n = read(fd[0], buf, sizeof(buf))) > 0) {
        out.insert(out.end(), buf, buf + n / sizeof(float));
    }
    close(fd[0]);

    int status;
    waitpid(pid, &status, 0);
    return !out.empty();
}

void AudioAnalyzer::limit_to(float seconds) {
    size_t n = (size_t)(seconds * AUDIO_RATE);
    if (seconds <= 0 || n >= samples.size()) return;
    samples.resize(n);
    duration = seconds;
    cout << "[audio] only the first " << seconds << "s" << endl;
}

bool AudioAnalyzer::load(const string& path) {
    cout << "Loading audio: " << path << endl;
    if (!read_pcm(path, samples)) {
        cerr << "could not decode audio from " << path << endl;
        return false;
    }
    duration = samples.size() / (float)AUDIO_RATE;
    cout << "Audio duration: " << duration << " seconds" << endl;
    return true;
}

bool AudioAnalyzer::load_from_video(const string& video_path) {
    cout << "Extracting audio from video: " << video_path << endl;
    return load(video_path);
}

// textbook radix 2
static void fft(vector<complex<float> >& a) {
    int n = a.size();
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) swap(a[i], a[j]);
    }
    for (int len = 2; len <= n; len <<= 1) {
        float ang = -2.0f * M_PI / len;
        complex<float> wl(cos(ang), sin(ang));
        for (int i = 0; i < n; i += len) {
            complex<float> w(1.0f, 0.0f);
            for (int j = 0; j < len / 2; j++) {
                complex<float> u = a[i + j];
                complex<float> v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

// squash a curve to 0-1. uses the 98th percentile as the top so one crazy
// peak doesnt make the rest of the song look silent
static void normalize(vector<float>& c) {
    if (c.empty()) return;
    vector<float> s(c);
    sort(s.begin(), s.end());
    float top = s[(size_t)(s.size() * 0.98f)];
    if (top < 1e-6f) return;
    for (size_t i = 0; i < c.size(); i++) {
        c[i] = min(1.0f, c[i] / top);
    }
}

void AudioAnalyzer::analyze() {
    cout << "Analyzing audio..." << endl;
    beat_times.clear();

    int frames = 0;
    if ((int)samples.size() > FFT_SIZE) {
        frames = (samples.size() - FFT_SIZE) / HOP_SIZE;
    }
    if (frames < 2) {
        cout << "no audio to analyze, no beats" << endl;
        return;
    }

    vector<float> window(FFT_SIZE);
    for (int i = 0; i < FFT_SIZE; i++) {
        window[i] = 0.5f - 0.5f * cos(2.0f * M_PI * i / (FFT_SIZE - 1));
    }

    int bass_top = 150 * FFT_SIZE / AUDIO_RATE + 1;    // <150hz = kick
    int treble_lo = 3000 * FFT_SIZE / AUDIO_RATE;      // >3k = hats, snare crack

    vector<float> rms(frames), flux_bass(frames), flux_treble(frames), flux_all(frames);

    vector<vector<float> > mags(frames);
    #pragma omp parallel for schedule(static)
    for (int f = 0; f < frames; f++) {
        vector<complex<float> > buf(FFT_SIZE);
        float e = 0;
        for (int i = 0; i < FFT_SIZE; i++) {
            float s = samples[f * HOP_SIZE + i];
            e += s * s;
            buf[i] = s * window[i];
        }
        rms[f] = sqrt(e / FFT_SIZE);
        fft(buf);
        mags[f].resize(FFT_SIZE / 2);
        for (int i = 0; i < FFT_SIZE / 2; i++) {
            // log so quiet bands still count
            mags[f][i] = log1p(abs(buf[i]) * 10.0f);
        }
    }

    // spectral flux: only how much each band went UP since last time.
    // a held note has energy but no flux, a kick has a ton
    for (int f = 1; f < frames; f++) {
        float b = 0, t = 0, a = 0;
        for (int i = 1; i < FFT_SIZE / 2; i++) {
            float d = mags[f][i] - mags[f - 1][i];
            if (d <= 0) continue;
            a += d;
            if (i <= bass_top) b += d;
            if (i >= treble_lo) t += d;
        }
        flux_bass[f] = b;
        flux_treble[f] = t;
        flux_all[f] = a;
    }
    // loud mastered music sits at like 0.8-1.0 the whole time which is
    // useless as a control signal, square it to get some contrast back
    for (int f = 0; f < frames; f++) rms[f] *= rms[f];
    normalize(rms);
    normalize(flux_bass);
    normalize(flux_treble);
    normalize(flux_all);

    // onset strength favors the kick, music people feel the low end first
    vector<float> onset(frames);
    for (int f = 0; f < frames; f++) {
        onset[f] = flux_bass[f] * 0.7f + flux_all[f] * 0.3f;
    }

    // peak picking against a sliding local average (~half a second back and
    // forth), so a quiet section still gets beats
    int half = (int)(0.25f / HOP_SEC);
    int min_gap = (int)(BEAT_MIN_INTERVAL / HOP_SEC);
    int last_beat = -min_gap;
    for (int f = 1; f < frames - 1; f++) {
        float avg = 0;
        int n = 0;
        for (int k = max(0, f - half); k <= min(frames - 1, f + half); k++) {
            avg += onset[k];
            n++;
        }
        avg /= n;
        bool peak = onset[f] >= onset[f - 1] && onset[f] >= onset[f + 1];
        if (peak && onset[f] > avg * 1.6f + 0.08f && f - last_beat >= min_gap) {
            beat_times.push_back(f * HOP_SEC + FFT_SIZE * 0.5f / AUDIO_RATE);
            last_beat = f;
        }
    }

    // follower curves: instant attack, exponential release. this is what the
    // effects read every frame
    energy_curve.resize(frames);
    bass_curve.resize(frames);
    treble_curve.resize(frames);
    float e = 0, b = 0, t = 0;
    for (int f = 0; f < frames; f++) {
        e = max(rms[f], e * 0.995f);
        b = max(flux_bass[f], b * 0.965f);    // ~200ms
        t = max(flux_treble[f], t * 0.93f);   // ~100ms
        energy_curve[f] = e;
        bass_curve[f] = b;
        treble_curve[f] = t;
    }


    samples.clear();  // dont need the pcm anymore
    samples.shrink_to_fit();

    cout << "Found " << beat_times.size() << " beats" << endl;
}

float AudioAnalyzer::curve_at(const vector<float>& c, float time) {
    if (c.empty() || time < 0) return 0.0f;
    size_t i = (size_t)(time / HOP_SEC);
    if (i >= c.size()) return 0.0f;
    return c[i];
}

float AudioAnalyzer::get_current_energy(float time) { return curve_at(energy_curve, time); }
float AudioAnalyzer::get_bass(float time) { return curve_at(bass_curve, time); }
float AudioAnalyzer::get_treble(float time) { return curve_at(treble_curve, time); }
