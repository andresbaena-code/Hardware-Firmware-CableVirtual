/**
 * ----------------------------------------------------------------------------
 * SISTEMA: Wiga V1 - LATCH (Edición Battery & Coil Guard)
 * ----------------------------------------------------------------------------
 * DESCRIPCIÓN Y LÓGICA DE PROTECCIÓN:
 * 1. DISPARO SECUENCIAL: Envía órdenes con 1.5s de diferencia para evitar 
 * picos de corriente que agoten la batería de 12V-9A.
 * 2. REINTENTOS RÁPIDOS: Si la radio falla, realiza hasta 5 intentos cada 30s.
 * 3. PERSISTENCIA DE 1 HORA: Si tras los 5 intentos rápidos no hay éxito, 
 * el sistema espera 60 MINUTOS para volver a intentar, protegiendo la 
 * batería y los solenoides de impactos innecesarios.
 * ----------------------------------------------------------------------------
 */

#include <XBee.h>
#include <LiquidCrystal_I2C.h>
#include <avr/wdt.h>

// --- CONFIGURACIÓN DE HARDWARE ---
LiquidCrystal_I2C lcd(0x27, 20, 4); 
XBee xbee = XBee();
ZBRxResponse rx = ZBRxResponse();

// --- CONSTANTES DE TIEMPO (Calculadas para Loop de 15ms) ---
const int TOTAL_ENTRADAS   = 24;
const int INTERVALO_MS     = 15;     

const int VENTANA_LATCH    = 100;    // Pausa de 1.5s entre válvulas
const long REINTENTO_30S   = 2000;   // Espera de 30s para reintento rápido
const long REINTENTO_1HORA = 240000; // Espera de 60 min para persistencia
const int MAX_INTENTOS     = 5;      

// --- VARIABLES DE ESTADO ---
int M[TOTAL_ENTRADAS];          
int C[TOTAL_ENTRADAS];          
bool verificado[TOTAL_ENTRADAS]; 
long T[TOTAL_ENTRADAS];         
int intentos[TOTAL_ENTRADAS];    
int bloqueoLatch = 0;           

// --- MAPEO DE RADIOS Y PINES ---
const uint32_t DIR[] = { 0x42878FDB, 0x42878FDB, 0x42878FDB, 0x42878FDB, 0x41C3f584, 0x41C3f584, 0x41C3f549, 0x41C3f549, 0x41C3f568, 0x41C3f568, 0x41C3f57a, 0x41C3f57a, 0x41C3f544, 0x41C3f544, 0x41C3f58b, 0x41C3f58b, 0x41C3f658, 0x41C3f658, 0x41C3f595, 0x41C3f595, 0x41C3f595, 0x41C3f595, 0x41C3f595, 0x41C3f54f };
const uint8_t IDS[]   = { 0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7 };
const int PINES[]     = { A0, A1, A2, A3, A4, A5, A6, A7, A8, 27, 5, 8, 46, 44, 11, 12, 10, 45, 9, A9, A12, A14, A13, A15 };

void setup() {
  wdt_disable();
  lcd.begin(); 
  lcd.backlight();
  dibujarInterfazFija(); 

  Serial.begin(9600);    
  Serial1.begin(9600);   
  xbee.setSerial(Serial1);

  for (int j = 0; j < TOTAL_ENTRADAS; j++) {
    pinMode(PINES[j], INPUT_PULLUP);
    int lectura = (digitalRead(PINES[j]) == LOW) ? 1 : 0;
    M[j] = lectura; C[j] = lectura; 
    verificado[j] = true; 
    T[j] = 0;
    intentos[j] = 0;
    actualizarPuntoLCD(j, C[j], false); 
  }
  wdt_enable(WDTO_8S); 
}

void loop() {
  wdt_reset();

  for (int i = 0; i < TOTAL_ENTRADAS; i++) {
    int lecturaFisica = (digitalRead(PINES[i]) == LOW) ? 1 : 0;
    if (lecturaFisica != M[i]) {
      M[i] = lecturaFisica;
      verificado[i] = false; 
      T[i] = 0;              
      intentos[i] = 0;       
      bloqueoLatch = 0;      
    }
  }

  if (bloqueoLatch <= 0) {
    for (int i = 0; i < TOTAL_ENTRADAS; i++) {
      bool puedeIntentarRapido = (!verificado[i] && intentos[i] < MAX_INTENTOS && T[i] <= 0);
      bool puedeIntentarHora   = (!verificado[i] && intentos[i] >= MAX_INTENTOS && T[i] <= 0);

      if (puedeIntentarRapido || puedeIntentarHora) {
        int estadoAEnviar = (digitalRead(PINES[i]) == LOW) ? 1 : 0;
        actualizarPuntoLCD(i, estadoAEnviar, true); 
        transmitirOrden(DIR[i], IDS[i], estadoAEnviar);
        
        if (puedeIntentarRapido) {
          intentos[i]++;
          T[i] = REINTENTO_30S; 
        } else {
          T[i] = REINTENTO_1HORA; // Espera de 60 min iniciada
        }
        
        bloqueoLatch = VENTANA_LATCH; 
        break; 
      }
    }
  } else {
    bloqueoLatch--; 
  }

  for (int i = 0; i < TOTAL_ENTRADAS; i++) {
    if (T[i] > 0) T[i]--;
  }

  gestionarRespuestasRadio(); 
  delay(INTERVALO_MS);
}

void transmitirOrden(uint32_t serieLsb, uint8_t valvulaId, uint8_t estado) {
  while(Serial1.available()) Serial1.read(); 
  uint8_t payload[] = { 0xAA, 0x14, valvulaId, (uint8_t)estado, 0x00 };
  payload[4] = 0xFF - (uint8_t)(payload[0] + payload[1] + payload[2] + payload[3]);
  XBeeAddress64 addr64 = XBeeAddress64(0x0013A200, serieLsb);
  ZBTxRequest zbTx = ZBTxRequest(addr64, payload, sizeof(payload));
  zbTx.setFrameId(0); 
  xbee.send(zbTx);
}

void gestionarRespuestasRadio() {
  xbee.readPacket();
  if (xbee.getResponse().isAvailable() && xbee.getResponse().getApiId() == ZB_RX_RESPONSE) {
    xbee.getResponse().getZBRxResponse(rx);
    uint8_t* data = rx.getData();
    if (data[0] == 0xAA && data[1] == 0x14) {
      uint32_t serieOrigen = rx.getRemoteAddress64().getLsb();
      uint8_t idRecibido   = data[2];
      uint8_t cmdRecibido  = data[3];
      for (int k = 0; k < TOTAL_ENTRADAS; k++) {
        if (DIR[k] == serieOrigen && IDS[k] == idRecibido) {
          if (cmdRecibido == M[k]) {
            C[k] = cmdRecibido;
            verificado[k] = true; 
            intentos[k] = 0;
            T[k] = 0;
            actualizarPuntoLCD(k, cmdRecibido, false); 
          }
          break;
        }
      }
    }
  }
}

void dibujarInterfazFija() {
  lcd.clear();
  // Se agregan espacios para alinear los puntos con el texto E1, E2, E3
  lcd.setCursor(0, 0); lcd.print(F("E1: . . . . . . . ."));
  lcd.setCursor(0, 1); lcd.print(F("E2: . . . . . . . ."));
  lcd.setCursor(0, 2); lcd.print(F("E3: . . . . . . . ."));
  // Centrado: 20 columnas - 15 caracteres = 5 espacios / 2 = 2 espacios iniciales
  lcd.setCursor(0, 3); lcd.print(F("  WIGA V1 - LATCH   "));
}

void actualizarPuntoLCD(int index, int estado, bool intentando) {
  int fila = index / 8; 
  // Columna ajustada para el nuevo espacio tras los dos puntos "E1: "
  int col = 4 + ((index % 8) * 2); 
  
  lcd.setCursor(col, fila);
  if (intentando) {
    lcd.print("?"); 
  } else {
    lcd.print((estado == 1) ? 0 : 1);
  }
}
