#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

#define DS1307_ADDRESS 0x68

byte zero = 0x00; 

// Inicializa o display no endereco 0x20
LiquidCrystal_I2C lcd(0x27,2,1,0,4,5,6,7,3, POSITIVE);


//Constantes dos estados de máquina
#define RUNNING 1
#define ALERT 2
#define RESET 3
#define STOPPED 4
#define CONFIG 5
#define BTMENU 1
#define BTSEL 2
#define BTOK 3
//Constantes dos pinos e tempos
const int btResetTempo=3, btTempo=4, btReset=5, buzzer=6, rlDisparo=7, rlSabot=8;
const int btMenu=9, btSelecionar=11, btOk=13;
const int tempos[17]={2,15,30,45,60,75,90,105,120,135,150,165,180,195,210,225,240};
byte sqicon[8] = {
  B11110,
  B11110,
  B11110,
  B11110,
  B11110,
  B11110,
  B11110,
  B11110
};

byte coracao[8] = {
  B00000,
  B00000,
  B11011,
  B11111,
  B11111,
  B01110,
  B00100,
  B00000
};

//Globais
int tempos_sel=0;
int disparos=0;
int state;
int proximo_disparo = 0;
int minutos=0;
unsigned long ultimo_reset = 0;
unsigned long ultima_sabotagem = 0;
unsigned long ultimo_disparo = 0;
int ciclos=0;
bool disparo_ativo=false;
bool reset_ativo=false;
bool sabotagem=false;
bool live=false;
int horaAtual = 0;
int minutosAtual = 0;
int TELA = 0;
bool configroot = false;
char* texts[] = {"1-Hora Atual", "2-Ativ. Relogio", "3-Hora Inicio", "4-Hora Fim", "5-Voltar" };
int horaInicio = 22;
int horaFim = 6;
int minutosInicio = 0;
int minutosFim = 0;
bool relogioAtivado = false;
bool telaHoraAtualInit = false;
bool telaAtivaRelogioInit = false;
bool telaHoraInicioInit = false;
bool telaHoraFimInit = false;
bool telaVoltarInit = false;
bool processTela = false;
int telaHoraAtualPos=1;
int debounceDelay = 500;
unsigned long lastgetButton=0;
unsigned long lastTimeChange=0;
int button = 0;

void conta_ciclo(){
  Serial.println(F("Method: conta_ciclo"));
  //ciclos por minutos | ciclos de 0,25s | 240 ciclos por minutos
  if(ciclos==240){
    minutos++;
    ciclos=0;
  }else{
    ciclos++;
  }
}

void atualizaTela(){
  Serial.println(F("Method: atualizaTela"));
  int minutosRestante = proximo_disparo-minutos;
  lcd.setBacklight(HIGH);
  lcd.setCursor(0,0);
  lcd.print("T:");
  if(proximo_disparo<10) lcd.print("00");
  if(proximo_disparo>9 && proximo_disparo<100) lcd.print("0");
  lcd.print(proximo_disparo);
  lcd.print(" Min ");
  lcd.print("D:");
  if(disparos<10) lcd.print("00");
  if(disparos>9 && disparos<100) lcd.print("0");
  lcd.print(disparos);
  lcd.setCursor(0,1);
  
  double dblproximo_disparo = (double)proximo_disparo;
  double dblminutosRestante = (double)minutosRestante;
  double porc = (dblminutosRestante/dblproximo_disparo);
  double dblbarToRemove = (1-porc)*16;
  int barToRemove = floor(dblbarToRemove);
  lcd.setCursor(0,1);
  for(int i=1;i<barToRemove;i++){
    lcd.print(" ");
    lcd.setCursor(i,1);
  }
}

void atualizaLCDSabotagem(){
  Serial.println(F("Method: atualizaLCDSabotagem"));
  lcd.clear();
  lcd.setBacklight(HIGH);
  lcd.setCursor(0,0);
  lcd.print("**SABOTAGEM**");
}

void onRunning(){
  Serial.println(F("Method: onRunning"));
  int proximo_alerta = proximo_disparo-1;
  if(minutos==proximo_alerta){
    digitalWrite(buzzer, HIGH);
    state=ALERT;
    Serial.println(F("ALERTA ATIVO"));
  }
}

void onAlert(){
  Serial.println(F("Method: onAlert"));
  if(minutos==proximo_disparo){
    state=RUNNING;
    disparar();
  }
}

void disparar(){
  Serial.println(F("Method: disparar"));
  digitalWrite(buzzer, LOW);
  digitalWrite(rlDisparo, HIGH);
  digitalWrite(12, HIGH);
  disparo_ativo=true;
  ultimo_disparo=millis();
  disparos++;
  gravaDisparos(disparos);
  resetVariaveis();
  Serial.println(F("DISPARO REGISTRADO"));
}

void onDisparo(){
    Serial.println(F("Method: onDisparo"));
    if(millis()>(ultimo_disparo+5000)){
      disparo_ativo=false;
      digitalWrite(rlDisparo, LOW);
      digitalWrite(12, LOW);
    }
}

void onReset(){
    Serial.println(F("Method: onReset"));
    if(digitalRead(btResetTempo)==HIGH){
      state=RUNNING;
      resetVariaveis();
    }else{
      if(millis()>(ultimo_reset+10000)){
        sabotar();
        state=RUNNING;
      }
    }
}

void sabotar(){
   Serial.println(F("Method: sabotar"));
   digitalWrite(rlSabot, HIGH);
   digitalWrite(10, HIGH);
   digitalWrite(buzzer, HIGH);
   sabotagem=true;
   ultima_sabotagem=millis();
   Serial.println(F("SABOTAGEM"));
}

void onSabotagem(){
    Serial.println(F("Method: onSabotagem"));
    digitalWrite(buzzer, HIGH);
    if(digitalRead(btResetTempo)==HIGH){
      state=RUNNING;
      sabotagem=false;
      digitalWrite(buzzer, LOW);
      digitalWrite(rlSabot, LOW);
      digitalWrite(10, LOW);
      resetVariaveis();
    }else{
      if(millis()>(ultima_sabotagem+600000)) digitalWrite(buzzer, LOW);
    }
}

void btResetTempoClick(){
    Serial.println(F("Method: btResetTempoClick"));
    resetVariaveis();
    ultimo_reset=millis();
    state=RESET;
    digitalWrite(buzzer, LOW);
    Serial.println(F("RESET TEMPO"));
}

void btTempoSelectClick(){
    Serial.println(F("Method: btTempoSelectClick"));
    if(tempos_sel<16){
      tempos_sel++;
    }
    else{
      tempos_sel=0;
    }
    EEPROM.write(2, tempos_sel);
    proximo_disparo = tempos[tempos_sel];
    int proximo_alerta = proximo_disparo-1;
    state=RUNNING;
    disparos=0;
    resetVariaveis();
}

void btResetClick(){
    Serial.println(F("Method: btResetClick"));
    state=RUNNING;
    disparos=0;
    gravaDisparos(disparos);
    resetVariaveis();
}

void resetVariaveis(){
  Serial.println(F("Method: Reset Var"));
  minutos=0;
  ciclos=0;
  lcd.setCursor(0,1);
  for(int i=0;i<16;i++) lcd.write(1);
}


void gravaDisparos(int numero){
  Serial.println(F("Method: gravaDisparos"));
  //EEPROM.write(0, numero/256);
  //EEPROM.write(1, numero%256);
  EEPROM.write(0,numero);
}

int leDisparos(){
  Serial.println(F("Method: leDisparos"));
  //int parte1 = EEPROM.read(0);
  //int parte2 = EEPROM.read(1);
  //int valor_original = (parte1 * 256) + parte2;
  int valor_original = EEPROM.read(0);
  return valor_original;
}

void liveStatus(){
  if(live){
      lcd.setCursor(15,0);
      lcd.write(2);
      live=false;
  }else{
      lcd.setCursor(15,0);
      lcd.write(" ");
      live=true;
  }
}

void setHora(int horas, int minutos){
  Serial.println(F("Method: setHora"));
  byte bhoras = horas;
  byte bminutos = minutos;
  byte bsegundos = 0;
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write(zero); //Stop no CI para que o mesmo possa receber os dados
  Wire.write(ConverteParaBCD(bsegundos));
  Wire.write(ConverteParaBCD(bminutos));
  Wire.write(ConverteParaBCD(bhoras));
  Wire.write(zero); //Start no CI
  Wire.endTransmission(); 
}

void getHora(){
  Serial.println(F("Method: getHora"));
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write(zero);
  Wire.endTransmission();
  Wire.requestFrom(DS1307_ADDRESS, 7);
  int segundos = ConverteparaDecimal(Wire.read());
  int minutos = ConverteparaDecimal(Wire.read());
  int horas = ConverteparaDecimal(Wire.read() & 0b111111); 
  int diadasemana = ConverteparaDecimal(Wire.read()); 
  int diadomes = ConverteparaDecimal(Wire.read());
  int mes = ConverteparaDecimal(Wire.read());
  int ano = ConverteparaDecimal(Wire.read());
  //coloca valor na Global horaAtual
  horaAtual = horas;
  minutosAtual = minutos;
}


byte ConverteParaBCD(byte val){ //Converte o número de decimal para BCD
  return ( (val/10*16) + (val%10) );
}

byte ConverteparaDecimal(byte val)  { //Converte de BCD para decimal
  return ( (val/16*10) + (val%16) );
}


void printLCD(int col, int lin, char* text, bool cls){
  if(cls) lcd.clear();
  lcd.setCursor(col,lin);
  lcd.print(text);
}

void getButton(){
  Serial.println(F("Method: getButton"));
  button = 0;
  if((millis()-lastgetButton)<debounceDelay){
    
  }
  else{
    lastgetButton=millis();
    if(digitalRead(btSelecionar)==LOW) button =  BTSEL;
    if(digitalRead(btMenu)==LOW) button =  BTMENU;
    if(digitalRead(btOk)==LOW) button = BTOK;
  }
  if(button!=0) lastTimeChange=millis();
  Serial.print(F("Button: "));
  Serial.println(button);
  if((millis()-lastTimeChange)>10000){
    TELA=0;
    button=0;
    configroot = false;
    lcd.setCursor(0,1);
    for(int i=0;i<16;i++) lcd.write(1);
  }

}

void printTime(int hora, int minuto){
  if(hora>9){
    lcd.print(hora);
  }else{
    lcd.print("0");
    lcd.print(hora);
  }
  lcd.print(":");
   if(minuto>9){
    lcd.print(minuto);
  }else{
    lcd.print("0");
    lcd.print(minuto);
  }
}

void telaHoraAtual(){
  Serial.println(F("Method: telaHoraAtual"));
  processTela = true;
  if(!telaHoraAtualInit){
    Serial.println(F("telaHoraAtualInit"));
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("1-Hora Atual:");
    lcd.setCursor(4,1);
    printTime(horaAtual, minutosAtual);
    lcd.setCursor(3,1);
    lcd.blink();
    telaHoraAtualInit = true;
    telaHoraAtualPos=1;
    TELA=1;
    delay(150);
    button=0;
  }
  

  if(button==BTMENU){
    telaHoraAtualInit = false;
    telaHoraAtualPos=1;
    TELA=2;
  }

  if(button==BTSEL){
    Serial.println(F("telaHoraAtual Button SEL"));
    if(telaHoraAtualPos==1){
      if(horaAtual>=23) horaAtual=0;
      else horaAtual++;
      lcd.setCursor(4,1);
      printTime(horaAtual, minutosAtual);
      lcd.setCursor(3,1);
    }
    if(telaHoraAtualPos==2){
      if(minutosAtual>=59) minutosAtual=0;
      else minutosAtual++;
      lcd.setCursor(4,1);
      printTime(horaAtual, minutosAtual);
      lcd.setCursor(9,1);
    }
  }

  if(button==BTOK){
    if(telaHoraAtualPos==1){
      telaHoraAtualPos++;
      lcd.setCursor(9,1);
      return;
    }else{
      lcd.noBlink();
      lcd.clear();
      lcd.setCursor(0,0);
      setHora(horaAtual, minutosAtual);
      lcd.print("GRAVADO!");
      delay(200);
      getHora();
      telaHoraAtualInit=false;
    }
  }
}

void telaAtivaRelogio(){
  Serial.println(F("Method: telaAtivaRelogio"));
  processTela = true;
  if(!telaAtivaRelogioInit){
    lcd.clear();
    lcd.noBlink();
    lcd.setCursor(0,0);
    lcd.print("2-Relogio:");
    lcd.setCursor(3,1);
    if(EEPROM.read(9)==1) relogioAtivado = true;
    else relogioAtivado = false;
    if(relogioAtivado) lcd.print("**ATIVADO**");
    else lcd.print("DESATIVADO");
    telaAtivaRelogioInit = true;
    TELA=2;
    delay(150);
    button=0;
  }

  
   if(button==BTMENU){
    telaAtivaRelogioInit = false;
    TELA=3;
   }

  if(button==BTSEL){
    if(relogioAtivado){
      relogioAtivado=false;
      lcd.setCursor(3,1);
      lcd.print("DESATIVADO");
    }else{
      relogioAtivado=true;
      lcd.setCursor(3,1);
      lcd.print("**ATIVADO**");
    }
  }

    if(button==BTOK){
      lcd.noBlink();
      lcd.clear();
      lcd.setCursor(0,0);
      if(relogioAtivado){
        EEPROM.write(9,1);
      }else{
        state=RUNNING;
        EEPROM.write(9,0);
      }
      lcd.print("GRAVADO!");
      delay(200);
      getHora();
      telaAtivaRelogioInit=false;
  }
  
}

void telaHoraInicio(){
  Serial.println(F("Method: telaHoraInicio"));
  processTela = true;
  if(!telaHoraInicioInit){
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("3-Hora Inicio:");
    lcd.setCursor(4,1);
    printTime(horaInicio, minutosInicio);
    lcd.setCursor(3,1);
    lcd.blink();
    telaHoraInicioInit = true;
    telaHoraAtualPos=1;
    TELA=3;
    delay(150);
    return;
  }
  
  if(button==BTMENU){
    telaHoraInicioInit = false;
    telaHoraAtualPos=1;
    TELA=4;
  }

  if(button==BTSEL){
    if(telaHoraAtualPos==1){
      if(horaInicio>=23) horaInicio=0;
      else horaInicio++;
      lcd.setCursor(4,1);
      printTime(horaInicio, minutosInicio);
      lcd.setCursor(3,1);
    }
    if(telaHoraAtualPos==2){
      if(minutosInicio>=59) minutosInicio=0;
      else minutosInicio++;
      lcd.setCursor(4,1);
      printTime(horaInicio, minutosInicio);
      lcd.setCursor(9,1);
    }
  }

  if(button==BTOK){
    if(telaHoraAtualPos==1){
      telaHoraAtualPos++;
      lcd.setCursor(9,1);
    }else{
      lcd.noBlink();
      lcd.clear();
      lcd.setCursor(0,0);
      EEPROM.write(5,horaInicio);
      EEPROM.write(6,minutosInicio);
      lcd.print("GRAVADO!");
      delay(200);
      telaHoraInicioInit=false;
    }
  }
}


void telaHoraFim(){
  Serial.println(F("Method: telaHoraFim"));
  processTela = true;
  if(!telaHoraFimInit){
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("4-Hora Fim:");
    lcd.setCursor(4,1);
    printTime(horaFim, minutosFim);
    lcd.setCursor(3,1);
    lcd.blink();
    telaHoraFimInit = true;
    telaHoraAtualPos=1;
    TELA=4;
    delay(150);
    button=0;
  }
  
  if(button==BTMENU){
    telaHoraFimInit = false;
    telaHoraAtualPos=1;
    TELA=5;
  }

  if(button==BTSEL){
    if(telaHoraAtualPos==1){
      if(horaFim>=23) horaFim=0;
      else horaFim++;
      lcd.setCursor(4,1);
      printTime(horaFim, minutosFim);
      lcd.setCursor(3,1);
    }
    if(telaHoraAtualPos==2){
      if(minutosFim>=59) minutosFim=0;
      else minutosFim++;
      lcd.setCursor(4,1);
      printTime(horaFim, minutosFim);
      lcd.setCursor(9,1);
    }
  }

  if(button==BTOK){
    if(telaHoraAtualPos==1){
      telaHoraAtualPos++;
      lcd.setCursor(9,1);
    }else{
      lcd.noBlink();
      lcd.clear();
      lcd.setCursor(0,0);
      EEPROM.write(7,horaFim);
      EEPROM.write(8,minutosFim);
      lcd.print("GRAVADO!");
      delay(200);
      telaHoraFimInit=false;
    }
  }
}

void telaVoltar(){
  Serial.println(F("Method: telaVoltar"));
  processTela = true;
  if(!telaVoltarInit){
    lcd.clear();
    lcd.noBlink();
    lcd.setCursor(0,0);
    lcd.print("5-Sair");
    telaVoltarInit = true;
    TELA=5;
    delay(150);
    button=0;
  }

  if(button==BTMENU){
    telaVoltarInit = false;
    TELA=1;
  }

  if(button==BTOK){
    TELA=0;
    configroot = false;
    telaVoltarInit = false;
    lcd.setCursor(0,1);
    for(int i=0;i<16;i++) lcd.write(1);
  }
}

void checkHorario(){
  Serial.println(F("Method: checkHorario"));
  int lastState = state;
  if(horaInicio<horaFim && horaInicio<horaAtual && horaFim>horaAtual) state = RUNNING;
  if(horaInicio>horaFim && horaInicio<horaAtual && horaFim<horaAtual) state = RUNNING;
  if(horaInicio>horaFim && horaInicio>horaAtual && horaFim>horaAtual) state = RUNNING;

  if(horaInicio<horaFim && horaInicio<horaAtual && horaFim<horaAtual) state = STOPPED;
  if(horaInicio<horaFim && horaInicio>horaAtual && horaFim>horaAtual) state = STOPPED;
  if(horaInicio>horaFim && horaInicio>horaAtual && horaFim<horaAtual) state = STOPPED;

  if(horaAtual==horaInicio){
    if(minutosAtual<minutosInicio) state = STOPPED;
    if(minutosAtual>=minutosInicio) state = RUNNING;
  }

  if(horaAtual==horaFim){
    if(minutosAtual<minutosFim) state = RUNNING;
    if(minutosAtual>=minutosFim) state = STOPPED;
  }

  if(lastState==STOPPED && state==RUNNING){
    lcd.setCursor(0,1);
    for(int i=0;i<16;i++) lcd.write(1);
  }
}

void onParado(){
    Serial.println(F("Method: onParado"));
    lcd.clear();
    lcd.noBlink();
    lcd.setCursor(0,0);
    lcd.print("Turno de Vigia:");
    lcd.setCursor(0,1);
    printTime(horaInicio, minutosInicio);
    lcd.print(" - ");
    printTime(horaFim, minutosFim);
    delay(750);
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Hora Atual:");
    lcd.setCursor(4,1);
    printTime(horaAtual, minutosAtual);
}

void setup() {
 Serial.println(F("INICIANDO"));
 //Configura Pinos
 pinMode(btResetTempo, INPUT_PULLUP);
 pinMode(btTempo, INPUT_PULLUP);
 pinMode(btReset, INPUT_PULLUP);
 pinMode(btMenu, INPUT_PULLUP);
 pinMode(btSelecionar, INPUT_PULLUP);
 pinMode(btOk, INPUT_PULLUP);
 pinMode(buzzer, OUTPUT);
 pinMode(rlDisparo, OUTPUT);
 pinMode(rlSabot, OUTPUT);
 pinMode(10, OUTPUT);
 pinMode(12, OUTPUT);

 digitalWrite(rlDisparo, LOW);
 digitalWrite(rlSabot, LOW);
 digitalWrite(10, LOW);
 digitalWrite(12, LOW);
 
 //Lê EEPROM
 Serial.println(F("LENDO EEPROM"));
 disparos = leDisparos();
 if(disparos==254){
  disparos=0;
  gravaDisparos(disparos);
 }


 tempos_sel = EEPROM.read(2);
 if(tempos_sel>16){
  tempos_sel=0;
  EEPROM.write(2, tempos_sel);
 }
 
 state=RUNNING;
 proximo_disparo=tempos[tempos_sel];


 Serial.println(F("CONFIGURANDO LCD"));
 //Configura LCD
 lcd.begin (16,2);
 lcd.createChar(1, sqicon);
 lcd.createChar(2, coracao);
 lcd.setCursor(0,1);
 for(int i=0;i<16;i++) lcd.write(1);

 //Configura I2C para DS1307
 Wire.begin();
 getHora();
 if(horaAtual>23 || minutosAtual>59){
  setHora(21, 58);
  getHora();
 }

 
 horaInicio = EEPROM.read(5);
 minutosInicio = EEPROM.read(6);
 horaFim = EEPROM.read(7);
 minutosFim = EEPROM.read(8);
 if(EEPROM.read(9)==1) relogioAtivado = true;
 else relogioAtivado = false;

 if(horaInicio>23) horaInicio=22;
 if(minutosInicio>50) minutosInicio=0;
 if(horaFim>23) horaFim=6;
 if(minutosFim>50) minutosFim=0;

 Serial.begin(9600);
}

void loop() {
  Serial.println(F("=======NEW LOOP================"));
  
  if(state!=RESET && configroot==false){
     if(digitalRead(btResetTempo)==LOW) btResetTempoClick();
     if(digitalRead(btTempo)==LOW) btTempoSelectClick();
     if(digitalRead(btReset)==LOW) btResetClick();
     if(digitalRead(btMenu)==LOW){
        configroot = true;
        telaHoraAtual();
     }
  }

  if(state==STOPPED && configroot==false) onParado();
  if(state==RUNNING && configroot==false) onRunning();
  if(state==ALERT && configroot==false) onAlert();
  if(state==RESET && !sabotagem && configroot==false) onReset();
  if(disparo_ativo && configroot==false) onDisparo();
  if(sabotagem && configroot==false) onSabotagem();
  if(disparos>254 && configroot==false) btResetClick(); //reset disparos e EEPROM


  if(configroot){
      getButton();
      processTela = false;
      if(TELA==1 && processTela==false)telaHoraAtual();
      if(TELA==2 && processTela==false) telaAtivaRelogio();
      if(TELA==3 && processTela==false)telaHoraInicio();
      if(TELA==4 && processTela==false) telaHoraFim();
      if(TELA==5 && processTela==false) telaVoltar();
  }else{
    if(state!=STOPPED){
      conta_ciclo();
      if(sabotagem){
        atualizaLCDSabotagem();
      }else{
        atualizaTela();
      }
    }
    liveStatus();
    if(relogioAtivado){
      getHora();
      checkHorario();
    }
  }
  delay(250);
}



