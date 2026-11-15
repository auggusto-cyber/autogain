#pragma once

// Limiter by Augusto Souza - motor de processamento (C++ puro, sem JUCE)
//
// Brickwall com lookahead (0 / 1 / 3 / 5 ms), release manual ou automatico
// e deteccao True Peak (interpolacao 4x). Estilo L2: o ganho de saida e
// (Ceiling - Threshold) e nada passa do Ceiling.
//
// Como funciona (por amostra, no "tempo de deteccao" p):
//   1. g_req(p) = Ceiling / pico(p)  (1 se o pico esta abaixo do teto)
//   2. m(p)     = minimo de g_req nas ultimas L+1 posicoes
//   3. r(p)     = envelope com ataque instantaneo e release suave
//   4. b(p)     = media de r nas ultimas L+1 posicoes (ataque suave)
//   5. saida    = x(p - L) * ganho * b(p)
// Cada r(s) do passo 4 e <= g_req(p - L), entao a saida nunca passa do teto.

#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>

struct LimiterParams
{
    float threshold   = 0.0f;    // dB, -30..0 (abaixar sobe o volume)
    float ceiling     = -1.0f;   // dBFS, -12..0
    float releaseMs   = 100.0f;  // 1..1000 ms (modo manual)
    bool  autoRelease = true;
    int   lookahead   = 1;       // 0 = 0 ms, 1 = 1 ms, 2 = 3 ms, 3 = 5 ms
    bool  truePeak    = false;   // so atua com lookahead > 0
    bool  active      = true;    // desligado: passa o som (com o mesmo atraso)
};

class LimiterEngine
{
public:
    static constexpr int   numLookaheads = 4;
    static constexpr float lookaheadMs[numLookaheads] = { 0.0f, 1.0f, 3.0f, 5.0f };
    static constexpr int   tpDelay = 6;      // atraso extra do detector True Peak (amostras)
    static constexpr int   maxChannels = 2;

    // ---- medidores ----
    std::atomic<float> grDb { 0.0f };            // atenuacao atual (<= 0)
    std::atomic<float> grPeakSinceRead { 0.0f }; // maior atenuacao desde a ultima leitura (<= 0)
    std::atomic<float> outPeakDb { -100.0f };
    std::atomic<float> outPeakChDb[maxChannels] { { -100.0f }, { -100.0f } };   // L / R

    static float toDb (float lin)  { return 20.0f * std::log10 (lin + 1.0e-9f); }
    static float fromDb (float db) { return std::pow (10.0f, db / 20.0f); }

    static int latencyFor (double sampleRate, int lookahead, bool truePeak)
    {
        const int la = std::clamp (lookahead, 0, numLookaheads - 1);
        const int L  = (int) std::lround (lookaheadMs[la] * 0.001 * sampleRate);
        return L + ((truePeak && L > 0) ? tpDelay : 0);
    }

    void prepare (double sampleRate)
    {
        sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        const int maxL = (int) std::lround (lookaheadMs[numLookaheads - 1] * 0.001 * sr);
        cap = maxL + tpDelay + 16;
        for (auto& d : delay) d.assign ((size_t) cap, 0.0f);
        for (auto& h : tpHist) h.assign (16, 0.0f);
        dqIdx.assign ((size_t) cap, 0);
        dqVal.assign ((size_t) cap, 1.0f);
        boxRing.assign ((size_t) cap, 1.0);
        buildTpFilter();
        configured = false;
    }

    int latencySamples() const { return L + D; }

    // Ganho maximo que o limiter aplica antes do teto, em dB (Ceiling - Threshold)
    static float makeupDb (const LimiterParams& p) { return p.ceiling - p.threshold; }

    void process (float* const* ch, int numCh, int n, const LimiterParams& p)
    {
        numCh = std::min (numCh, maxChannels);
        if (numCh <= 0 || n <= 0) return;

        const int la = std::clamp (p.lookahead, 0, numLookaheads - 1);
        if (! configured || la != curLookahead || p.truePeak != curTruePeak)
            configure (la, p.truePeak);

        const float ceilLin = fromDb (p.ceiling);
        const float pre     = fromDb (makeupDb (p));
        const int   W       = L + 1;
        float blockMinGain  = 1.0f;
        float outPk         = 0.0f;
        float outPkCh[maxChannels] { 0.0f, 0.0f };

        for (int i = 0; i < n; ++i, ++pos)
        {
            // ---- 1. guarda a entrada (linha de atraso) e mede o pico na posicao de deteccao ----
            float v = 0.0f;
            for (int c = 0; c < numCh; ++c)
            {
                const float x = ch[c][i];
                delay[(size_t) c][(size_t) (pos % cap)] = x;

                if (D == 0)
                    v = std::max (v, std::abs (x));
                else
                {
                    auto& h = tpHist[(size_t) c];
                    tpHead[c] = (tpHead[c] + 1) & 15;
                    h[(size_t) tpHead[c]] = x;
                    // pontos interpolados em p, p+0.25, p+0.5, p+0.75 (p = agora - 6)
                    // k = 0 e a propria amostra p (filtro identidade): nao precisa calcular
                    float seg = std::abs (h[(size_t) ((tpHead[c] - 6) & 15)]);
                    for (int k = 1; k < 4; ++k)
                    {
                        float y = 0.0f;
                        for (int t = 0; t < tpTaps; ++t)
                            y += tpCoef[k][t] * h[(size_t) ((tpHead[c] - t) & 15)];
                        seg = std::max (seg, std::abs (y));
                    }
                    v = std::max (v, std::max (seg, tpPrevSeg[c]));   // cobre [p-1, p+1)
                    tpPrevSeg[c] = seg;
                }
            }
            v *= pre;

            // ---- 2. ganho necessario nesta posicao ----
            const float gReq = v > ceilLin ? ceilLin / v : 1.0f;

            // ---- 3. minimo deslizante (janela de L+1 posicoes) ----
            while (dqCount > 0 && dqVal[(size_t) ((dqHead + dqCount - 1) % cap)] >= gReq)
                --dqCount;
            const int slot = (dqHead + dqCount) % cap;
            dqVal[(size_t) slot] = gReq;
            dqIdx[(size_t) slot] = pos;
            ++dqCount;
            while (dqIdx[(size_t) dqHead] <= pos - W) { dqHead = (dqHead + 1) % cap; --dqCount; }
            const float m = dqVal[(size_t) dqHead];

            // ---- 4. envelope: ataque instantaneo, release suave ----
            if ((pos & 31) == 0)
                relCoef = std::exp (-1.0f / (0.001f * currentReleaseMs (p) * (float) sr));
            env = m < env ? m : m + (env - m) * relCoef;

            // memoria da atenuacao longa (para o release automatico)
            const float envDb = toDb (env);
            grAvgDb = grAvgCoef * grAvgDb + (1.0f - grAvgCoef) * envDb;

            // ---- 5. media movel (ataque suave dentro do lookahead) ----
            float g = env;
            if (L > 0)
            {
                boxSum += (double) env - boxRing[(size_t) (pos % W)];
                boxRing[(size_t) (pos % W)] = env;
                g = (float) (boxSum / (double) W);
                if ((pos & 4095) == 0)   // corrige o acumulo de erro numerico
                {
                    double s = 0.0;
                    for (int k = 0; k < W; ++k) s += boxRing[(size_t) k];
                    boxSum = s;
                }
                g = std::min (g, 1.0f);
            }

            // ---- 6. saida atrasada ----
            const long long rd = pos - (L + D);
            for (int c = 0; c < numCh; ++c)
            {
                const float xd = rd >= 0 ? delay[(size_t) c][(size_t) (rd % cap)] : 0.0f;
                float y = xd;
                if (p.active)
                {
                    y = xd * pre * g;
                    y = std::clamp (y, -ceilLin, ceilLin);   // trava final (erro numerico)
                }
                ch[c][i] = y;
                outPk = std::max (outPk, std::abs (y));
                outPkCh[c] = std::max (outPkCh[c], std::abs (y));
            }
            blockMinGain = std::min (blockMinGain, p.active ? g : 1.0f);
        }

        const float gr = toDb (blockMinGain);
        grDb.store (gr);
        float prev = grPeakSinceRead.load();
        while (gr < prev && ! grPeakSinceRead.compare_exchange_weak (prev, gr)) {}
        outPeakDb.store (toDb (outPk));
        for (int c = 0; c < maxChannels; ++c)
            outPeakChDb[c].store (toDb (c < numCh ? outPkCh[c] : outPkCh[0]));
    }

private:
    float currentReleaseMs (const LimiterParams& p) const
    {
        if (! p.autoRelease)
            return std::clamp (p.releaseMs, 1.0f, 1000.0f);
        // pico isolado: 30 ms (recupera rapido) / atenuacao longa: ate 400 ms (sem bombear)
        const float sustained = std::clamp (-grAvgDb / 6.0f, 0.0f, 1.0f);
        return 30.0f + (400.0f - 30.0f) * sustained;
    }

    void configure (int la, bool tp)
    {
        curLookahead = la;
        curTruePeak  = tp;
        L = (int) std::lround (lookaheadMs[la] * 0.001 * sr);
        D = (tp && L > 0) ? tpDelay : 0;
        for (auto& d : delay) std::fill (d.begin(), d.end(), 0.0f);
        for (auto& h : tpHist) std::fill (h.begin(), h.end(), 0.0f);
        std::fill (boxRing.begin(), boxRing.end(), 1.0);
        boxSum = (double) (L + 1);
        dqHead = 0; dqCount = 0;
        env = 1.0f;
        tpHead[0] = tpHead[1] = 0;
        tpPrevSeg[0] = tpPrevSeg[1] = 0.0f;
        grAvgDb = 0.0f;
        grAvgCoef = std::exp (-1.0f / (0.4f * (float) sr));   // memoria de ~400 ms
        relCoef = std::exp (-1.0f / (0.1f * (float) sr));
        pos = 0;
        configured = true;
    }

    // Interpolacao 4x: sinc com janela Blackman-Harris, 13 pontos, atraso de 6 amostras
    void buildTpFilter()
    {
        constexpr double pi = 3.14159265358979323846;
        for (int k = 0; k < 4; ++k)
        {
            double sum = 0.0;
            for (int t = 0; t < tpTaps; ++t)
            {
                // ponto interpolado no tempo (agora - 6 + k/4); amostra (agora - t)
                const double x = (double) (6 - t) + (double) k / 4.0;   // distancia ate a amostra
                const double sinc = std::abs (x) < 1e-9 ? 1.0 : std::sin (pi * x) / (pi * x);
                const double a = std::abs (x) / 6.5;
                const double w = a >= 1.0 ? 0.0
                               : 0.35875 + 0.48829 * std::cos (pi * a) + 0.14128 * std::cos (2.0 * pi * a)
                                 + 0.01168 * std::cos (3.0 * pi * a);
                tpCoef[k][t] = (float) (sinc * w);
                sum += sinc * w;
            }
            for (int t = 0; t < tpTaps; ++t)
                tpCoef[k][t] = (float) (tpCoef[k][t] / sum);
        }
    }

    double sr = 48000.0;
    int cap = 1024, L = 0, D = 0;
    int curLookahead = -1;
    bool curTruePeak = false, configured = false;
    long long pos = 0;

    std::vector<float> delay[maxChannels];

    // minimo deslizante (fila monotona em anel)
    std::vector<long long> dqIdx;
    std::vector<float> dqVal;
    int dqHead = 0, dqCount = 0;

    // media movel
    std::vector<double> boxRing;
    double boxSum = 1.0;

    float env = 1.0f, relCoef = 0.0f, grAvgDb = 0.0f, grAvgCoef = 0.0f;

    // True Peak
    static constexpr int tpTaps = 13;
    float tpCoef[4][tpTaps] {};
    std::vector<float> tpHist[maxChannels];
    int tpHead[maxChannels] { 0, 0 };
    float tpPrevSeg[maxChannels] { 0.0f, 0.0f };
};
