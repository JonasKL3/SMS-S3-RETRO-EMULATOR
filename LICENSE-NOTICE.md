# Créditos, origem e termos por componente

- **JonasKL3:** estrutura/interface dos projetos GB-S3 e NES-S3. Os módulos comuns foram adaptados a partir desses projetos e da revisão GBC-S3 desta conversa. As referências originais estão em `references/` e seus avisos não são substituídos por este documento.
- **SMSPlus:** Charles MacDonald, Eke-Eke e colaboradores; adaptação ESP32 por ducalex/Retro-Go. Revisão fixada `4ced120669750ca7228fd0414211430c1d923166`. Avisos GPL2-ou-posterior permanecem nos arquivos; cópia da GPL em `licenses/SMSPLUS-COPYING.txt` e créditos upstream ao lado dos fontes.
- **Z80:** Juergen Buchmueller e colaboradores. O cabeçalho de `src/smsplus/cpu/z80.c` contém termos legados explícitos de uso não comercial e exigência de atribuição/aviso de modificações. Esse aviso integral foi preservado. **O pacote não deve ser descrito como composto exclusivamente por GPL nem como livre de restrições comerciais.** Veja `licenses/Z80-NOTICE.txt` e o cabeçalho original.
- **PSG SN76489:** implementação de Maxim e adaptações de Charles MacDonald. Avisos originais preservados no módulo.
- **DabbleESP32:** STEMpedia, componentes incorporados com os avisos originais e correções locais de fila BLE/decodificação da base GBC-S3. A biblioteca incorporada não substitui os termos de cada arquivo.
- **TFT_eSPI, Arduino-ESP32 e dependências de sistema:** bibliotecas externas da instalação do usuário, com seus próprios termos. Não foram redistribuídas nem alteradas globalmente.

Os arquivos modificados do core são identificados no inventário de fontes/patches. O código-fonte da integração é entregue para inspeção e recompilação, com os avisos de terceiros mantidos. Não contém imagens de jogos, ROMs comerciais ou BIOS Coleco/SMS.

Fontes de referência: [GB-S3](https://github.com/JonasKL3/GB-S3-RETRO-EMULATOR), [NES-S3](https://github.com/JonasKL3/NES-S3-EMULATOR-FOR-ESP32), [SMSPlus/Retro-Go](https://github.com/ducalex/retro-go/tree/4ced120669750ca7228fd0414211430c1d923166/retro-core/components/smsplus).
