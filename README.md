# Firmware de Bring-Up — Multi-Signal-Analyzer

Este repositório contém **um firmware de bring-up** para o projeto **Multi-Signal-Analyzer**.  
O objetivo principal deste firmware é **validar o hardware**, **testar periféricos**, **confirmar o mapeamento de pinos** e **habilitar a comunicação básica** antes do desenvolvimento das funcionalidades finais do analisador lógico.

O **Multi-Signal-Analyzer** é um analisador lógico e de sinais mistos de alto desempenho (>100 MS/s), baseado no **RP2350**, projetado com firmware bare-metal, aceleração via **PIO/DMA** e suporte nativo a protocolos industriais, permitindo integração síncrona com **Sigrok/PulseView**.

> ⚠️ **Nota:** Este firmware **não representa a versão final do produto**. Ele é destinado exclusivamente à fase de **bring-up**, depuração inicial e validação elétrica e funcional do hardware.

---

## Tecnologias e Interfaces em Teste

### 🔹 Sinais Digitais
- **GPIOs:** GPIO9 até GPIO20  
- Uso previsto: captura digital, validação de níveis lógicos

### 🔹 Sinais Analógicos
- **GPIOs:** GPIO45 até GPIO47  
- Uso previsto: testes de ADC, ruído, offset e faixa dinâmica

### 🔹 Comunicação Serial RS-232
- **GPIOs:** GPIO24 (TX) e GPIO25 (RX)  
- Uso previsto: debug, logs e comunicação básica

### 🔹 Comunicação RS-485
- **GPIO:** GPIO31  
- Uso previsto: testes de barramento diferencial e controle de direção

### 🔹 Comunicação CAN
- **GPIO:** GPIO35  
- Uso previsto: validação do controlador CAN, transceptor externo e temporização do barramento

---

## Objetivos do Firmware de Bring-Up

- Verificar inicialização do RP2350
- Testar GPIOs digitais e analógicos
- Validar comunicação serial e protocolos industriais
- Confirmar funcionamento de PIO e DMA
- Identificar falhas elétricas ou de layout
- Servir como base para o firmware definitivo

---

Este repositório é parte fundamental do ciclo de desenvolvimento do **Multi-Signal-Analyzer**, garantindo que o hardware esteja plenamente funcional antes da implementação das camadas avançadas de captura e análise de sinais.
