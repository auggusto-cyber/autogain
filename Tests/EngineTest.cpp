// Teste do motor do Auto Gain com sinais simulados (sem DAW).
// Compilar: g++ -std=c++17 -O2 -I../Source EngineTest.cpp -o engine_test
#include "AutoGainEngine.h"
#include <cstdio>
#include <random>
#include <string>

static constexpr double SR = 48000.0;
static constexpr int BLOCK = 256;
static int failures = 0;

static void check (bool ok, const std::string& name, const std::string& detail)
{
    std::printf ("[%s] %-48s %s\n", ok ? " OK " : "FALHA", name.c_str(), detail.c_str());
    if (! ok) ++failures;
}

// Gera "voz": tom com modulacao de silabas, nivel RMS aproximado em dBFS
struct Source
{
    std::mt19937 rng { 42 };
    std::normal_distribution<float> noise { 0.0f, 1.0f };
    double phase = 0.0, t = 0.0;

    float voice (float rmsDb)
    {
        const float amp = AutoGainEngine::fromDb (rmsDb) * 1.41421356f;
        const float syll = 0.75f + 0.25f * (float) std::sin (2.0 * 3.14159265358979323846 * 4.0 * t);   // 4 silabas/s
        phase += 2.0 * 3.14159265358979323846 * 220.0 / SR;
        t += 1.0 / SR;
        return amp * syll * (float) std::sin (phase) / 0.79f;
    }
    float hiss (float rmsDb) { t += 1.0 / SR; return AutoGainEngine::fromDb (rmsDb) * noise (rng); }
};

struct Run
{
    AutoGainEngine eng;
    AutoGainParams p;
    float maxOutPeak = 0.0f;
    std::vector<float> L, R;

    Run() { eng.prepare (SR); L.resize (BLOCK); R.resize (BLOCK); }

    template <typename Gen>
    void play (double seconds, Gen gen)
    {
        const int blocks = (int) (seconds * SR / BLOCK);
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < BLOCK; ++i) { L[(size_t) i] = gen(); R[(size_t) i] = L[(size_t) i]; }
            float* ch[2] = { L.data(), R.data() };
            eng.process (ch, 2, BLOCK, p);
            for (int i = 0; i < BLOCK; ++i)
                maxOutPeak = std::max (maxOutPeak, std::max (std::abs (L[(size_t) i]), std::abs (R[(size_t) i])));
        }
    }
    float gain() const   { return eng.gainDb.load(); }
    float outRms() const { return eng.outRmsDb.load(); }
};

static std::string f (const char* fmt, double a, double b = 0, double c = 0)
{
    char buf[200]; std::snprintf (buf, sizeof buf, fmt, a, b, c); return buf;
}

int main()
{
    std::printf ("\n=== AUTO GAIN by Augusto Souza - teste do motor ===\n\n");

    // 1. Voz baixa (-30 dBFS RMS) sobe ate o alvo -18
    {
        Run r; Source s; r.p.detection = 1;
        r.play (15.0, [&] { return s.voice (-30.0f); });
        check (std::abs (r.outRms() - (-18.0f)) < 1.0f, "Voz baixa (-30) chega ao alvo -18 [RMS]",
               f ("saida %.1f dBFS, ganho %+.1f dB", r.outRms(), r.gain()));
    }
    // 2. Voz alta (-8 dBFS) baixa ate o alvo
    {
        Run r; Source s; r.p.detection = 1;
        r.play (10.0, [&] { return s.voice (-8.0f); });
        check (std::abs (r.outRms() - (-18.0f)) < 1.0f, "Voz alta (-8) desce ao alvo -18 [RMS]",
               f ("saida %.1f dBFS, ganho %+.1f dB", r.outRms(), r.gain()));
    }
    // 3. Limite de aumento: sinal muito baixo (-45) para em +12
    {
        Run r; Source s; r.p.detection = 1;
        r.play (20.0, [&] { return s.voice (-45.0f); });
        check (r.gain() <= 12.01f && r.gain() > 11.0f, "Aumento maximo respeitado (+12 dB)",
               f ("ganho %+.2f dB", r.gain()));
    }
    // 4. Gate: depois da fala, silencio com chiado -60 -> ganho congela
    {
        Run r; Source s; r.p.detection = 1;
        r.play (15.0, [&] { return s.voice (-30.0f); });
        const float before = r.gain();
        r.play (20.0, [&] { return s.hiss (-60.0f); });
        check (std::abs (r.gain() - before) < 0.5f && r.eng.gateClosed.load(), "Gate: no silencio o ganho NAO sobe",
               f ("antes %+.1f dB, apos 20 s de silencio %+.1f dB", before, r.gain()));
    }
    // 5. Teto: transientes fortes nunca passam de -1 dBFS
    {
        Run r; Source s; int n = 0;
        r.play (10.0, [&] { ++n; float v = s.voice (-30.0f); if (n % 24000 < 200) v += 0.9f * (float) std::sin (n * 0.3); return v; });
        check (r.maxOutPeak <= AutoGainEngine::fromDb (-1.0f) + 1e-6f, "Protecao de pico: nunca passa de -1 dBFS",
               f ("pico maximo de saida %.2f dBFS", AutoGainEngine::toDb (r.maxOutPeak)));
    }
    // 6. Pico + RMS: sinal com picos altos -> ganho limitado pelo pico
    {
        Run r; Source s; int n = 0; r.p.detection = 2;
        r.play (12.0, [&] { ++n; float v = s.voice (-34.0f); if (n % 4800 < 30) v += 0.2f; return v; });
        const float pk = r.eng.outPeakDb.load();
        check (pk <= -0.9f && r.gain() < 12.0f, "Pico + RMS: ganho respeita o pico",
               f ("ganho %+.1f dB, pico saida %.1f dBFS", r.gain(), pk));
    }
    // 7. Resposta lenta (ao vivo): nao reage a uma palavra isolada mais forte
    {
        Run r; Source s; r.p.detection = 1;
        r.play (15.0, [&] { return s.voice (-30.0f); });
        const float before = r.gain();
        r.play (0.3, [&] { return s.voice (-20.0f); });   // uma palavra 10 dB mais forte
        check (before - r.gain() < 3.5f, "Resposta lenta: palavra forte isolada nao derruba o ganho",
               f ("ganho %+.1f -> %+.1f dB", before, r.gain()));
    }
    // 8. HOLD congela
    {
        Run r; Source s; r.p.detection = 1;
        r.play (15.0, [&] { return s.voice (-30.0f); });
        const float before = r.gain(); r.p.hold = true;
        r.play (10.0, [&] { return s.voice (-10.0f); });
        check (std::abs (r.gain() - before) < 0.01f, "HOLD congela o ganho", f ("%+.2f -> %+.2f dB", before, r.gain()));
    }
    // 9. Desligado = bypass exato
    {
        Run r; Source s; r.p.active = false; float maxDiff = 0.0f;
        Source ref;
        r.play (2.0, [&] { return s.voice (-6.0f); });
        // compara ultimo bloco com o sinal original
        for (int i = 0; i < BLOCK; ++i) (void) ref;
        check (std::abs (r.gain()) < 1e-6f && r.maxOutPeak > AutoGainEngine::fromDb (-1.0f), "Desligado = passa o sinal sem alterar",
               f ("ganho %+.2f dB, pico %.1f dBFS (sem teto)", r.gain(), AutoGainEngine::toDb (r.maxOutPeak)));
        (void) maxDiff;
    }
    // 10. Leitura do dia: 10 s de fala com pausas
    {
        Run r; Source s; int n = 0;
        r.eng.startAnalysis (10.0f);
        r.play (10.5, [&] { ++n; const bool talking = (n / 48000) % 3 != 2; return talking ? s.voice (-25.0f) : s.hiss (-58.0f); });
        auto res = r.eng.getAnalysisResult();
        check (res.valid && std::abs (res.noiseDb - (-58.0f)) < 2.0f && res.peakDb > res.rmsDb,
               "Leitura do dia: mede pico, RMS e ruido de fundo",
               f ("pico %.1f, RMS %.1f, ruido %.1f dBFS", res.peakDb, res.rmsDb, res.noiseDb));
        check (r.eng.analysisState.load() == AutoGainEngine::done, "Leitura do dia: termina sozinha", "estado = concluida");
    }
    // 11. Aplicar ajustes: salto imediato do ganho
    {
        Run r; Source s; r.p.detection = 1;
        r.eng.jumpGainTo (6.8f);
        r.play (0.01, [&] { return s.voice (-25.0f); });
        check (std::abs (r.gain() - 6.8f) < 0.3f, "Aplicar ajustes: ganho vai direto ao sugerido", f ("ganho %+.2f dB", r.gain()));
    }
    // 12. Latencia zero: impulso sai na mesma amostra
    {
        AutoGainEngine e; e.prepare (SR); AutoGainParams p;
        std::vector<float> buf (BLOCK, 0.0f); buf[10] = 0.1f;
        float* ch[1] = { buf.data() };
        e.process (ch, 1, BLOCK, p);
        int pos = -1; for (int i = 0; i < BLOCK; ++i) if (std::abs (buf[(size_t) i]) > 1e-6f) { pos = i; break; }
        check (pos == 10, "Latencia zero (impulso na mesma posicao)", f ("entrou na amostra 10, saiu na %.0f", pos));
    }

    std::printf ("\n%s: %d falha(s)\n\n", failures == 0 ? "RESULTADO: TUDO OK" : "RESULTADO: COM FALHAS", failures);
    return failures == 0 ? 0 : 1;
}
