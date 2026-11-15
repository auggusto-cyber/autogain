#pragma once

// Leitura do dia (aba Analise) - C++ puro, sem JUCE.
// Mede na ENTRADA do plugin: pico, true peak, RMS, loudness (LUFS, ITU-R BS.1770),
// dinamica (pico - RMS) e ruido de fundo (janelas de 100 ms mais silenciosas).

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

class Analyzer
{
public:
    enum State { idle = 0, running = 1, done = 2 };

    struct Result
    {
        float peakDb = -100.0f, truePeakDb = -100.0f, rmsDb = -100.0f;
        float lufs = -100.0f, noiseDb = -100.0f;
        bool  valid = false;
        float dynamicsDb() const { return peakDb - rmsDb; }
    };

    std::atomic<int>   state { idle };
    std::atomic<float> progress { 0.0f };

    static float toDb (double lin)    { return (float) (20.0 * std::log10 (lin + 1.0e-9)); }
    static float msToDb (double ms)   { return (float) (10.0 * std::log10 (ms + 1.0e-12)); }

    void prepare (double sampleRate)
    {
        sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        win = std::max (1, (int) std::lround (sr * 0.1));
        designKWeighting();
        buildTpFilter();
    }

    void start (float seconds)
    {
        targetSamples.store ((int64_t) (std::clamp (seconds, 5.0f, 120.0f) * sr));
        progress.store (0.0f);
        startRequested.store (true);
        state.store (running);
    }

    void cancel()
    {
        startRequested.store (false);
        state.store (idle);
        progress.store (0.0f);
    }

    // Chamado na thread de audio, antes de qualquer processamento.
    void process (const float* const* ch, int numCh, int n)
    {
        if (startRequested.exchange (false))
            reset();
        if (state.load() != running || numCh <= 0)
            return;

        numCh = std::min (numCh, 2);
        const int64_t target = targetSamples.load();

        for (int i = 0; i < n && samples < target; ++i, ++samples)
        {
            double sq = 0.0, kSum = 0.0;
            for (int c = 0; c < numCh; ++c)
            {
                const double x = ch[c][i];
                sq = std::max (sq, x * x);
                peak = std::max (peak, std::abs (x));

                // true peak (4x)
                auto& h = tpHist[(size_t) c];
                tpHead[c] = (tpHead[c] + 1) & 15;
                h[(size_t) tpHead[c]] = (float) x;
                for (int k = 1; k < 4; ++k)
                {
                    double y = 0.0;
                    for (int t = 0; t < 13; ++t)
                        y += tpCoef[k][t] * h[(size_t) ((tpHead[c] - t) & 15)];
                    truePeak = std::max (truePeak, std::abs (y));
                }

                // filtro K (BS.1770): shelf + passa-altas
                auto& s = kState[(size_t) c];
                const double y1 = kb[0][0] * x + s[0];
                s[0] = kb[0][1] * x - ka[0][1] * y1 + s[1];
                s[1] = kb[0][2] * x - ka[0][2] * y1;
                const double y2 = kb[1][0] * y1 + s[2];
                s[2] = kb[1][1] * y1 - ka[1][1] * y2 + s[3];
                s[3] = kb[1][2] * y1 - ka[1][2] * y2;
                kSum += y2 * y2;
            }
            energy += sq;
            winEnergy += sq;
            winK += kSum;

            if (++winCount >= win)
            {
                if (nWin < maxWindows)
                {
                    winMs[(size_t) nWin] = winEnergy / winCount;
                    winKms[(size_t) nWin] = winK / winCount;
                    ++nWin;
                }
                winEnergy = winK = 0.0;
                winCount = 0;
            }
        }

        truePeak = std::max (truePeak, peak);
        progress.store (target > 0 ? (float) samples / (float) target : 1.0f);
        if (samples >= target)
            state.store (done);
    }

    // Chamar na thread da interface depois que state == done.
    Result result() const
    {
        Result r;
        if (state.load() != done || samples <= 0)
            return r;

        r.peakDb     = toDb (peak);
        r.truePeakDb = toDb (truePeak);
        r.rmsDb      = msToDb (energy / (double) samples);

        // ruido de fundo: janela de 100 ms no percentil 10
        if (nWin > 0)
        {
            std::vector<double> w (winMs.begin(), winMs.begin() + nWin);
            const auto k = (size_t) std::max (0, (int) ((double) nWin * 0.1));
            std::nth_element (w.begin(), w.begin() + (long) k, w.end());
            r.noiseDb = msToDb (w[k]);
        }

        // loudness integrado: blocos de 400 ms (4 janelas, passo de 100 ms),
        // porta absoluta -70 LUFS e porta relativa -10 LU
        std::vector<double> blocks;
        for (int b = 0; b + 3 < nWin; ++b)
            blocks.push_back ((winKms[(size_t) b] + winKms[(size_t) b + 1] + winKms[(size_t) b + 2] + winKms[(size_t) b + 3]) / 4.0);
        auto lufsOf = [] (double ms) { return -0.691 + 10.0 * std::log10 (ms + 1.0e-12); };
        double sum = 0.0; int cnt = 0;
        for (double z : blocks) if (lufsOf (z) > -70.0) { sum += z; ++cnt; }
        if (cnt > 0)
        {
            const double rel = lufsOf (sum / cnt) - 10.0;
            double s2 = 0.0; int c2 = 0;
            for (double z : blocks) if (lufsOf (z) > -70.0 && lufsOf (z) > rel) { s2 += z; ++c2; }
            r.lufs = (float) lufsOf (c2 > 0 ? s2 / c2 : sum / cnt);
        }
        else
            r.lufs = -70.0f;

        r.valid = true;
        return r;
    }

private:
    void reset()
    {
        samples = 0; energy = winEnergy = winK = 0.0; winCount = 0; nWin = 0;
        peak = truePeak = 0.0;
        for (auto& s : kState) s.fill (0.0);
        for (auto& h : tpHist) h.fill (0.0f);
        tpHead[0] = tpHead[1] = 0;
    }

    void designKWeighting()
    {
        constexpr double pi = 3.14159265358979323846;
        // estagio 1: shelf de agudos (+4 dB)
        {
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K = std::tan (pi * f0 / sr);
            const double Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            kb[0][0] = (Vh + Vb * K / Q + K * K) / a0;
            kb[0][1] = 2.0 * (K * K - Vh) / a0;
            kb[0][2] = (Vh - Vb * K / Q + K * K) / a0;
            ka[0][1] = 2.0 * (K * K - 1.0) / a0;
            ka[0][2] = (1.0 - K / Q + K * K) / a0;
        }
        // estagio 2: passa-altas (~38 Hz)
        {
            const double f0 = 38.13547087602444, Q = 0.5003270373238773;
            const double K = std::tan (pi * f0 / sr);
            const double a0 = 1.0 + K / Q + K * K;
            kb[1][0] = 1.0; kb[1][1] = -2.0; kb[1][2] = 1.0;
            ka[1][1] = 2.0 * (K * K - 1.0) / a0;
            ka[1][2] = (1.0 - K / Q + K * K) / a0;
        }
    }

    void buildTpFilter()
    {
        constexpr double pi = 3.14159265358979323846;
        for (int k = 0; k < 4; ++k)
        {
            double sum = 0.0;
            for (int t = 0; t < 13; ++t)
            {
                const double x = (double) (6 - t) + (double) k / 4.0;
                const double sinc = std::abs (x) < 1e-9 ? 1.0 : std::sin (pi * x) / (pi * x);
                const double a = std::abs (x) / 6.5;
                const double w = a >= 1.0 ? 0.0
                               : 0.35875 + 0.48829 * std::cos (pi * a) + 0.14128 * std::cos (2.0 * pi * a)
                                 + 0.01168 * std::cos (3.0 * pi * a);
                tpCoef[k][t] = sinc * w;
                sum += sinc * w;
            }
            for (int t = 0; t < 13; ++t) tpCoef[k][t] /= sum;
        }
    }

    double sr = 48000.0;
    int win = 4800;

    std::atomic<bool>    startRequested { false };
    std::atomic<int64_t> targetSamples { 0 };
    int64_t samples = 0;
    double energy = 0.0, winEnergy = 0.0, winK = 0.0;
    int winCount = 0, nWin = 0;
    double peak = 0.0, truePeak = 0.0;

    static constexpr int maxWindows = 1300;
    std::array<double, maxWindows> winMs {}, winKms {};

    double kb[2][3] {}, ka[2][3] {};
    std::array<std::array<double, 4>, 2> kState {};

    double tpCoef[4][13] {};
    std::array<std::array<float, 16>, 2> tpHist {};
    int tpHead[2] { 0, 0 };
};
