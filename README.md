# Auto Gain — by Augusto Souza

Plugin VST3 de ganho automático para uso ao vivo no REAPER (Input FX). Latência zero.

## Baixar

- **Página de downloads:** [Releases → última versão](../../releases/latest)
- **Plugin (Windows 64 bits):** [AutoGain-Windows-x64.zip](../../releases/latest/download/AutoGain-Windows-x64.zip)
- **Instruções de instalação:** [INSTALAR.txt](INSTALAR.txt)

## Como é gerado

A cada envio para `main`, o GitHub compila o plugin num Windows na nuvem e roda dois testes antes de publicar:

1. **Motor de áudio**: 13 verificações (alvo, gate, teto de pico, HOLD, bypass, leitura do dia, latência zero).
2. **pluginval** nível 5: a ferramenta padrão do mercado para validar plugins VST3.

O andamento fica na aba [Actions](../../actions).

## Estrutura

- `Source/AutoGainEngine.h`: processamento de áudio
- `Source/PluginProcessor.*`: parâmetros do VST3
- `Source/PluginEditor.*`: interface Black Neon (Minimalista / Avançado / Análise)
- `Tests/EngineTest.cpp`: teste do motor
- `Fonts/`: fonte Inter (OFL)
