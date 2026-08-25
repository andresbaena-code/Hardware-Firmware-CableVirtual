#include <XBee.h>
#include <LiquidCrystal_I2C.h>
  #include <avr/wdt.h> // Incluir la librería que contiene el watchdog (wdt.h)
  
  LiquidCrystal_I2C lcd(0x27, 16, 3);
  XBee xbee = XBee();
  
  ZBRxResponse rx = ZBRxResponse();
  
  int M[24]; //ESTADO IDEAL DE LA VÁLVULA
int C[24]; //ESTADO REAL DE LA VÁLVULA
int T[30]; //TIEMPO DE ESPERA ENTRE TRAMAS
int E[24]; //VALVULA CON ERROR
long contador = 0;
int i = 0;
int estado = 0;
const int maxM = 5; //# validaciones antirebote encendido
const int minM = 0; //# validaciones antirebote apagado
const int Entradas = 24; //# entradas digitales
const int maxE = 5; //#limite para enviar al matadero
int ultimoRevisado = 0;  //esto es que valida valvula por valvula entonces para saber en cual va
////los nodo x pro tienen las entradas invertidas por eso van de la 7 a la 0 
//////////////////////////BLOQUE        Dirección     N° Valvulas   # Finca/////////////////////////////////////
////////////////////////// 1            0x415CCBE8    5             1-5     ////////////////////////////////////
////////////////////////// 2            0x4197C1CD    5             6-10    ////////////////////////////////////
////////////////////////// 3            0x4197C28F    4             11-14   ////////////////////////////////////
////////////////////////// 4            0x4197BC38    5             15-20   ////////////////////////////////////
////////////////////////// 5            0x4197BC5E    4             21-24   ////////////////////////////////////
//°_°_||_°_°_||_°_°_||_°-°//°_°_||_°_°_||_°_°_||_°-°//°_°_||_°_°_||_°_°_||_°-°//°_°_||_°_°_||_°_°_||_°-°//°_°_||
/////////////////////////{dirección bloque 1- 0x4197C2B7- 5 valvulas 1-5-} {dir bloque 2-0x4197C1CD -5 válvulas- 6-10--} {dir bloque 3 0x4197C28F - 4 val-} {dir bloque 4 0x4197BC38- 6 val} {dir bloque 5 0x4197BC5E 4 valv}

uint32_t direcciones[] = {0x418446DC, 0x418446DC, 0x418446DC, 0x418446DC, 0x418446DC, 0x418446DC, 0x418446DC, 0x418446DC, 0x415CCBE8,0x415CCBE8, 0x415CCBE8, 0x415CCBE8,0x415CCBE8, 0x415CCBE8, 0x415CCBE8, 0x415CCBE8,  0x415CCBE8,  0x415CCBE8,  0x415CCBE8,  0x415CCBE8, 0x415CCBE8, 0x415CCBE8, 0x415CCBE8, 0x415CCBE8};
uint8_t entradas[] = {0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7};//bloque 1 tiene la salida 6 mala se cambio por la 1, bloque 4 tiene la salida 7 mala toco cambiarla por la 0
int pines[] = {A0, A1, A2, A3, A4, A5, A6, A7, A8, 27, 5, 8, 46, 44, 11, 12, 10, 45, 9, A9, A12, A14, A13, A15};
const int alarma = 13;  // led indicador de que hay por lo menos una válvula en el matadero
const int speedWiga = 100;
const int maxT = 300 / speedWiga; //Tiempo de espera entre tramas
const long pulso = 1000000;//1000000 / speedWiga; // tiempo de validacion para apagado

/////////////////////////////////////////////////////////
int mapear(uint32_t direccion, int entrada) {

  int index = 100;

  for (int i = 0; i < sizeof(direcciones); i++) {

    if (direcciones[i] == direccion && entradas[i] == entrada) {
      index = i;
    }
  }
  return index;
}

void operarValvula(uint32_t direccion, uint8_t entrada, uint8_t accion) {

  uint8_t payload[] = {0xaa, 0x14, entrada, accion};


  XBeeAddress64 destino = XBeeAddress64(0x0013A200, direccion);
  ZBTxRequest zbTx2 = ZBTxRequest(destino, payload, sizeof(payload));
  zbTx2.setFrameId(0);
  xbee.send(zbTx2);
}

void leerRespuesta() {

  xbee.readPacket();

  if (xbee.getResponse().isAvailable()) {

    if (xbee.getResponse().getApiId() == ZB_RX_RESPONSE) {

      xbee.getResponse().getZBRxResponse(rx);
      int paqueteRecibido [rx.getDataLength()];

      for (int i = 0; i < rx.getDataLength(); i++) {

        paqueteRecibido[i] = rx.getData()[i] ;

      }

      if (paqueteRecibido[0] == 0xaa && paqueteRecibido[1] == 0x14) {

        XBeeAddress64 dir;
        dir = rx.getRemoteAddress64() ;
        uint32_t direccion = dir.getLsb ();
        int entrada = paqueteRecibido[2];
        int accion = paqueteRecibido[3];
       
        Serial.print("entrada :");
        Serial.println(entrada,HEX);
        Serial.print("accion :");
        Serial.println(accion,HEX);
  
        int index = mapear(direccion, entrada);
        C[index] = accion;
        E[index] = 0;
        Imprimir_LCD(index, accion);
      }
    }
  }
}

void Imprimir_LCD(uint8_t entrada, uint8_t estado) {
  int row;
  int col;
  if (entrada < 8) {
    row = 0;
  }
  else if (entrada >= 8 and entrada < 16) {
    row = 1;
    entrada = entrada - 8;
  }
  else if (entrada >= 16 and entrada < 24) {
    row = 2;
    entrada = entrada - 16;
  }
  else {
    //Serial.println("ERROR");
    lcd.setCursor(0, 3);
    lcd.print("Entrada Invalida");
  }
  if (entrada == 0) {
    col = 5;
  }
  else {
    col = entrada + 5;
  }
  lcd.setCursor(col + entrada, row);
  lcd.print(estado);
}

void setup() {
  // put your setup code here, to run once:
  wdt_disable(); // Desactivar el watchdog mientras se configura
  wdt_enable(WDTO_8S); // Configurar a 8 segundos
  i = 0;
  contador = 0;
  lcd.begin();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("EXP1 0 0 0 0 0 0 0 0");
  lcd.setCursor(0, 1);
  lcd.print("EXP2 0 0 0 0 0 0 0 0");
  lcd.setCursor(0, 2);
  lcd.print("EXP3 0 0 0 0 0 0 0 0");
  lcd.setCursor(0, 3);
  lcd.print("SOLUCIONES WIGA!!!");
 

  Serial1.begin(9600);//PUERTO DE COMUNICACION DEL XBEE
  //Serial1.println("Inicio...");
  Serial.begin(9600);//PUERTO DE COMUNICACION DEBUG
  Serial.println("Inicio...");
  xbee.setSerial(Serial1);

  for (int j = 0; j < Entradas; j++) {
    pinMode(entradas[j], INPUT);
  }
  pinMode(alarma, OUTPUT);
}

void loop() {

  wdt_reset(); // Actualizar el watchdog para que no produzca un reinicio
  i = (i + 1) % Entradas;
  contador++;
  if (contador>=9000){
    Serial.print("contador :");
    Serial.println(contador);
  }
  
  estado = digitalRead(pines[i]);

  if (estado == HIGH) {
    M[i] = min(M[i] + 1, maxM);
  }

  if (estado == LOW) {
    M[i] = max(M[i] - 1, minM);
  }

  delay(speedWiga);

  leerRespuesta();

  if (M[i] == minM && C[i] == 1 && T[i] == 0 && E[i] < maxE) {
    operarValvula(direcciones[i], entradas[i], 0);
    T[i] = maxT;
    E[i] = E[i] + 1;
    //Imprimir_LCD(i, 0);

  }

  if (M[i] == maxM && C[i] == 0 && T[i] == 0 && E[i] < maxE) {
    operarValvula(direcciones[i], entradas[i], 1);
    delay(500);
    operarValvula(direcciones[i], entradas[i], 1);
    T[i] = maxT;
    E[i] = E[i] + 1;
    //Imprimir_LCD(i, 1);
  }

  if (T[i] > 0) {
    T[i] = T[i] - 1;
  }

  if (contador > pulso) {
    Serial.println("VALIDACION");
    ultimoRevisado = (ultimoRevisado + 1) % Entradas;
    if (M[ultimoRevisado] == 0) {
      operarValvula(direcciones[ultimoRevisado], entradas[ultimoRevisado], 0);
    }
    contador = 0;
  }
  int problemas = 0;
  for (int k = 0; k < Entradas; k++) {
    if (E[k] >= maxE) {
      problemas++;
      Imprimir_LCD(k, 2);
      E[k] = 0;
    }
  }
  //  if (problemas > 0) {
  //    digitalWrite(alarma, HIGH);
  //  } else {
  //    digitalWrite(alarma, LOW);
  //  }

}
