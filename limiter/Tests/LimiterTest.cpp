// Teste do Auto Gain + Limiter (motores reais, sinais simulados, sem DAW).
#include "LimiterEngine.h"
#include "Analyzer.h"
#include "AutoGainEngine.h"
#include <chrono>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

static constexpr double PI = 3.14159265358979323846;
static int failures = 0;

static void check (bool ok, const std::string& name, const std::string& detail)
{
    std::printf ("[%s] %-52s %s\n", ok ? " OK " : "FALHA", name.c_str(), detail.c_str());
    if (! ok) ++failures;
}
static std::string f (const char* fmt, double a = 0, double b = 0, double c = 0)
{
    char buf[256]; std::snprintf (buf, sizeof buf, fmt, a, b, c); return buf;
}
static float dB (float lin) { return 20.0f * std::log10 (lin + 1e-12f); }

// Roda um sinal mono->estereo pelo limiter em blocos de 128; devolve a saida (canal L)
static std::vector<float> runLimiter (LimiterEngine& e, const LimiterParams& p, const std::vector<float>& in, int block = 128)
{
    std::vector<float> out (in.size());
    std::vector<float> L (block), R (block);
    for (size_t s = 0; s < in.size(); s += (size_t) block)
    {
        const int n = (int) std::min<size_t> ((size_t) block, in.size() - s);
        for (int i = 0; i < n; ++i) L[(size_t) i] = R[(size_t) i] = in[s + (size_t) i];
        float* ch[2] = { L.data(), R.data() };
        e.process (ch, 2, n, p);
        for (int i = 0; i < n; ++i) out[s + (size_t) i] = L[(size_t) i];
    }
    return out;
}

// True peak de referencia: interpolacao 16x com sinc longo (64 pontos de cada lado)
static float refTruePeak (const std::vector<float>& x, size_t from)
{
    float tp = 0.0f;
    for (size_t n = std::max<size_t> (from, 64); n + 64 < x.size(); ++n)
        for (int k = 0; k < 16; ++k)
        {
            const double t = (double) k / 16.0;
            double y = 0.0;
            for (int j = -63; j <= 64; ++j)
            {
                const double d = t - j;
                const double s = std::abs (d) < 1e-12 ? 1.0 : std::sin (PI * d) / (PI * d);
                const double w = 0.5 + 0.5 * std::cos (PI * d / 65.0);
                y += x[(size_t) ((long) n + j)] * s * w;
            }
            tp = std::max (tp, (float) std::abs (y));
        }
    return tp;
}

static double harmonicRatio (const std::vector<float>& x, size_t from, double f0, double sr)
{
    auto power = [&] (double fr) {
        double re = 0, im = 0;
        for (size_t n = from; n < x.size(); ++n) { re += x[n] * std::cos (2 * PI * fr * (double) n / sr); im += x[n] * std::sin (2 * PI * fr * (double) n / sr); }
        return re * re + im * im;
    };
    const double fund = power (f0);
    double harm = 0; for (int h = 2; h <= 7; ++h) harm += power (f0 * h);
    return std::sqrt (harm / fund);
}

int main()
{
    const double SR = 48000.0;
    std::mt19937 rng (7);
    std::normal_distribution<float> nd (0.0f, 1.0f);

    std::printf ("\n=== AUTO GAIN + LIMITER by Augusto Souza - teste dos motores ===\n\n");

    // ---------- 1. Teto absoluto em todos os lookaheads ----------
    {
        std::vector<float> in ((size_t) (SR * 4));
        for (size_t i = 0; i < in.size(); ++i)
        {
            in[i] = 0.25f * nd (rng);
            if (i % 9000 < 40) in[i] += (i % 2 ? 0.9f : -0.9f);   // estalos fortes
        }
        for (int la = 0; la < 4; ++la)
            for (int tp = 0; tp < 2; ++tp)
            {
                if (la == 0 && tp == 1) continue;
                LimiterEngine e; e.prepare (SR);
                LimiterParams p; p.threshold = -10.0f; p.ceiling = -1.0f; p.lookahead = la; p.truePeak = tp;
                auto out = runLimiter (e, p, in);
                float mx = 0; for (float v : out) mx = std::max (mx, std::abs (v));
                check (mx <= LimiterEngine::fromDb (-1.0f) + 1e-6f,
                       f ("Teto -1 dBFS respeitado (lookahead %.0f ms)", LimiterEngine::lookaheadMs[la]) + (tp ? " + TP" : ""),
                       f ("pico de saida %.3f dBFS", dB (mx)));
            }
    }

    // ---------- 2. Ganho do Threshold (estilo L2) sem limitar ----------
    {
        std::vector<float> in ((size_t) SR);
        for (size_t i = 0; i < in.size(); ++i) in[i] = 0.0316f * (float) std::sin (2 * PI * 440 * (double) i / SR); // pico -30 dBFS
        LimiterEngine e; e.prepare (SR);
        LimiterParams p; p.threshold = -6.0f; p.ceiling = -1.0f;
        auto out = runLimiter (e, p, in);
        float mx = 0; for (size_t i = 4800; i < out.size(); ++i) mx = std::max (mx, std::abs (out[i]));
        check (std::abs (dB (mx) - (-25.0f)) < 0.05f && e.grDb.load() > -0.01f,
               "Threshold -6 / Ceiling -1 = +5 dB de ganho, sem atenuar", f ("-30 dBFS -> %.2f dBFS, GR %.2f dB", dB (mx), e.grDb.load()));
    }

    // ---------- 3. Latencia medida (impulso) ----------
    std::printf ("\n  Latencia medida do Limiter (impulso):\n");
    for (double sr : { 44100.0, 48000.0 })
        for (int la = 0; la < 4; ++la)
            for (int tp = 0; tp < 2; ++tp)
            {
                if (la == 0 && tp == 1) continue;
                LimiterEngine e; e.prepare (sr);
                LimiterParams p; p.lookahead = la; p.truePeak = tp; p.threshold = 0.0f; p.ceiling = 0.0f;
                std::vector<float> in (2048, 0.0f); in[100] = 0.5f;
                auto out = runLimiter (e, p, in);
                int at = -1; for (int i = 0; i < 2048; ++i) if (std::abs (out[(size_t) i]) > 1e-4f) { at = i; break; }
                const int measured = at - 100, declared = LimiterEngine::latencyFor (sr, la, tp);
                check (measured == declared,
                       f ("  %.1f kHz, lookahead %.0f ms", sr / 1000.0, LimiterEngine::lookaheadMs[la]) + (tp ? " + True Peak" : ""),
                       f ("%.0f amostras = %.3f ms (informado ao REAPER: %.0f)", measured, measured * 1000.0 / sr, declared));
            }
    std::printf ("\n");

    // ---------- 4. Release manual ----------
    {
        std::vector<float> in ((size_t) SR, 0.0f);
        for (size_t i = 0; i < in.size(); ++i) in[i] = 0.05f * (float) std::sin (2 * PI * 300 * (double) i / SR);
        for (int i = 4800; i < 4848; ++i) in[(size_t) i] = 0.95f;      // um estalo
        LimiterEngine e; e.prepare (SR);
        LimiterParams p; p.threshold = -10.0f; p.ceiling = -1.0f; p.autoRelease = false; p.releaseMs = 100.0f; p.lookahead = 1;
        std::vector<float> L (128), R (128); float grAt = 0, grMin = 0;
        for (size_t s = 0; s < in.size(); s += 128)
        {
            for (int i = 0; i < 128; ++i) L[(size_t) i] = R[(size_t) i] = in[s + (size_t) i];
            float* ch[2] = { L.data(), R.data() }; e.process (ch, 2, 128, p);
            grMin = std::min (grMin, e.grDb.load());
            if (s / 128 == (size_t) ((4800 + 0.6 * SR) / 128)) grAt = e.grDb.load();
        }
        check (grMin < -6.0f && grAt > -0.2f, "Release 100 ms: segura o estalo e solta",
               f ("atenuacao no estalo %.1f dB, 600 ms depois %.2f dB", grMin, grAt));
    }

    // ---------- 5. Release automatico ----------
    {
        auto recoverMs = [&] (int loudSamples) {
            std::vector<float> in ((size_t) (SR * 3), 0.0f);
            for (size_t i = 0; i < in.size(); ++i) in[i] = 0.05f * (float) std::sin (2 * PI * 200 * (double) i / SR);
            for (int i = 4800; i < 4800 + loudSamples; ++i) in[(size_t) i] = 0.9f * (float) std::sin (2 * PI * 200 * i / SR);
            LimiterEngine e; e.prepare (SR);
            LimiterParams p; p.threshold = -10.0f; p.ceiling = -1.0f; p.autoRelease = true; p.lookahead = 1;
            std::vector<float> L (64), R (64); int endBlock = (4800 + loudSamples) / 64 + 1; int b = 0;
            for (size_t s = 0; s < in.size(); s += 64, ++b)
            {
                for (int i = 0; i < 64; ++i) L[(size_t) i] = R[(size_t) i] = in[s + (size_t) i];
                float* ch[2] = { L.data(), R.data() }; e.process (ch, 2, 64, p);
                if (b > endBlock && e.grDb.load() > -0.5f) return (b - endBlock) * 64 * 1000.0 / SR;
            }
            return 9999.0;
        };
        const double clickMs = recoverMs (240), longMs = recoverMs ((int) (SR * 1.5));
        check (clickMs < longMs && clickMs < 200.0, "Auto Release: rapido no estalo, lento no trecho longo",
               f ("estalo 5 ms: solta em %.0f ms / trecho 1,5 s: solta em %.0f ms", clickMs, longMs));
    }

    // ---------- 6. True Peak ----------
    {
        std::vector<float> in ((size_t) SR);   // fs/4 com fase 45 graus: amostras 3 dB abaixo do pico real
        for (size_t i = 0; i < in.size(); ++i) in[i] = 0.98f * (float) std::sin (PI / 2 * (double) i + PI / 4);
        for (int tp = 0; tp < 2; ++tp)
        {
            LimiterEngine e; e.prepare (SR);
            LimiterParams p; p.threshold = -1.0f; p.ceiling = -1.0f; p.lookahead = 1; p.truePeak = tp;
            auto out = runLimiter (e, p, in);
            const float otp = dB (refTruePeak (out, 6000));
            if (tp) check (otp <= -0.85f, "True Peak ON: pico entre amostras fica no teto", f ("true peak de saida %.2f dBTP (teto -1.0)", otp));
            else    check (otp > -0.5f,   "True Peak OFF: passa do teto entre amostras (esperado)", f ("true peak de saida %.2f dBTP", otp));
        }
    }

    // ---------- 7. Desligado = som original atrasado (latencia constante) ----------
    {
        std::vector<float> in ((size_t) SR / 2); for (auto& v : in) v = 0.9f * nd (rng) * 0.3f;
        LimiterEngine e; e.prepare (SR);
        LimiterParams p; p.active = false; p.lookahead = 1; p.threshold = -12.0f;
        auto out = runLimiter (e, p, in);
        const int L = e.latencySamples(); float diff = 0;
        for (size_t i = (size_t) L; i < in.size(); ++i) diff = std::max (diff, std::abs (out[i] - in[i - (size_t) L]));
        check (diff == 0.0f, "Limiter desligado: som identico (so o atraso)", f ("diferenca maxima %.1g, atraso %.0f amostras", diff, L));
    }

    // ---------- 8. Lookahead deixa o corte mais limpo ----------
    {
        std::vector<float> in ((size_t) SR); for (size_t i = 0; i < in.size(); ++i) in[i] = 0.9f * (float) std::sin (2 * PI * 100 * (double) i / SR);
        double thd[2];
        for (int k = 0; k < 2; ++k)
        {
            LimiterEngine e; e.prepare (SR);
            LimiterParams p; p.threshold = -7.0f; p.ceiling = -1.0f; p.lookahead = k == 0 ? 0 : 1; p.autoRelease = false; p.releaseMs = 50.0f;
            auto out = runLimiter (e, p, in);
            thd[k] = harmonicRatio (out, 24000, 100.0, SR) * 100.0;
        }
        check (thd[1] < thd[0], "Lookahead 1 ms distorce menos que 0 ms", f ("distorcao: 0 ms = %.1f%% / 1 ms = %.1f%%", thd[0], thd[1]));
    }

    // ---------- 9. Analisador ----------
    {
        Analyzer a; a.prepare (SR); a.start (10.0f);
        std::vector<float> buf (480);
        double ph = 0;
        for (int b = 0; b < 1100; ++b)
        {
            for (auto& v : buf) { v = (float) std::sin (ph); ph += 2 * PI * 997.0 / SR; }
            const float* ch[1] = { buf.data() }; a.process (ch, 1, 480);
        }
        auto r = a.result();
        check (r.valid && std::abs (r.lufs - (-3.01f)) < 0.1f, "Loudness (BS.1770): seno 997 Hz 0 dBFS = -3.01 LUFS", f ("medido %.2f LUFS", r.lufs));
    }
    {
        Analyzer a; a.prepare (SR); a.start (10.0f);
        std::vector<float> buf (480); size_t n = 0;
        for (int b = 0; b < 1100; ++b)
        {
            for (auto& v : buf) { v = 0.98f * (float) std::sin (PI / 2 * (double) n + PI / 4); ++n; }
            const float* ch[1] = { buf.data() }; a.process (ch, 1, 480);
        }
        auto r = a.result();
        check (r.valid && std::abs (r.truePeakDb - dB (0.98f)) < 0.2f && r.peakDb < r.truePeakDb - 2.5f,
               "Analise: True Peak acima do pico normal", f ("pico %.2f dBFS / true peak %.2f dBTP", r.peakDb, r.truePeakDb));
    }
    {
        Analyzer a; a.prepare (SR); a.start (12.0f);
        std::vector<float> buf (480); size_t n = 0;
        for (int b = 0; b < 1300; ++b)
        {
            for (auto& v : buf)
            {
                const bool talk = (n / 48000) % 3 != 2;
                v = talk ? 0.08f * (float) std::sin (2 * PI * 220 * (double) n / SR) : LimiterEngine::fromDb (-58.0f) * nd (rng);
                ++n;
            }
            const float* ch[1] = { buf.data() }; a.process (ch, 1, 480);
        }
        auto r = a.result();
        check (r.valid && std::abs (r.noiseDb - (-58.0f)) < 2.0f, "Analise: ruido de fundo nas pausas", f ("medido %.1f dBFS (real -58)", r.noiseDb));
    }

    // ---------- 10. Cadeia completa: Auto Gain + Limiter ----------
    {
        AutoGainEngine ag; ag.prepare (SR);
        LimiterEngine lim; lim.prepare (SR);
        AutoGainParams ap; ap.peakProtection = false; ap.detection = 2; ap.ceiling = -1.0f;
        LimiterParams lp; lp.threshold = -1.0f; lp.ceiling = -1.0f; lp.lookahead = 1;
        std::vector<float> L (128), R (128); float mx = 0; size_t n = 0;
        for (int b = 0; b < (int) (SR * 15 / 128); ++b)
        {
            for (int i = 0; i < 128; ++i, ++n)
            {
                float v = 0.045f * (float) std::sin (2 * PI * 220 * (double) n / SR) * (0.75f + 0.25f * (float) std::sin (2 * PI * 4 * (double) n / SR));
                if (n % 30000 < 60) v += 0.7f;   // golpe no microfone
                L[(size_t) i] = R[(size_t) i] = v;
            }
            float* ch[2] = { L.data(), R.data() };
            ag.process (ch, 2, 128, ap);
            lim.process (ch, 2, 128, lp);
            if (n > SR * 2) for (float v : L) mx = std::max (mx, std::abs (v));
        }
        check (mx <= LimiterEngine::fromDb (-1.0f) + 1e-6f, "Cadeia Auto Gain -> Limiter: nada passa de -1 dBFS",
               f ("pico %.2f dBFS, ganho do Auto Gain %+.1f dB", dB (mx), ag.gainDb.load()));
    }

    // ---------- 11. CPU com 20 tracks ----------
    std::printf ("\n  Processamento com 20 tracks estereo (48 kHz, buffer 128, 60 s de audio):\n");
    for (int cfg = 0; cfg < 3; ++cfg)
    {
        const int tracks = 20, block = 128; const double seconds = 60;
        std::vector<AutoGainEngine> ags (tracks); std::vector<LimiterEngine> lims (tracks);
        for (auto& e : ags) e.prepare (SR);
        for (auto& e : lims) e.prepare (SR);
        AutoGainParams ap; ap.peakProtection = false;
        LimiterParams lp; lp.threshold = -6.0f;
        lp.lookahead = cfg == 0 ? 0 : (cfg == 1 ? 1 : 3); lp.truePeak = cfg == 2;
        std::vector<float> L (block), R (block);
        double total = 0;
        for (int b = 0; b < (int) (seconds * SR / block); ++b)
            for (int t = 0; t < tracks; ++t)
            {
                for (int i = 0; i < block; ++i) { L[(size_t) i] = 0.1f * nd (rng); R[(size_t) i] = 0.1f * nd (rng); }
                float* ch[2] = { L.data(), R.data() };
                const auto t0 = std::chrono::steady_clock::now();
                ags[(size_t) t].process (ch, 2, block, ap);
                lims[(size_t) t].process (ch, 2, block, lp);
                total += std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            }
        const char* names[] = { "Lookahead 0 ms (ao vivo)", "Lookahead 1 ms (padrao)", "Lookahead 5 ms + True Peak" };
        if (cfg == 2) { (void) 0; }
        const int lat = LimiterEngine::latencyFor (SR, lp.lookahead, lp.truePeak);
        std::printf ("    %-28s CPU %.2f%% de 1 nucleo (%.3f%%/track) | latencia por track %.2f ms (%d amostras)\n",
                     names[cfg], 100 * total / seconds, 100 * total / seconds / tracks, lat * 1000.0 / SR, lat);
    }

    std::printf ("\n%s: %d falha(s)\n\n", failures == 0 ? "RESULTADO: TUDO OK" : "RESULTADO: COM FALHAS", failures);
    return failures == 0 ? 0 : 1;
}
