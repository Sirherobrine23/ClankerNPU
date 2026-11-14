# EN7523 Wireless Lab 7.13.24

Base Clanker `en7523_v2`: `50e92a1cc20629cb2e3fe8a5925e6d3fc9a4f65a`.
OpenWrt conferido: `airha_npu`, `0179add735d627d1be6fc3ebbe5072617e86851a`.

## Correção de layout do debug

O log da v19 informa:

```text
[WLAB] reject cmd=8 arg=84d00000 reason=7 rx=0/38060 pause=0 ack=0/0
```

O pool foi recusado porque o flag RX de 5 GHz contém `0x38060`, antes do
primeiro SET_DESC. Esse valor coincide com a soma das alocações SRAM realizadas
no boot. A pausa não participou desta recusa.

No ELF v19 compilado com o toolchain do projeto:

| Objeto | Endereço |
| --- | --- |
| `rxd_5g_init_done` | `0x1e902b04` |
| `NDBG_SET(NC_SRAM_USED, ...)` | `0x1e906b04` |
| Diferença | `0x4000` |
| Valor do contador após o boot | `0x38060` |

O valor e o deslocamento são forte evidência de espelhamento/alias da janela
local. O teste de host usa duas janelas separadas por `0x4000` mapeadas ao mesmo
backing e o NDBG_SET real do firmware: reproduz RX5=`0x38060` com o endereço
antigo. É um modelo da hipótese, não uma medição da capacidade física do chip.

A v20 move NDBG para `0x1e85f000..0x1e85ffff`, na última página de SRAM de
pacotes já excluída do alocador do Lab. O diagnóstico específico do Lab continua
em `0x1e85e000`. O alocador permanece limitado a `[0x1e800000,0x1e85e000)`.
Assim, a região de debug deixa de usar a janela local suspeita de alias.

O linker do Lab impõe um limite conservador de 16 KiB aos globais locais e
verifica o endereço e o fim da página NDBG. O firmware atual ocupa 11308 bytes (perfil desligado)
locais e termina em `0x1e902c2c`. Esse limite de build não é uma conclusão
sobre a capacidade física total da NPU. O layout do build padrão é preservado.

Não foi removida a validação que recusa trocar o pool com RX já inicializado.
O startup, a sequência CSR/IRQ e o transporte de mailbox continuam compartilhados
com a base. As alterações da v19 são mantidas: antes do primeiro START não há
pausa desnecessária; depois dele a pausa entre workers continua obrigatória.

## Build direto

```sh
make -j10 SOC=EN7523 WIFI=MT7915 FW_IMPL=wireless-lab LAB_CUSTOM_OPS=0 \
  CROSS=/caminho/para/riscv64-unknown-elf-
```

O par de blobs fica em `firmwares/en7523_mt7915_npu_{rv32,data}.bin`.
ELF, mapa e disassembly ficam em `build/EN7523_MT7915/wireless-lab/`.
Use inicialmente `LAB_CUSTOM_OPS=0`; o startup ainda contém suas instruções
explícitas de manutenção de cache. `LAB_CUSTOM_OPS=1` exige o GCC/binutils com
`xen7523ops`. O backend padrão é selecionado com `FW_IMPL=clanker`.

## Build pelo OpenWrt

Aplique o patch na raiz do Clanker usado pelo feed `npu`. Selecione somente
EN7523/MT7915 para este teste Lab.

```sh
make package/feeds/npu/ClankerNPU/clean V=s
make -j10 package/feeds/npu/ClankerNPU/compile \
  CLANKERNPU_FW_IMPL=wireless-lab CLANKERNPU_LAB_CUSTOM_OPS=0 V=s
make -j10 CLANKERNPU_FW_IMPL=wireless-lab CLANKERNPU_LAB_CUSTOM_OPS=0 V=s
```

Faça boot a frio com a imagem reconstruída e confira a versão `7.13.24`.
No início deve aparecer:

```text
[WLAB] local_end=1e902c2c ndbg=1e85f000 rx_ready=0/0
```

Depois capture:

```sh
cat /sys/kernel/debug/airoha_npu/status
cat /sys/kernel/debug/airoha_npu/mib
```

MIB16 deve passar a `0x84d00000` depois da configuração bem-sucedida do pool.
Se falhar, a linha `[WLAB] reject` registra comando, argumento, RX e ACK.
MIB19 desta versão é `0x574c3234`.

| MIB | Conteúdo |
| --- | --- |
| 0 | Motivo da última recusa SET, persistente até outra recusa |
| 1–4 | Quantidade, comando, argumento e base da última mensagem WLAN |
| 5–6 | RX por banda |
| 16 | Base do pool de pacotes |
| 18 | Bit 0: Lab pronto; bit 1: QDMA anexado; bit 2: HWNAT solicitado |
| 19–20 | Assinatura e endereço do diagnóstico do Lab |
| 23–25 | Submissões QDMA, devoluções PPE e frames ao host |
| 26 | Erro fatal, separado das recusas de configuração |
| 27–30 | Buffers livres, completions QDMA, punts PPE e estado |
| 31 | Sinal de boot da base, `0xcccccccc` |

Motivos de recusa: 1=payload curto; 2=comando fora da enumeração;
3=ACK anterior pendente; 4=ACK de pausa ausente; 5=pool zerado;
6=pool desalinhado; 7=RX já inicializado; 8=outro argumento inválido;
9=SET não implementado. Zero significa nenhuma recusa registrada.

## Validação e escopo

```sh
make SOC=EN7523 WIFI=MT7915 FW_IMPL=wireless-lab lab-test
```

Inclui o modelo de colisão NDBG/RX com os tipos e o macro reais da raiz, os
testes de START/pausa da v19 e os 12866 checks de filas/ownership/DMA simulados.
O modo escalar padrão é verificado por disassembly e o linker impõe os limites.
A correção precisa de confirmação no dispositivo; o modelo não prova sozinho
que o hardware espelha a região.

O backend experimental cobre RX → QDMA/PPE e retorno ao host com descritores
de 24 bytes. Exige QDMA0 ring 6 e IRQ queue 1 reservados pelo host, com 1024
entradas, e pool de 5600 buffers de 4096 bytes. TX wireless completo, STOP,
detach e reload durante uso não foram validados. Coerência/cache e o caminho
de tráfego completo ainda exigem teste em hardware.

## ABI mt76 corrigida na versão 21

O log da v20 confirmou pool aceito (`pkt_buf_addr=84d00000`) e RX ready=0/0.
O probe então parou em `reject cmd=30 arg=1 reason=9`, retornando -95.
O dispatcher Lab não possuía esse SET. O Clanker padrão já devolvia sucesso
por um stub, mas o Lab agora aplica uma política explícita ao caminho de dados.

Foi conferido `mt7915/npu.c` da branch `npu_en7523`, commit
`983ef6983102461f6daf6995a7819eed8c6f8496` do repositório
<https://sirherobrine23.com.br/airoha/mt76>. O Makefile OpenWrt usa
`USE_SOURCE_DIR=../mt76`, portanto o checkout local efetivamente usado pode
ter alterações adicionais. A sequência observada no log corresponde à fonte
conferida até o erro. Não foi necessário alterar o driver Linux.

| Comando | Efeito no Lab v21 |
| --- | --- |
| 30 / HWNAT_INIT, valor 0/1 | Controla a permissão para novos frames wireless seguirem à QDMA/PPE |
| 2 / NPU_INIT_DONE, valor 1 | START por banda, após pool e RX descritores prontos |
| 2 / NPU_INIT_DONE, valor 0 | Retira a banda da máscara dos workers |
| 13 / PCIE_STATE | Notificação de link, sem liberar os workers |

HWNAT começa desabilitado no Lab. Com valor 0, o bridge encaminha os novos
frames ao host; valor 1 permite o caminho QDMA/PPE, respeitando também as
políticas existentes de force_to_cpu, capacidade e rate limit. O submit QDMA
também recusa novos frames quando a permissão está desativada. As completions
já em voo não são apagadas por esse SET. A inicialização dos registradores PPE
continua pertencendo ao Linux; SET WLAN 30 não executa uma segunda inicialização.

O envio desse SET não prova que a QDMA/PPE já está operacional. Em MIB18,
bit 2 indica somente a permissão pedida pelo host. O bit 1 continua indicando
que o backend QDMA anexou com sucesso.

Mensagens esperadas, conforme o host avançar:

```text
[WLAB] hwnat_requested=1
[WLAB] runner band=0 enable=1 mask=1
[WLAB] runner band=1 enable=1 mask=3
```

O STOP agora bloqueia novos acessos dos workers à banda. Ele não constitui
um protocolo completo de drain/retirada de DMA, nem autoriza liberar buffers
em uso. A política de retenção de armazenamento do mt76 continua necessária.

Os testes da v21 exercitam o dispatcher real para habilitar/desabilitar HWNAT,
valores inválidos, START/STOP por banda no comando 2 e comando 13 sem START.
Outro teste usa a função real `pkt_enqueue_bridge` e verifica a alternância
host → PPE → host. O modelo QDMA verifica que, sem permissão, não há submit
nem doorbell. A captura posterior da v21 confirmou o boot e o tráfego;
os resultados e as correções seguintes estão descritos abaixo.

## Retorno PPE e fragmentação corrigidos na versão 22

Na captura v21, MIB18=7 e MIB26=0. A última amostra contém RX5G=58051,
submissões QDMA=44795, completions=44795, punts PPE=44795, free-only=0 e
frames ao host=58046. O transporte funciona, mas não há evidência de fluxo
wireless encaminhado pelo PPE sem retorno ao host nessa amostra.

O Lab armazenava hash/motivo em word 1 (`info`) do descritor host e escrevia
somente `band<<6` no word 2 (`vendor`). O mt76 encaminha word 2 para
`mt7915_npu_rx_status`, que exige bit 28 para chamar o caminho de aprendizado
do PPE. Esse erro impedia que o retorno do PPE informasse sua proveniência ao
consumidor. A v22 preserva os campos de word 1 para fragmentação e também
publica a metadata esperada em word 2 nos pacotes que realmente retornaram do PPE.

| Campo vendor, word 2 | Conteúdo no retorno PPE |
| --- | --- |
| Bit 28 | Proveniência PPE |
| Bits 20..16 | Motivo retornado pelo PPE |
| Bits 14..0 | Índice/hash retornado pelo PPE |

O hint antigo de banda não é ORado no hash. Pacotes RX crus não recebem o
bit PPE. A v22 não inventa PN_VALIDATED, REORDERED nem autorização de estação;
as verificações do mt76 para estação autorizada e frame 802.3 continuam ativas.
A interpretação dos valores efetivamente retornados pelo hardware deve ser
conferida pelos novos contadores e pela tabela de fluxos do Linux.

Outra diferença de ABI corrigida: mt7915_npu_rx espera fragmentos de 1 até N,
enquanto o Lab emitia 0 até N-1. A primeira posição agora é 1; pacotes inteiros
mantêm o campo em 0. Um teste executa a função consumidora copiada do mt76
conferido, com shims nativos de skb/NAPI: o formato antigo é recusado e um frame
de 3500 bytes no formato novo é remontado corretamente.

A cada incremento de pelo menos 2048 punts, no máximo uma vez por tick de
status, o firmware publica uma linha como:

```text
[WLAB] PPE punt=... free=... host_ppe=... reasons=0d:.../0e:.../0f:... invalid_hash=... last=...
```

O histograma completo de 32 motivos e a última metadata também ficam no final
do diagnóstico Lab em 0x1e85e000, preservando o prefixo anterior. Essa linha
ajuda a separar misses/unbind de hash inválido e de outras causas de punt.
O patch não altera mensagem QDMA, registradores PPE ou formato das entradas FoE.

O log também contém aviso `SRAM size mismatch: wrote 7, read 6` e
`stats entries -95` durante o setup PPE. Esses pontos do driver permanecem para
investigação caso o fluxo não entre em BND depois da correção de metadata.
Não há promessa de taxa de transferência nem prova de aceleração apenas por
MIB18=7. A confirmação exige tráfego e entradas BND correspondentes ao teste.


## Perfilamento v23

A v23 introduziu LAB_PROFILE=1 por padrão. Na v24, LAB_PROFILE=0 é o padrão;
as sondas continuam disponíveis explicitamente com valor 1. O hart 1
amostra uma chamada a cada 64 de qdone, ppe, rx, class, submit e alloc.
CLASS/SUBMIT/ALLOC estão aninhados em RX; os tempos são inclusivos.
Não se deve somar essas linhas como se fossem custos independentes.

A leitura mcycle usa barreira de compilador e a subtração unsigned trata o wrap
em medições curtas. Os contadores de ciclos são cumulativos de 32 bits;
o relatório usa diferenças a cada aproximadamente dois segundos. Intervalos
maiores que um wrap de mcycle (~12,9 s a 333 MHz) não têm tempo reconstruível.
O máximo de cada estágio é desde o boot, não apenas da janela.

O diagnóstico de perfil foi anexado ao fim da página Lab, mantendo o prefixo
anterior. O hart 1 é o único escritor das estatísticas. Publica um snapshot
com sequência a cada 4096 passes; o hart 0 tenta lê-lo até três vezes e pula
uma publicação ocupada. Não há espera ilimitada do hart de controle.
Não há impressão por pacote nem escrita UART do hart RX. O hart 0 imprime
linhas [PROF] a cada aproximadamente dois segundos de atividade RX.

QPEAK conta descritores submetidos sem completion, limitado a 1023; IRQPEAK e
FIFOPEAK registram picos das filas de completion e retorno PPE. IRQCAP/FIFOCAP
contam polls cujo backlog excedeu o orçamento 128. PEEK registra uma inspeção
limitada aos próximos 16 descritores RX, uma a cada 64 polls de cada banda;
16 significa pelo menos 16 descritores DDONE consecutivos, não ring cheio.
FULL é a quantidade dessas inspeções que atingiu o limite. POLL/READY e NOBUF
separam polls vazios, frames prontos e falhas de reposição de buffer.
Picos são desde o boot; demais campos de pressão impressos são deltas da janela.

LAB_PROFILE=0 desliga as sondas e os relatórios. No OpenWrt use
CLANKERNPU_LAB_PROFILE=0, preservando FW_IMPL=wireless-lab e CUSTOM_OPS=0.
O heartbeat cycles[hart] da v22 permanece inalterado para não misturar uma
possível otimização do tráfego de diagnóstico com o experimento de medição.
O perfilamento tem custo: compare os dois modos no mesmo cenário antes de
interpretar uma diferença de throughput como um problema no datapath.


## Lotes RX na v24

O novo lab_rx_schedule.c chama o consumidor de um frame até consumir oito
frames no total, alternando bandas produtivas. Entre lotes volta ao laço
existente que atende QDMA/PPE. Um consumidor retorna 0 quando nenhum descritor
foi consumido (ring vazio ou frame ainda incompleto) e 1 quando consumiu,
inclusive se o pacote precisou ser descartado pelos caminhos existentes.

Ao encontrar uma banda vazia/incompleta, pula até sete passagens seguintes;
a oitava consulta novamente. Isso é um limite em passagens, não em microssegundos.
A manutenção de BA foi movida para o escalonador e continua sendo atendida
mesmo quando os polls de descritores dessa banda estão em backoff. O limite
original de idade >9 ticks e o comportamento no modo sem BA são preservados.

O escalonador verifica START, RX inicializado, pausa e erro fatal. Uma pausa
ou erro observado entre frames encerra o lote. As duas bandas carregadas
recebem quatro frames cada por lote completo; uma banda vazia não reserva
créditos que impeçam a outra de consumir os oito frames. Não houve mudança
de hart, ownership de buffers, formato host/QDMA ou instruções de cache.

No modo de perfil opcional, a linha stage=rx passa a medir uma chamada do
escalonador (até oito frames), enquanto class/submit/alloc seguem por frame.
Use work para normalizar ciclos por frame, não a média por chamada RX da v23.
O leitor profile-summary.py também passa a tolerar linhas UART incompletas,
marcando missing/incomplete em vez de interromper com KeyError.
