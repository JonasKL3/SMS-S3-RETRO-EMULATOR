# Testes e limitações

## Build registrado

Arduino-ESP32 3.3.12, TFT_eSPI2.5.43, ESP32-S3 N16R8, 240MHz, OPI PSRAM, Huge APP, LoopCore=1, EventsCore=1: PASS.

- Programa: 815891bytes.
- Variáveis globais: 49124bytes; alocações dinâmicas não estão integralmente neste resumo.
- ELF SHA256: `d8c62d10036f9ff0f40fb76512ce1fbfed17240dfcbffe1ed39c9f0db14d70e8`.
- Strings dos relatórios periódicos ausentes no ELF. Núcleo e BLE preservados em relação à base anterior.

Os fontes do firmware nesta publicação são idênticos aos do pacote validado. A apresentação pública reorganiza documentação e avisos, sem recompilar ou alterar a lógica do jogo.

## Regressões no computador

Python3 e Zig0.14.1:

```text
python tests/run_host_tests.py --zig CAMINHO_DO_ZIG
```

Core real e testes C/C++ com instrumentação de comportamento indefinido. Há regressões de estado, mappers/CPU, vídeo assíncrono, pixels, fila, cache de capas e metadados. Os periféricos são simulados; testes isolados do wrapper de saves usam uma API de core controlada para injetar falhas.

No SMS:64cores, PSGativo, NMI, estado real em RAM e arquivo, flags Z80 exaustivos,64cenas de colisão/overflow/scroll/zoom e200quadros concorrentes de vídeo paletizado.

## Limitações atuais

- O autor confirmou o teste da versão sem telemetria na placa e sua publicação como release estável. Essa confirmação não representa uma medição numérica de FPS nem garante compatibilidade com todas as ROMs.
- Há erros intermitentes de armazenamento5/9 nos logs de uso. Build e testes não provam que cartão, conexão e gravação com BLE estejam resolvidos.
- Autosave periódicoOFF; reativá-lo pode causar pausa síncrona de gravação. Saves manuais continuam dependentes do SD.
- O barramento40MHz limita imagens completas. 320×240RGB565 exige no mínimo30,72ms só no fio. Velocidade emulada e atualização do LCD são grandezas diferentes.
- FM/YM2413 e sistemas além de Master System/SMS2 não são funcionalidades anunciadas.
- A redução de telemetria retira trabalho adicional, sem atribuir ganho numérico de FPS que não foi medido.

Use os logs e hashes em `audit/` para identificar exatamente a entrega testada.
