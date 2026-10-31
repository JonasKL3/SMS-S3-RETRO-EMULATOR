# Instalação — SMS-S3 0.1.2

Use os passos do [README](../README.md#instalação-rápida). Perfil do build: Arduino-ESP32 3.3.12, TFT_eSPI 2.5.43, ESP32S3 Dev Module, 240 MHz, flash 16 MB, OPI PSRAM, Huge APP, LoopCore=1, EventsCore=1, USBMode=hwcdc. Selecione Upload, não depuração OpenOCD.

O núcleo e Dabble estão dentro de `firmware/SMS_S3/src/`. Não instale outra cópia externa do núcleo para este projeto. TFT_eSPI/SD globais não foram modificadas por esta publicação.

Abra o `.ino` depois de extrair o ZIP inteiro. Preserve a árvore `src`, os módulos `.cpp/.h` e a pinagem indicada em [hardware](HARDWARE.md). Use o mesmo User_Setup.h já aprovado na placa; `config/User_Setup.h` é uma referência.

O firmware cria diretórios ausentes em `/JOGOS/SMS`. Copie ROMs `.sms` para `jogos/`. Não há leitura de jogos diretamente de ZIP nem dependência de Wi-Fi.

## Primeiro teste

Confirme `[BUILD] SMS-S3 DEV 0.1.2 SEM-TELEMETRIA` no serial. Navegue, abra uma ROM conhecida, teste controles, áudio, AMPLIADO/NATIVA, pausa, save/load manual e retorno ao catálogo. Durante a jogatina não devem aparecer resumos periódicos `[PERF]`, `[STACK]` ou `[VIDEO][WORK]`.

A ausência de telemetria não remove a temporização da emulação nem a proteção contra travamento. Avisos de erro e mensagens de operações continuam disponíveis. Não há uma medição física de ganho de FPS desta versão.
