## SMS-S3 0.1.2 — sem telemetria contínua

Publicação inicial do emulador independente de Sega Master System para ESP32-S3 N16R8, seguindo a estrutura dos projetos GB-S3 e NES-S3.

### Incluído

- Núcleo SMSPlus, Dabble e módulos do sketch incorporados.
- Catálogo com busca/favoritos/recentes, capas NS3C com cache, áudio MAX98357A e pausa com estados próprios.
- Vídeo assíncrono e remoção dos relatórios contínuos de FPS/tempos/pilha/áudio.
- Fontes, ferramentas, estrutura do SD, testes e avisos de licença.

### Instalar

Extraia o ZIP completo e abra `firmware/SMS_S3/SMS_S3.ino`. Use seu User_Setup.h atual. ROMs `.sms` em `/JOGOS/SMS/jogos/`, capas em `/JOGOS/SMS/cover/`. Perfil e controles no README.

### Validação e limites

Compilação ESP32-S3 e testes de host aprovados, com registros em `audit/`. O autor confirmou o teste desta versão na placa e sua publicação como release estável. Autosave e SRAM convencional iniciam OFF. Falhas intermitentes de SD não foram declaradas resolvidas.

O Z80 incorporado preserva termos legados de uso não comercial. O conjunto não deve ser apresentado como exclusivamente GPL ou sem restrições comerciais. Veja os avisos integrais em `licenses/` e no código.

Não contém ROMs comerciais ou BIOS. O ZIP é um pacote de fontes, não um binário pronto para gravar. Confira `SHA256SUMS.txt` dos assets para integridade.
