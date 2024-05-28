#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

RTC_DS1307 rtc;
LiquidCrystal_I2C lcd(0x27, 20, 4);
Servo meuServo;

const int pinosBotoes[] = {2, 3, 4, 5, 6};
const int numBotoes = 5;

const int pinoBuzzer = 8;
const int pinoLed = 10;
bool alertaAtivo = false;
unsigned long inicioAlerta = 0;

int hora = 0;
int dezenaMinuto = 0;
int unidadeMinuto = 0;

bool estadosBotoes[numBotoes] = {false, false, false, false, false};  
bool ultimosEstadosBotoes[numBotoes] = {true, true, true, true, true};
unsigned long ultimaLeituraDebounce[numBotoes] = {0, 0, 0, 0, 0};  
const unsigned long atrasoDebounce = 50;

bool horarioConfirmado = false;
int horaConfirmada = 0;
int dezenaMinutoConfirmada = 0;
int unidadeMinutoConfirmada = 0;

const int maxHorarios = 3;
int horariosRegistrados[maxHorarios][3];
bool horariosDisparados[maxHorarios] = {false, false, false};
int numHorariosRegistrados = 0;

bool displayConfigurado = false;
unsigned long tempoConfirmacao = 0;
unsigned long tempoPressionado = 0;

void setup() {
  Serial.begin(9600);

  if (!rtc.begin()) {
    Serial.println("RTC não encontrado!");
    while (1);
  }
  
  if (!rtc.isrunning()) {
    Serial.println("RTC não está funcionando, configurando a data e hora!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  meuServo.attach(9);        
  delay(1000);

  lcd.init();
  lcd.backlight();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Remedio 3000");
  lcd.setCursor(0, 1);
  lcd.print("iniciando...");
  delay(1000);

  for (int i = 0; i < numBotoes; i++) {
    pinMode(pinosBotoes[i], INPUT_PULLUP);
  }

  pinMode(pinoBuzzer, OUTPUT);
  pinMode(pinoLed, OUTPUT);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Hora atual:");
  lcd.setCursor(0, 2);
  lcd.print("Configurar horario:");
}

void loop() {
  atualizarHoraAtual();
  verificarBotoes();
  verificarAlarmes();
  if (alertaAtivo && (millis() - inicioAlerta >= 10000)) {
    desligarAlerta();
  }
}

void atualizarHoraAtual() {
  DateTime agora = rtc.now();
  char bufferHora[20];
  sprintf(bufferHora, "%02d/%02d/%04d %02d:%02d:%02d", agora.day(), agora.month(), agora.year(), agora.hour(), agora.minute(), agora.second());
  lcd.setCursor(0, 1);
  lcd.print(bufferHora);
}

void verificarBotoes() {
  for (int i = 0; i < numBotoes; i++) {
    bool leitura = digitalRead(pinosBotoes[i]) == LOW;

    if (leitura != ultimosEstadosBotoes[i]) {
      ultimaLeituraDebounce[i] = millis();
    }

    if ((millis() - ultimaLeituraDebounce[i]) > atrasoDebounce) {
      if (leitura != estadosBotoes[i]) {
        estadosBotoes[i] = leitura;
        
        if (estadosBotoes[i]) {
          tratarPressionamentoBotao(i);
        }
      }
    }

    ultimosEstadosBotoes[i] = leitura;
  }
}

void tratarPressionamentoBotao(int indiceBotao) {
  Serial.print("Botão ");
  Serial.print(indiceBotao);
  Serial.println(" pressionado.");

  switch (indiceBotao) {
    case 0:
      incrementarHora();
      break;
    case 1:
      incrementarDezenaMinuto();
      break;
    case 2:
      incrementarUnidadeMinuto();
      break;
    case 3:
      confirmarHorario();
      break;
    case 4:
      apagarHorarios();
      break;
  }

  atualizarHorarioConfigurado();
}

void incrementarHora() {
  hora++;
  if (hora > 23) hora = 0;
  Serial.print("Botão 1 pressionado. Hora: ");
  Serial.println(hora);
}

void incrementarDezenaMinuto() {
  dezenaMinuto++;
  if (dezenaMinuto > 5) dezenaMinuto = 0;
  Serial.print("Botão 2 pressionado. Dezena de Minuto: ");
  Serial.println(dezenaMinuto);
}

void incrementarUnidadeMinuto() {
  unidadeMinuto++;
  if (unidadeMinuto > 9) unidadeMinuto = 0;
  Serial.print("Botão 3 pressionado. Unidade de Minuto: ");
  Serial.println(unidadeMinuto);
}

void confirmarHorario() {
  if (numHorariosRegistrados < maxHorarios) {
    horarioConfirmado = true;
    horaConfirmada = hora;
    dezenaMinutoConfirmada = dezenaMinuto;
    unidadeMinutoConfirmada = unidadeMinuto;

    horariosRegistrados[numHorariosRegistrados][0] = horaConfirmada;
    horariosRegistrados[numHorariosRegistrados][1] = dezenaMinutoConfirmada;
    horariosRegistrados[numHorariosRegistrados][2] = unidadeMinutoConfirmada;
    horariosDisparados[numHorariosRegistrados] = false;
    numHorariosRegistrados++;
    
    Serial.print("Botão 4 pressionado. Horário Confirmado: ");
    Serial.print(horaConfirmada);
    Serial.print(":");
    Serial.print(dezenaMinutoConfirmada);
    Serial.println(unidadeMinutoConfirmada);
    displayConfigurado = true;
    tempoConfirmacao = millis();
    tempoPressionado = millis();

    exibirMensagemConfirmacao();
  } else {
    exibirMensagemErro();
  }
}

void apagarHorarios() {
  numHorariosRegistrados = 0;
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("Horarios apagados!");
  delay(3000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Hora atual:");
  lcd.setCursor(0, 2);
  lcd.print("Configurar horario:");
  Serial.println("Botão 5 pressionado. Todos os horários programados foram apagados.");
}

void atualizarHorarioConfigurado() {
  lcd.setCursor(0, 3);
  lcd.print("Horario: ");
  char bufferHora[6];
  sprintf(bufferHora, "%02d:%02d", hora, dezenaMinuto * 10 + unidadeMinuto);
  lcd.print(bufferHora);
}

void exibirMensagemConfirmacao() {
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("Horario Confirmado:");
  lcd.setCursor(0, 2);
  char bufferConfirmacaoHora[6];
  sprintf(bufferConfirmacaoHora, "%02d:%02d", horaConfirmada, dezenaMinutoConfirmada * 10 + unidadeMinutoConfirmada);
  lcd.print(bufferConfirmacaoHora);
  delay(3000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Hora atual:");
  lcd.setCursor(0, 2);
  lcd.print("Configurar horario:");
}

void exibirMensagemErro() {
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("Erro: Max horarios");
  lcd.setCursor(0, 2);
  lcd.print("registrados atingido.");
  delay(3000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Hora atual:");
  lcd.setCursor(0, 2);
  lcd.print("Configurar horario:");
}

void verificarAlarmes() {
  DateTime agora = rtc.now();
  int horaAtual = agora.hour();
  int minutoAtual = agora.minute();

  for (int i = 0; i < numHorariosRegistrados; i++) {
    if (horariosRegistrados[i][0] == horaAtual && 
        horariosRegistrados[i][1] == minutoAtual / 10 &&
        horariosRegistrados[i][2] == minutoAtual % 10) {
      if (!alertaAtivo && !horariosDisparados[i]) {
        acionarAlerta();
        horariosDisparados[i] = true;
      }
    }
  }
}

void acionarAlerta() {
  digitalWrite(pinoBuzzer, HIGH);
  digitalWrite(pinoLed, HIGH);
  alertaAtivo = true;
  inicioAlerta = millis();
  Serial.println("Alerta acionado.");
  girarServo();
}

void desligarAlerta() {
  digitalWrite(pinoBuzzer, LOW);
  digitalWrite(pinoLed, LOW);
  alertaAtivo = false;
  pararServo();
  Serial.println("Alerta desligado.");
}

void girarServo() {
  meuServo.write(100);
  delay(1000);
  pararServo();
}

void pararServo() {
  meuServo.write(90);
  Serial.println("Servo parado.");
}
