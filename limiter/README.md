# Auto Gain + Limiter — by Augusto Souza

Um plugin VST3 so com a cadeia **Entrada → Auto Gain → Limiter → Saida**, interface Black Neon com as abas Minimalista, Avancado e Analise.

## Baixar

- **Pagina de download:** [Release agl-v1.1.0](../../../releases/tag/agl-v1.1.0)
- **Plugin (Windows 64 bits):** [AutoGainLimiter-Windows-x64.zip](../../../releases/download/agl-v1.1.0/AutoGainLimiter-Windows-x64.zip)
- **Instalacao:** [INSTALAR.txt](INSTALAR.txt)

## Latencia (medida)

| Lookahead | 48 kHz |
|---|---|
| 0 ms (ao vivo) | 0 ms |
| 1 ms (padrao) | 1,00 ms (48 amostras) |
| 3 ms | 3,00 ms (144 amostras) |
| 5 ms | 5,00 ms (240 amostras) |
| + True Peak | +6 amostras |

O Auto Gain nao acrescenta latencia. A latencia nao soma entre tracks.

## Como e gerado

A cada mudanca nesta pasta, o GitHub compila no Windows e roda, antes de publicar:

1. **Teste dos motores** (`Tests/LimiterTest.cpp`): teto, latencia por impulso, release manual/auto, True Peak, bypass, distorcao, loudness BS.1770, ruido de fundo, cadeia completa e CPU com 20 tracks.
2. **Teste do Auto Gain** (`../Tests/EngineTest.cpp`).
3. **pluginval** nivel 5.

## Estrutura

- `Source/LimiterEngine.h`: limiter brickwall (lookahead, auto release, True Peak 4x)
- `Source/Analyzer.h`: leitura do dia (pico, true peak, RMS, LUFS, dinamica, ruido)
- `../Source/AutoGainEngine.h`: o mesmo motor do Auto Gain v1.0
- `Source/PluginProcessor.*`, `Source/PluginEditor.*`: plugin e interface
