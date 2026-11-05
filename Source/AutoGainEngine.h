#pragma once

// Auto Gain by Augusto Souza - motor de processamento (C++ puro, sem JUCE)
// Latencia zero: nenhum lookahead, o ganho e calculado e aplicado no mesmo bloco.

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

struct AutoGainParams
{
    float target   = -18.0f;  // dBFS
    int   detection = 2;      // 0 = Pico, 1 = RMS, 2 = Pico + RMS
    int   response  = 0;      // 0 = Lenta (ao vivo), 1 = Media, 2 = Rapida
    float maxBoost = 12.0f;   // dB
    float maxCut   = 24.0f;   // dB
    float gate     = -50.0f;  // dBFS - abaixo disso o ganho congela
    float ceiling  = -1.0f;   // dBFS - teto da protecao de pico
    bool  active   = true;
    bool  hold     = false;
};

class AutoGainEngine
{
public:
    enum AnalysisState { idle = 0, running = 1, done = 2 };
    struct AnalysisResult { float peakDb = -100.0f, rmsDb = -100.0f, noiseDb = -100.0f; bool valid = false; };

    // ---- medidores (lidos pela interface) ----
    std::atomic<float> inRmsDb  { -100.0f }, inPeakDb  { -100.0f };
    std::atomic<float> outRmsDb { -100.0f }, outPeakDb { -100.0f };
    std::atomic<float> gainDb { 0.0f }, limiterGrDb { 0.0f };
    std::atomic<bool>  gateClosed { false };

    std::atomic<int>   analysisState { idle };
    std::atomic<float> analysisProgress { 0.0f };

    static float toDb (float lin)  { return 20.0f * std::log10 (lin + 1.0e-9f); }
    static float msToDb (float ms) { return 10.0f * std::log10 (ms + 1.0e-12f); }
    static float fromDb (float db) { return std::pow (10.0f, db / 20.0f); }

    void prepare (double sampleRate)
    {
        sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        rmsCoef       = coefFor (300.0f);   // janela RMS ~300 ms
        peakRelCoef   = coefFor (1000.0f);  // detector de pico: descida de 1 s
        meterPeakCoef = coefFor (300.0f);   // medidores
        limRelCoef    = coefFor (50.0f);    // retorno da protecao de pico
        winLength = std::max (1, (int) (sr * 0.1));
        inMs = inPk = outMs = outPk = 0.0f;
        limEnv = 1.0f;
        lastGainLin = fromDb (currentGainDb);
    }

    // Aplica um ganho imediatamente (botao "Aplicar ajustes")
    void jumpGainTo (float db) { pendingJumpDb.store (db); jumpRequested.store (true); }

    void startAnalysis (float seconds)
    {
        const float secs = std::clamp (seconds, 5.0f, 120.0f);
        analysisTargetSamples.store ((int64_t) (secs * sr));
        analysisProgress.store (0.0f);
        analysisStartRequested.store (true);
        analysisState.store (running);
    }

    void cancelAnalysis()
    {
        analysisStartRequested.store (false);
        analysisState.store (idle);
        analysisProgress.store (0.0f);
    }

    AnalysisResult getAnalysisResult() const
    {
        AnalysisResult r;
        const int n = std::min (windowCount.load(), maxWindows);
        if (analysisState.load() != done || n <= 0)
            return r;

        r.peakDb = resultPeakDb.load();
        r.rmsDb  = resultRmsDb.load();

        // ruido de fundo = janela de 100 ms no percentil 10 (as mais silenciosas)
        std::vector<float> w (windowDb.begin(), windowDb.begin() + n);
        const auto k = (size_t) std::max (0, (int) ((float) n * 0.1f));
        std::nth_element (w.begin(), w.begin() + (long) k, w.end());
        r.noiseDb = w[k];
        r.valid = true;
        return r;
    }

    // Processa no lugar. channels[c][i], numCh >= 1.
    void process (float* const* channels, int numCh, int n, const AutoGainParams& p)
    {
        if (numCh <= 0 || n <= 0)
            return;

        static constexpr float downT[] = { 1.0f, 0.3f, 0.08f };  // segundos para baixar
        static constexpr float upT[]   = { 3.0f, 1.0f, 0.30f };  // segundos para subir
        const int   resp  = std::clamp (p.response, 0, 2);
        const float tDown = downT[resp], tUp = upT[resp];
        const float ceilLin = fromDb (p.ceiling);

        if (analysisStartRequested.exchange (false))
        {
            analysisSamples = 0; energySum = 0.0; winEnergy = 0.0; winSamples = 0;
            maxPeakLin = 0.0f; windowCount.store (0);
        }
        const bool    analysing      = analysisState.load() == running;
        const int64_t analysisTarget = analysisTargetSamples.load();

        if (jumpRequested.exchange (false))
            currentGainDb = std::clamp (pendingJumpDb.load(), -p.maxCut, p.maxBoost);

        bool  gateNow = false;
        float minLim  = 1.0f;
        constexpr int chunk = 32;

        for (int start = 0; start < n; start += chunk)
        {
            const int len = std::min (chunk, n - start);

            // 1. detectores na entrada
            for (int i = 0; i < len; ++i)
            {
                float sq = 0.0f, pk = 0.0f;
                for (int c = 0; c < numCh; ++c)
                {
                    const float x = channels[c][start + i];
                    sq = std::max (sq, x * x);
                    pk = std::max (pk, std::abs (x));
                }
                inMs = rmsCoef * inMs + (1.0f - rmsCoef) * sq;
                inPk = std::max (pk, inPk * peakRelCoef);

                if (analysing && analysisSamples < analysisTarget)
                {
                    energySum += sq;
                    winEnergy += sq;
                    maxPeakLin = std::max (maxPeakLin, pk);
                    ++analysisSamples;
                    if (++winSamples >= winLength)
                    {
                        const int idx = windowCount.load();
                        if (idx < maxWindows)
                        {
                            windowDb[(size_t) idx] = msToDb ((float) (winEnergy / winSamples));
                            windowCount.store (idx + 1);
                        }
                        winEnergy = 0.0; winSamples = 0;
                    }
                }
            }

            const float rmsDb = msToDb (inMs);
            const float pkDb  = toDb (inPk);

            // 2. ganho desejado
            gateNow = rmsDb < p.gate;
            float desired = currentGainDb;

            if (! p.active)
                desired = 0.0f;
            else if (! gateNow && ! p.hold)
            {
                switch (p.detection)
                {
                    case 0:  desired = p.target - pkDb;  break;
                    case 1:  desired = p.target - rmsDb; break;
                    default: desired = std::min (p.target - rmsDb, p.ceiling - pkDb); break;
                }
                desired = std::clamp (desired, -p.maxCut, p.maxBoost);
            }

            // 3. suavizacao
            if (! p.active)
                currentGainDb = 0.0f;
            else
            {
                const float tau = desired < currentGainDb ? tDown : tUp;
                const float k   = std::exp (-(float) len / (tau * (float) sr));
                currentGainDb   = desired + k * (currentGainDb - desired);
            }

            // 4. aplica ganho + protecao de pico (sem lookahead)
            const float newLin = fromDb (currentGainDb);
            const float step   = (newLin - lastGainLin) / (float) len;

            for (int i = 0; i < len; ++i)
            {
                const float g = lastGainLin + step * (float) (i + 1);
                float a = 0.0f;
                for (int c = 0; c < numCh; ++c)
                    a = std::max (a, std::abs (channels[c][start + i] * g));

                float lim = 1.0f;
                if (p.active)
                {
                    const float needed = a > ceilLin ? ceilLin / a : 1.0f;
                    limEnv = needed < limEnv ? needed : needed + (limEnv - needed) * limRelCoef;
                    lim = limEnv;
                    minLim = std::min (minLim, limEnv);
                }

                float sq = 0.0f, pk = 0.0f;
                for (int c = 0; c < numCh; ++c)
                {
                    float y = channels[c][start + i] * g * lim;
                    if (p.active)
                        y = std::clamp (y, -ceilLin, ceilLin);  // trava final de seguranca
                    channels[c][start + i] = y;
                    sq = std::max (sq, y * y);
                    pk = std::max (pk, std::abs (y));
                }
                outMs = rmsCoef * outMs + (1.0f - rmsCoef) * sq;
                outPk = std::max (pk, outPk * meterPeakCoef);
            }
            lastGainLin = newLin;
        }

        if (analysing)
        {
            analysisProgress.store (analysisTarget > 0 ? (float) analysisSamples / (float) analysisTarget : 1.0f);
            if (analysisSamples >= analysisTarget && analysisSamples > 0)
            {
                resultPeakDb.store (toDb (maxPeakLin));
                resultRmsDb.store  (msToDb ((float) (energySum / (double) analysisSamples)));
                analysisState.store (done);
            }
        }

        inRmsDb.store  (msToDb (inMs));
        inPeakDb.store (toDb (inPk));
        outRmsDb.store (msToDb (outMs));
        outPeakDb.store (toDb (outPk));
        gainDb.store (currentGainDb);
        limiterGrDb.store (toDb (minLim));
        gateClosed.store (p.active && gateNow);
    }

private:
    float coefFor (float ms) const { return std::exp (-1.0f / (0.001f * ms * (float) sr)); }

    double sr = 48000.0;
    float inMs = 0.0f, inPk = 0.0f, outMs = 0.0f, outPk = 0.0f;
    float currentGainDb = 0.0f, lastGainLin = 1.0f, limEnv = 1.0f;
    float rmsCoef = 0.0f, peakRelCoef = 0.0f, meterPeakCoef = 0.0f, limRelCoef = 0.0f;

    std::atomic<float> pendingJumpDb { 0.0f };
    std::atomic<bool>  jumpRequested { false };

    static constexpr int maxWindows = 1300;  // ate 130 s em janelas de 100 ms
    std::array<float, maxWindows> windowDb {};
    std::atomic<int>     windowCount { 0 };
    std::atomic<bool>    analysisStartRequested { false };
    std::atomic<int64_t> analysisTargetSamples { 0 };
    int64_t analysisSamples = 0;
    double  energySum = 0.0, winEnergy = 0.0;
    int     winSamples = 0, winLength = 4800;
    float   maxPeakLin = 0.0f;
    std::atomic<float> resultPeakDb { -100.0f }, resultRmsDb { -100.0f };
};
