# Auto Gain + Limiter — by Augusto Souza

Um plugin VST3 só com a cadeia **Entrada → Auto Gain → Limiter → Saída**, interface Black Neon com as abas Minimalista, Avançado e Análise.

## Baixar

- **Página de download:** [Release agl-v1.0.0](../../../releases/tag/agl-v1.0.0)
- **Plugin (Windows 64 bits):** [AutoGainLimiter-Windows-x64.zip](../../../releases/download/agl-v1.0.0/AutoGainLimiter-Windows-x64.zip)
- **Instalação:** [INSTALAR.txt](INSTALAR.txt)

## Latência (medida)

| Lookahead | 48 kHz |
|---|---|
| 0 ms (ao vivo) | 0 ms |
| 1 ms (padrão) | 1,00 ms (48 amostras) |
| 3 ms | 3,00 ms (144 amostras) |
| 5 ms | 5,00 ms (240 amostras) |
| + True Peak | +6 amostras |

O Auto Gain não acrescenta latência. A latência não soma entre tracks.

## Como é gerado

A cada mudança nesta pasta, o GitHub compila no Windows e roda, antes de publicar:

1. **Teste dos motores** (`Tests/LimiterTest.cpp`): teto, latência por impulso, release manual/auto, True Peak, bypass, distorção, loudness BS.1770, ruído de fundo, cadeia completa e CPU com 20 tracks.
2. **Teste do Auto Gain** (`../Tests/EngineTest.cpp`).
3. **pluginval** nível 5.

## Estrutura

- `Source/LimiterEngine.h`: limiter brickwall (lookahead, auto release, True Peak 4x)
- `Source/Analyzer.h`: leitura do dia (pico, true peak, RMS, LUFS, dinâmica, ruído)
- `../Source/AutoGainEngine.h`: o mesmo motor do Auto Gain v1.0
- `Source/PluginProcessor.*`, `Source/PluginEditor.*`: plugin e interface
