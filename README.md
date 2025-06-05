# CASA INTELIGENTE COM MQTT

Este projeto propõe o desenvolvimento de um sistema de automação residencial com controle remoto de dispositivos como luzes, alarmes e portas, utilizando o protocolo MQTT. A solução adota uma arquitetura leve, eficiente e orientada a eventos, promovendo comunicação rápida e confiável entre os dispositivos.

## Componentes Utilizados


1. **Botão Pushbutton**
2. **Display OLED 1306**
3. **Buzzer**
4. **Matriz de LED 5x5 WS2812** 
5. **Led RGB**
6. **Modulo WIFI (CYW4)**
7. **Jumpers**
8. **Servo Motor**
9. **Protoboard**
10. **Fonte de alimentação externa 5v**

## Funcionalidade

O sistema permite o controle remoto, via aplicativo IoT MQTT Panel, das seguintes funcionalidades:

**Ligar/Desligar a lâmpada da sala**

**Ligar/Desligar a lâmpada do quarto**

**Abrir/Fechar a porta**

**Ativar/Desativar o alarme**

**Ativar/Desativar o modo viagem**

**Visualizar gráfico de temperatura ambiente em tempo real**

Ao iniciar o sistema pela primeira vez, é necessário configurar a rede Wi-Fi (SSID e senha). É recomendado ter o Termux instalado e configurado no dispositivo Android, com o Mosquitto como broker MQTT local.

Após a conexão Wi-Fi, o endereço IP será exibido via UART, permitindo o acesso à interface web por meio desse IP.

### Como Usar

#### Usando a BitDogLab

- Clone este repositório: git clone https://github.com/rober1o/casa_inteligente_MQTT.git;
- Usando a extensão Raspberry Pi Pico importar o projeto;
- Ajuste a rede wifi e senha 
- Compilar o projeto;
- Plugar a BitDogLab usando um cabo apropriado

## Demonstração
<!-- TODO: adicionar link do vídeo -->
Vídeo demonstrando as funcionalidades da solução implementada: [Demonstração](https://youtu.be/4Z0IVPpOc64)
