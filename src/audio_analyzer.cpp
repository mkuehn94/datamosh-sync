#include "audio_analyzer.h"
#include "arrange.h"
#include "constants.h"
#include <iostream>
#include <vector>
#include <cstdio>
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

// glue the audio back on with ffmpeg, the render is silent
// (lives here now, was in main.cpp, its all ffmpeg forks anyway)
int mux_audio(const string& video, const string& audio_src, const string& out) {
    pid_t pid = fork();
    if (pid == 0) {
        // child
        execlp("ffmpeg", "ffmpeg",
               "-i", video.c_str(),
               "-i", audio_src.c_str(),
               "-c:v", "copy",
               "-c:a", "copy",
               "-map", "0:v:0",
               "-map", "1:a:0",
               "-shortest",
               out.c_str(),
               "-y",
               "-loglevel", "error",
               (char*)nullptr);
        exit(1);  // execlp failed
    }
    int status;
    waitpid(pid, &status, 0);
    return WEXITSTATUS(status);
}

// same trick as mux_audio. decoding audio by hand with
// libav + resampling is a ton of code, ffmpeg can just pipe us raw floats
static bool read_pcm(const string& path, vector<float>& out) {
    int fd[2];
    pipe(fd);

    pid_t pid = fork();
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


// ---------------------------------------------------------------------
// finding the parts of a song. slide along it and look for the spots where
// the 4 seconds before sound different from the 4 seconds after

#define HOP 0.25f          // seconds per analysis step
#define MIN_SECTION 2.0f
#define MAX_SECTION 6.0f   // bigger and a low slider cant hit its percent

// tiny grey video through ffmpeg, 10 per second. same pipe trick as read_pcm
// and it still has to decode the whole video so its not instant on big files
static vector<float> video_motion(const string& path) {
    vector<float> motion;
    if (path.empty()) return motion;

    const int W = 32, H = 18;
    int fd[2];
    pipe(fd);
    pid_t pid = fork();
    if (pid == 0) {
        dup2(fd[1], 1);
        close(fd[0]);
        close(fd[1]);
        execlp("ffmpeg", "ffmpeg", "-loglevel", "error", "-i", path.c_str(), "-an",
               "-vf", "fps=10,scale=32:18,format=gray", "-f", "rawvideo", "-", (char*)nullptr);
        _exit(1);
    }
    close(fd[1]);

    vector<unsigned char> cur(W * H), last(W * H);
    bool have_last = false;
    while (true) {
        int got = 0;
        while (got < W * H) {
            ssize_t n = read(fd[0], cur.data() + got, W * H - got);
            if (n <= 0) break;
            got += n;
        }
        if (got < W * H) break;
        float d = 0;
        for (int i = 0; i < W * H; i++) d += abs((int)cur[i] - (int)last[i]);
        motion.push_back(have_last ? d / (W * H) : 0.0f);
        last = cur;
        have_last = true;
    }
    close(fd[0]);
    int status;
    waitpid(pid, &status, 0);
    return motion;
}

static void squash(vector<float>& v) {
    float mean = 0, var = 0;
    for (size_t i = 0; i < v.size(); i++) mean += v[i];
    mean /= v.size();
    for (size_t i = 0; i < v.size(); i++) var += (v[i] - mean) * (v[i] - mean);
    float sd = sqrt(var / v.size());
    if (sd < 0.0001f) sd = 1;
    for (size_t i = 0; i < v.size(); i++) v[i] = (v[i] - mean) / sd;
}

static float mean_between(const vector<float>& v, int a, int b) {
    if (a < 0) a = 0;
    if (b > (int)v.size()) b = v.size();
    if (b <= a) return 0;
    float s = 0;
    for (int i = a; i < b; i++) s += v[i];
    return s / (b - a);
}

static float percentile(vector<float> v, float p) {
    sort(v.begin(), v.end());
    return v[(int)(p * (v.size() - 1))];
}

// 0-1 but one crazy spike doesnt squash everything else
static vector<float> norm01(const vector<float>& v) {
    float lo = percentile(v, 0.05f), hi = percentile(v, 0.95f);
    vector<float> out(v.size());
    for (size_t i = 0; i < v.size(); i++) {
        float x = hi - lo > 0.0001f ? (v[i] - lo) / (hi - lo) : 0.5f;
        out[i] = x < 0 ? 0 : (x > 1 ? 1 : x);
    }
    return out;
}

static vector<float> blur_curve(const vector<float>& v, int radius) {
    vector<float> out(v.size());
    for (int i = 0; i < (int)v.size(); i++) out[i] = mean_between(v, i - radius, i + radius + 1);
    return out;
}

struct Moment {
    float start, end;
    int role;
};

static int role_at(const vector<Moment>& m, float t) {
    int role = ROLE_NORMAL;
    for (size_t i = 0; i < m.size(); i++) {
        if (t >= m[i].start && t < m[i].end && m[i].role > role) role = m[i].role;
    }
    return role;
}

static float snap_to_beat(float t, const vector<float>& beats) {
    float best = t, best_d = 1.0f;  // more than a second away, dont bother
    for (size_t i = 0; i < beats.size(); i++) {
        float d = fabs(beats[i] - t);
        if (d < best_d) {
            best_d = d;
            best = beats[i];
        }
    }
    // kick fires a tiny bit before the beat time (the on_beat window)
    if (best != t) best -= 0.05f;
    return best < 0 ? 0 : best;
}

vector<Section> cut_song(AudioAnalyzer* audio, const string& video_path) {
    vector<Section> out;
    float song = audio->get_duration();
    vector<float> beats = audio->get_beat_times();
    int n = (int)(song / HOP);
    if (n < 1) n = 1;

    vector<float> motion = video_motion(video_path);

    vector<float> energy(n), bass(n), treble(n), busy(n), moving(n);
    for (int i = 0; i < n; i++) {
        float t = i * HOP;
        // curves snap back fast, a single sample could be between two hits
        for (int k = 0; k < 5; k++) {
            float tt = t + k * HOP / 5;
            energy[i] += audio->get_current_energy(tt) / 5;
            bass[i] += audio->get_bass(tt) / 5;
            treble[i] += audio->get_treble(tt) / 5;
        }
        for (size_t b = 0; b < beats.size(); b++) {
            if (fabs(beats[b] - t) < 1.0f) busy[i] += 1;
        }
        moving[i] = mean_between(motion, (int)(t * 10), (int)((t + HOP) * 10));
    }

    // copies for the novelty thing, the raw ones are for the section numbers
    vector<float> ze = energy, zb = bass, zt = treble, zu = busy, zm = moving;
    squash(ze); squash(zb); squash(zt); squash(zu); squash(zm);

    // short clips get a shorter window or nothing ever fits
    int win = (int)(min(4.0f, song / 4) / HOP);
    if (win < 1) win = 1;
    vector<float> novelty(n, 0.0f);
    for (int i = win; i + win < n; i++) {
        novelty[i] = fabs(mean_between(ze, i - win, i) - mean_between(ze, i, i + win))
                   + fabs(mean_between(zb, i - win, i) - mean_between(zb, i, i + win))
                   + fabs(mean_between(zt, i - win, i) - mean_between(zt, i, i + win))
                   + fabs(mean_between(zu, i - win, i) - mean_between(zu, i, i + win))
                   + 0.5f * fabs(mean_between(zm, i - win, i) - mean_between(zm, i, i + win));
    }
    float nm = mean_between(novelty, 0, n), nv = 0;
    for (int i = 0; i < n; i++) nv += (novelty[i] - nm) * (novelty[i] - nm);
    float limit = nm + 0.5f * sqrt(nv / n);

    vector<float> bounds;
    int peak_range = (int)(2.0f / HOP);
    for (int i = 0; i < n; i++) {
        if (novelty[i] < limit) continue;
        bool peak = true;
        for (int j = max(0, i - peak_range); j <= min(n - 1, i + peak_range); j++) {
            if (novelty[j] > novelty[i]) peak = false;
        }
        if (peak) bounds.push_back(snap_to_beat(i * HOP, beats));
    }

    // hard cuts in the video. motion jumps way above whats around it
    int cuts = 0;
    for (int i = 1; i < (int)motion.size(); i++) {
        float around = (mean_between(motion, i - 10, i) + mean_between(motion, i + 1, i + 11)) / 2;
        if (motion[i] > 25.0f && motion[i] > around * 5.0f) {
            bounds.push_back(snap_to_beat(i / 10.0f, beats));
            cuts++;
        }
    }

    // ===== the big picture: where does the song go =====
    // same numbers as above but slow, a few seconds at once. this is the
    // "tension" of the song, and from its shape we guess the moments
    vector<float> en01 = norm01(energy), ba01 = norm01(bass), tr01 = norm01(treble), bu01 = norm01(busy);
    vector<float> raw_tension(n);
    for (int i = 0; i < n; i++) {
        raw_tension[i] = 0.45f * en01[i] + 0.2f * tr01[i] + 0.2f * bu01[i] + 0.15f * ba01[i];
    }
    vector<float> tension = norm01(blur_curve(raw_tension, (int)(2.0f / HOP)));

    // a song thats the same all the way has no climax, and norm01 would blow
    // up tiny wiggles into fake ones
    bool has_shape = percentile(energy, 0.95f) - percentile(energy, 0.05f) > 0.12f;

    vector<Moment> moments;
    vector<float> forced;   // these cuts have to happen, no matter how short
    vector<float> drop_times;

    if (has_shape) {
        // drops: louder + more bass right now than the 2 seconds before
        int two = (int)(2.0f / HOP);
        vector<float> hit(n), jump(n, 0.0f);
        for (int i = 0; i < n; i++) hit[i] = en01[i] + 0.5f * ba01[i];
        for (int i = two; i + two < n; i++) {
            jump[i] = mean_between(hit, i, i + two) - mean_between(hit, i - two, i);
        }
        for (int i = two; i + two < n; i++) {
            if (i * HOP < 6.0f) continue;   // the song starting isnt a drop
            if (jump[i] < 0.45f || mean_between(en01, i, i + two) < 0.55f) continue;
            bool peak = true;
            for (int j = max(0, i - two * 2); j <= min(n - 1, i + two * 2); j++) {
                if (jump[j] > jump[i]) peak = false;
            }
            if (peak) drop_times.push_back(snap_to_beat(i * HOP, beats));
        }

        // builds: tension keeps climbing for at least 8 sec
        for (int i = 0; i + 4 < n; ) {
            int j = i;
            while (j + 4 < n && tension[j + 4] >= tension[j] - 0.01f) j++;
            // flat also counts as "not falling". the build starts where it really climbs
            int st = i;
            while (st < j && tension[st + 4] - tension[st] < 0.02f) st++;
            float a = st * HOP, b = (j + 4) * HOP;
            bool ends_on_drop = false;
            // the blur makes the curve keep rising through the drop itself, a
            // build ends where the hit is
            for (size_t d = 0; d < drop_times.size(); d++) {
                if (drop_times[d] > a + 4.0f && drop_times[d] < b + 3.0f) {
                    b = drop_times[d];
                    ends_on_drop = true;
                    break;
                }
            }
            int ib = min(n - 1, (int)(b / HOP));
            // climbing from nothing to slightly more than nothing isnt a build
            bool goes_somewhere = ends_on_drop || tension[ib] >= 0.5f;
            if (b - a >= 8.0f && tension[ib] - tension[st] >= 0.2f && goes_somewhere) {
                Moment m = {a, b, ROLE_BUILD};
                moments.push_back(m);
            }
            i = j + 1;
        }

        // breakdown: was loud, now clearly isnt, stays that way a while
        for (int i = 1; i < n; ) {
            if (tension[i] >= 0.45f) { i++; continue; }
            int j = i;
            while (j < n && tension[j] < 0.45f) j++;
            float was = 0;
            for (int k = max(0, i - (int)(8.0f / HOP)); k < i; k++) was = max(was, tension[k]);
            // ends where it starts climbing again, not where its loud again
            float lowest = 1;
            for (int k = i; k < j; k++) lowest = min(lowest, tension[k]);
            int e = j - 1;
            while (e > i && tension[e] > lowest + 0.05f) e--;
            if ((e - i) * HOP >= 4.0f && was > 0.55f) {
                Moment m = {i * HOP, e * HOP, ROLE_BREAKDOWN};
                moments.push_back(m);
            }
            i = j + 1;
        }

        // climax: strongest stretch of the song, later counts a bit more
        // because thats usually where it is
        if (song >= 30.0f) {
            float len = min(16.0f, song / 5);
            int w = (int)(len / HOP);
            int best = -1;
            float best_v = -1;
            for (int i = 0; i + w <= n; i++) {
                float v = mean_between(tension, i, i + w) * (0.85f + 0.3f * i / n);
                if (v > best_v) { best_v = v; best = i; }
            }
            float a = best * HOP;
            for (size_t d = 0; d < drop_times.size(); d++) {
                if (fabs(drop_times[d] - a) < 8.0f) a = drop_times[d];  // starts on the hit if there is one
            }
            a = snap_to_beat(a, beats);
            float b = min(song, snap_to_beat(a + len, beats));
            Moment m = {a, b, ROLE_CLIMAX};
            moments.push_back(m);
            Moment after = {b, min(song, b + 8.0f), ROLE_AFTER};
            moments.push_back(after);
            forced.push_back(a);
            forced.push_back(b);
            cout << "[parts] climax " << a << " - " << b << endl;
        }

        for (size_t d = 0; d < drop_times.size(); d++) {
            float t = drop_times[d];
            float breath = max(0.0f, t - 1.25f);
            Moment m1 = {breath, t, ROLE_BREATH};
            float end = min(song, snap_to_beat(t + 4.0f, beats));
            Moment m2 = {t, end, ROLE_DROP};
            moments.push_back(m1);
            moments.push_back(m2);
            forced.push_back(breath);
            forced.push_back(t);
            forced.push_back(end);
            cout << "[parts] drop at " << t << endl;
        }
        for (size_t i = 0; i < moments.size(); i++) {
            if (moments[i].role == ROLE_BUILD || moments[i].role == ROLE_BREAKDOWN) {
                bounds.push_back(moments[i].start);
                bounds.push_back(moments[i].end);
                cout << "[parts] " << role_name(moments[i].role) << " " << moments[i].start
                     << " - " << moments[i].end << endl;
            }
        }
    } else {
        cout << "[parts] song is pretty much the same all the way, no big moments" << endl;
    }

    // lead-in: everything before the first big moment, if thats a while
    float lead_end = 0;
    if (has_shape) {
        for (int i = (int)(6.0f / HOP); i < n; i++) {
            if (tension[i] >= 0.5f) { lead_end = i * HOP; break; }
        }
        for (size_t d = 0; d < drop_times.size(); d++) {
            float breath = max(0.0f, drop_times[d] - 1.25f);
            if (lead_end == 0 || breath < lead_end) lead_end = breath;
        }
        // song that starts loud doesnt need to be led into anything
        if (lead_end >= 6.0f && mean_between(tension, 0, (int)(lead_end / HOP)) < 0.5f) {
            Moment m = {0.0f, lead_end, ROLE_LEADIN};
            moments.push_back(m);
            bounds.push_back(lead_end);
            cout << "[parts] lead-in 0 - " << lead_end << endl;
        } else {
            lead_end = 0;
        }
    }

    // how hard every effect hits, on top of its slider. every step of the song
    vector<float> hard(n);
    for (int i = 0; i < n; i++) {
        float t = i * HOP;
        float base = has_shape ? 0.7f + 0.3f * tension[i] : 1.0f;
        hard[i] = base;
        // pick the moment thats winning right now
        const Moment* now = nullptr;
        for (size_t m = 0; m < moments.size(); m++) {
            if (t >= moments[m].start && t < moments[m].end && (!now || moments[m].role > now->role)) now = &moments[m];
        }
        if (!now) continue;
        float f = (t - now->start) / (now->end - now->start);
        if (now->role == ROLE_BUILD) hard[i] = 0.35f + 0.65f * f;  // gets worse and worse
        if (now->role == ROLE_LEADIN) hard[i] = 0.75f + 0.25f * f;
        if (now->role == ROLE_BREAKDOWN) hard[i] = 0.5f;
        if (now->role == ROLE_CLIMAX || now->role == ROLE_DROP) hard[i] = 1.0f;
        if (now->role == ROLE_BREATH) hard[i] = 0.1f;
        if (now->role == ROLE_AFTER) hard[i] = 1.0f + (base - 1.0f) * f;  // falls apart slowly
    }

    // long parts get chopped at beats
    bounds.push_back(0.0f);
    bounds.push_back(song);
    sort(bounds.begin(), bounds.end());
    vector<float> chopped;
    float min_len = min(MIN_SECTION, song / 4);
    for (size_t i = 0; i + 1 < bounds.size(); i++) {
        chopped.push_back(bounds[i]);
        float len = bounds[i+1] - bounds[i];
        int pieces = (int)ceil(len / MAX_SECTION);
        for (int p = 1; p < pieces; p++) {
            chopped.push_back(snap_to_beat(bounds[i] + len * p / pieces, beats));
        }
    }
    sort(chopped.begin(), chopped.end());

    // nothing shorter than min_len, whatever is too close just goes away
    vector<float> keep;
    keep.push_back(0.0f);
    for (size_t i = 1; i < chopped.size(); i++) {
        if (chopped[i] - keep.back() >= min_len && song - chopped[i] >= min_len) {
            keep.push_back(chopped[i]);
        }
    }
    keep.push_back(song);

    // the forced cuts go in now, whatever is too close to one has to move out
    sort(forced.begin(), forced.end());
    vector<float> final_cuts;
    for (size_t i = 0; i < keep.size(); i++) {
        bool too_close = false;
        for (size_t f = 0; f < forced.size(); f++) {
            if (fabs(keep[i] - forced[f]) < 1.0f) too_close = true;
        }
        if (!too_close || i == 0 || i + 1 == keep.size()) final_cuts.push_back(keep[i]);
    }
    for (size_t f = 0; f < forced.size(); f++) {
        if (forced[f] > 0.1f && forced[f] < song - 0.1f) final_cuts.push_back(forced[f]);
    }
    sort(final_cuts.begin(), final_cuts.end());
    keep.clear();
    for (size_t i = 0; i < final_cuts.size(); i++) {
        if (keep.empty() || final_cuts[i] - keep.back() > 0.2f) keep.push_back(final_cuts[i]);
    }
    keep.back() = song;

    for (size_t i = 0; i + 1 < keep.size(); i++) {
        Section s;
        s.start = keep[i];
        s.end = keep[i+1];
        int a = (int)(s.start / HOP), b = (int)(s.end / HOP);
        if (b <= a) b = a + 1;
        s.feat[F_ENERGY] = mean_between(energy, a, b);
        s.feat[F_BASS] = mean_between(bass, a, b);
        s.feat[F_TREBLE] = mean_between(treble, a, b);
        s.feat[F_BUSY] = mean_between(busy, a, b);
        s.feat[F_MOTION] = mean_between(moving, a, b);
        s.feat[F_RISE] = 0;
        s.role = role_at(moments, (s.start + s.end) / 2);
        // round UP or a drop starts with the leftover value from the breath before it
        s.hard_start = hard[min(n - 1, (int)ceil(s.start / HOP))];
        s.hard_end = hard[min(n - 1, max(a, b - 1))];
        s.music = mean_between(tension, a, b);
        if (!has_shape) s.music = 0.6f;
        if (s.role == ROLE_DROP || s.role == ROLE_CLIMAX) s.music = 1.0f;
        if (s.role == ROLE_BREATH) s.music = 0.0f;
        if (s.role == ROLE_BREAKDOWN) s.music = min(s.music, 0.3f);
        if (s.role == ROLE_LEADIN) s.music = 0.35f + 0.25f * (s.start + s.end) / 2 / lead_end;
        for (int fx = 0; fx < FX_COUNT; fx++) s.on[fx] = false;
        out.push_back(s);
    }
    for (size_t i = 1; i < out.size(); i++) {
        out[i].feat[F_RISE] = out[i].feat[F_ENERGY] - out[i-1].feat[F_ENERGY];
    }

    // 0-1 inside this song. a quiet song still has its loudest part
    for (int f = 0; f < F_COUNT; f++) {
        float lo = 1e9f, hi = -1e9f;
        for (size_t i = 0; i < out.size(); i++) {
            lo = min(lo, out[i].feat[f]);
            hi = max(hi, out[i].feat[f]);
        }
        for (size_t i = 0; i < out.size(); i++) {
            out[i].feat[f] = hi - lo > 0.0001f ? (out[i].feat[f] - lo) / (hi - lo) : 0.5f;
        }
    }

    // every drop flips which video is the main one
    int world = 0;
    for (size_t i = 0; i < out.size(); i++) {
        if (out[i].role == ROLE_DROP && (i == 0 || out[i-1].role != ROLE_DROP)) world = !world;
        out[i].world = world;
    }

    cout << "[parts] " << out.size() << " sections, " << cuts << " hard cuts in the video" << endl;
    return out;
}

vector<float> intensity_curve(AudioAnalyzer* audio) {
    int n = (int)(audio->get_duration() * 20);
    vector<float> raw(n > 0 ? n : 1, 0.0f);
    for (int i = 0; i < n; i++) {
        float t = i / 20.0f;
        raw[i] = 0.5f * audio->get_current_energy(t) + 0.3f * audio->get_bass(t) + 0.2f * audio->get_treble(t);
    }
    // half fast half slow. only fast = it shakes with every single kick, only slow =
    // you dont notice it moving
    vector<float> fast = blur_curve(raw, 5);    // quarter sec
    vector<float> slow = blur_curve(raw, 40);   // 2 sec
    vector<float> out(raw.size());
    for (size_t i = 0; i < raw.size(); i++) out[i] = 0.5f * fast[i] + 0.5f * slow[i];

    float mean = mean_between(out, 0, out.size()), var = 0;
    for (size_t i = 0; i < out.size(); i++) var += (out[i] - mean) * (out[i] - mean);
    float sd = sqrt(var / out.size());
    if (sd < 0.001f) sd = 1;  // silence
    for (size_t i = 0; i < out.size(); i++) {
        out[i] = (out[i] - mean) / sd;
        if (out[i] > 2.5f) out[i] = 2.5f;
        if (out[i] < -2.5f) out[i] = -2.5f;
    }
    return out;
}

void print_sections(const vector<Section>& sections) {
    for (size_t i = 0; i < sections.size(); i++) {
        const Section& s = sections[i];
        printf("SECTION %.3f %.3f", s.start, s.end);
        for (int f = 0; f < F_COUNT; f++) printf(" %.4f", s.feat[f]);
        printf(" %d %.3f %.3f %.3f %d", s.role, s.hard_start, s.hard_end, s.music, s.world);
        printf("\n");
    }
    fflush(stdout);
}

void print_track(const vector<Section>& sections) {
    if (sections.empty()) return;
    float song = sections.back().end;
    const int COLS = 70;
    string roles;
    for (int c = 0; c < COLS; c++) {
        const char* r = role_name(section_at(sections, (c + 0.5f) / COLS * song)->role);
        roles += r[0] ? r[0] : ' ';
    }
    printf("%10s |%s|\n", "moments", roles.c_str());
    for (int fx = 0; fx < FX_COUNT; fx++) {
        string line;
        float played = 0;
        for (int c = 0; c < COLS; c++) {
            line += section_at(sections, (c + 0.5f) / COLS * song)->on[fx] ? '#' : '.';
        }
        for (size_t i = 0; i < sections.size(); i++) {
            if (sections[i].on[fx]) played += sections[i].end - sections[i].start;
        }
        printf("%10s |%s| %3d%%\n", fx_name(fx), line.c_str(), (int)(played / song * 100 + 0.5f));
    }
    // what it thinks it did, so you can check that against reality
    for (size_t i = 0; i < sections.size(); i++) {
        float clean = 1;
        for (int fx = 0; fx < FX_COUNT; fx++) clean *= 1.0f - sections[i].fx_aggro[fx] * 0.7f;
        printf("[plan] %.2f %.2f music %.2f violence %.2f\n", sections[i].start, sections[i].end,
               sections[i].music, 1.0f - clean);
    }
    fflush(stdout);
}
