# Sistema Inteligente de Gestão Energética Residencial com IoT

Projeto da disciplina **Objetos Inteligentes Conectados** — Mackenzie 2026.02
Prof. Wallace Rodrigues de Santana

**Grupo:**
- Jean Alex da Silva — 10426728
- Deborah Jamilly de Abreu Souza — 10420342
- Gabriela Nellessen de Sousa — 10441930

## Descrição
Sistema IoT que monitora o consumo elétrico residencial em tempo real, segmentado por circuito do quadro de luz, e dispara ações automatizadas para reduzir desperdício. Aderente aos ODS 7 (Energia Limpa e Acessível) e ODS 12 (Consumo Responsável) da Agenda 2030.

## Arquitetura

```
ESP32 A (Tomadas)   ──MQTT──▶  HiveMQ  ◀──▶  Node-RED  ──▶  InfluxDB  ──▶  Grafana
  pot + relé                                    │
                                                └──▶  Telegram Bot

ESP32 B (Lâmpadas)  ──MQTT──▶  HiveMQ  ◀──MQTT──  Node-RED  (comandos)
  pot + relé
```

Os **dois dispositivos são idênticos em hardware e firmware**, diferenciados apenas pela constante `CIRCUITO` no código (tomadas ou lampadas). Cada um mede a corrente do seu circuito e aciona o relé que controla o disjuntor correspondente.

## Estrutura do repositório
```
.
├── wokwi-dispositivo-a-tomadas/   # ESP32 + pot + módulo relé — circuito de tomadas
│   ├── sketch.ino
│   ├── diagram.json
│   └── libraries.txt
├── wokwi-dispositivo-b-lampadas/  # ESP32 + pot + módulo relé — circuito de lâmpadas
│   ├── sketch.ino
│   ├── diagram.json
│   └── libraries.txt
├── node-red/
│   └── flows.json                 # Fluxo Node-RED completo
├── grafana/
│   └── dashboard-gestao-energetica.json   # 1 dashboard, 4 seções, 17 painéis
└── artigo/
    └── Trabalho IoT - V6.docx
```

## Como reproduzir

### 1. Dispositivos Wokwi
1. Acesse https://wokwi.com → New Project → ESP32
2. Substitua `sketch.ino` pelo conteúdo de `wokwi-dispositivo-a-tomadas/sketch.ino`
3. Substitua `diagram.json` pelo conteúdo de `wokwi-dispositivo-a-tomadas/diagram.json`
4. Adicione a biblioteca `PubSubClient` no Library Manager
5. Repita para `wokwi-dispositivo-b-lampadas/` em outro projeto
6. Inicie a simulação em ambos

### 2. Node-RED
1. Instalar Node-RED localmente, em VM ou usar FlowFuse Cloud
2. Instalar paletas adicionais:
   - `node-red-contrib-influxdb`
   - `node-red-contrib-telegrambot`
3. Menu → Import → cole o conteúdo de `node-red/flows.json` (com Replace)
4. Configurar credenciais nos nós:
   - **Telegram bot**: token obtido em @BotFather + chat_id obtido em @userinfobot
   - **InfluxDB**: URL, org ID, bucket `energia-residencial` e token

### 3. InfluxDB Cloud
1. Conta gratuita em https://cloud2.influxdata.com
2. Criar bucket `energia-residencial`
3. Gerar token de API e colar no Node-RED

### 4. Grafana Cloud
1. Conta gratuita em https://grafana.com
2. Adicionar InfluxDB como Data Source (Query Language: **Flux**)
3. Importar `grafana/dashboard-gestao-energetica.json` (**Dashboards → New → Import → Upload JSON**) e mapear o data source. O dashboard tem 4 Rows e 17 painéis:
   - **Seção 1 — Circuito Tomadas**: consumo instantâneo, kWh, custo, status, estado do relé
   - **Seção 2 — Circuito Lâmpadas**: mesmos indicadores, espelhados
   - **Seção 3 — Histórico Comparativo**: série temporal 24h, kWh/dia, maior consumo
   - **Seção 4 — Automação e Alertas**: log de acionamentos, motivos, picos e stand-by do dia

## Vídeo de demonstração
https://youtu.be/yww3u4SdNF8

## Tópicos MQTT

| Tópico | Direção | Payload |
|---|---|---|
| `iot/residencial/tomadas/consumo` | Dispositivo A → broker | Watts (float) |
| `iot/residencial/tomadas/estado` | Dispositivo A → broker | `ON` / `OFF` (retained) |
| `iot/residencial/tomadas/comando` | broker → Dispositivo A | `ON` / `OFF` |
| `iot/residencial/lampadas/consumo` | Dispositivo B → broker | Watts (float) |
| `iot/residencial/lampadas/estado` | Dispositivo B → broker | `ON` / `OFF` (retained) |
| `iot/residencial/lampadas/comando` | broker → Dispositivo B | `ON` / `OFF` |

## Cálculo da potência (firmware)
```
leituraADC ∈ [0, 4095]                         (ADC do ESP32)
corrente_A   = (leituraADC / 4095) × 60        (SCT-013 60A)
potencia_W   = corrente_A × 127                (tensão nominal SP)
```
A energia em kWh é integrada no Node-RED em janelas de uma hora, e o custo (R$) usa a tarifa de R$ 0,95/kWh.

## Regras de negócio (Node-RED)
- **Pico**: potência > 2000 W por mais de 5 min → desliga o disjuntor daquele circuito e alerta no Telegram
- **Stand-by**: 5–15 W por mais de 30 min → alerta de desperdício no Telegram
- **Horário programado**: 01:00 desliga lâmpadas, 06:00 liga
- **Auditoria**: todo acionamento é gravado no InfluxDB (measurement `acionamentos`)
- **Status do consumo**: classificado em `normal` / `alto` / `critico`

## Dados gravados no InfluxDB (bucket `energia-residencial`)
- `energia` — tag `circuito ∈ {tomadas, lampadas}` — fields: `watts`, `corrente_a`, `tensao_v`, `kwh`, `custo_brl`, `status`
- `acionamentos` — tag `circuito` — fields: `acao`, `motivo`

## Dashboard
<img width="975" height="1229" alt="WhatsApp Image 2026-06-17 at 23 17 54" src="https://github.com/user-attachments/assets/7a7878e8-cb98-4aa5-b43a-68e739034b75" />


## Atenção — anonimização
Antes de tornar o repositório público, garanta que:
- O **token do bot Telegram** está apenas configurado na UI do Node-RED, não hardcoded em arquivo
- O **token do InfluxDB** está apenas configurado na UI do Node-RED
