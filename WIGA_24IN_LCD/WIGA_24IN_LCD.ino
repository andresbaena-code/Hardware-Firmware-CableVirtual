#include <XBee.h>
#include <LiquidCrystal_I2C.h>
#include <avr/wdt.h>

// Definiciones de hardware
LiquidCrystal_I2C lcd(0x27, 20, 4); // Ajustado a tu LCD de 4 filas
XBee xbee = XBee();
ZBRxResponse rx = ZBRxResponse();

// Variables de control (Basadas en tu código exitoso)
const int Entradas = 24;
int M[Entradas]; // Estado ideal
int C[Entradas]; // Estado real
int T[Entradas]; // Tiempo de espera entre reintentos
int E[Entradas]; // Contador de errores por válvula

// Direcciones y Pines (Tu configuración exacta)
uint32_t direcciones[] = { 0x41E7F190, 0x41E7F190, 0x42878fdb, 0x42878fdb, 0x41C3f584, 0x41C3f584, 0x41C3f549, 0x41C3f549, 0x41C3f568, 0x41C3f568, 0x41C3f57a, 0x41C3f57a, 0x41C3f544, 0x41C3f544, 0x41C3f58b, 0x41C3f58b, 0x41C3f658, 0x41C3f658, 0x41C3f595, 0x41C3f595, 0x41C3f595, 0x41C3f595, 0x41C3f595, 0x41C3f54f };
uint8_t ids[] = { 0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7 };
int pines[] = { A0, A1, A2, A3, A4, A5, A6, A7, A8, 27, 5, 8, 46, 44, 11, 12, 10, 45, 9, A9, A12, A14, A13, A15 };

// Configuración de tiempos
const int maxT = 15;        // Espera entre tramas (reintentos)
const int maxE = 5;         // Intentos máximos antes de marcar error "2"
int i = 0;                  // Índice del loop

void setup() {
  wdt_disable();
  lcd.begin();
  lcd.backlight();
  
  Serial.begin(9600);
  Serial1.begin(9600);
  xbee.setSerial(Serial1);
  
  dibujarInterfazFija();
  
  for (int j = 0; j < Entradas; j++) {
    pinMode(pines[j], INPUT_PULLUP); // Usamos Pullup para estabilidad
    M[j] = 0;
    C[j] = 0;
    T[j] = 0;
    E[j] = 0;
  }
  
  wdt_enable(WDTO_8S);
  Serial.println("Sistema Wiga Mejorado Iniciado...");
}

void loop() {
  wdt_reset();
  
  // 1. Lectura de entrada actual (Secuencial como tu código)
  int lectura = digitalRead(pines[i]);
  
  // Lógica de "Ideal" (Ajusta HIGH/LOW según tu PLC)
  if (lectura == HIGH) M[i] = 1; else M[i] = 0;

  // 2. Revisar si hay que enviar trama (Si ideal != real y no estamos esperando tiempo T)
  if (M[i] != C[i] && T[i] == 0 && E[i] < maxE) {
    operarValvula(direcciones[i], ids[i], M[i]);
    T[i] = maxT; 
    E[i]++;
    actualizarPuntoLCD(i, M[i], true); // Mostrar "?" de enviado
  }

  // 3. Leer respuestas del Radio (Frecuente)
  leerRespuesta();

  // 4. Gestión de Tiempos
  if (T[i] > 0) T[i]--;
  
  // 5. Gestión de errores (El "matadero")
  if (E[i] >= maxE) {
    actualizarPuntoLCD(i, 2, false); // Error crítico
    if (T[i] == 0) E[i] = 0; // Opcional: resetear después de un tiempo para reintentar
  }

  // Pasar a la siguiente válvula
  i = (i + 1) % Entradas;
  delay(10); // Velocidad de escaneo
}

void operarValvula(uint32_t dir, uint8_t id, uint8_t accion) {
  uint8_t payload[] = { 0xaa, 0x14, id, accion };
  XBeeAddress64 destino = XBeeAddress64(0x0013A200, dir);
  ZBTxRequest zbTx = ZBTxRequest(destino, payload, sizeof(payload));
  zbTx.setFrameId(0);
  xbee.send(zbTx);
}

void leerRespuesta() {
  xbee.readPacket();
  if (xbee.getResponse().isAvailable() && xbee.getResponse().getApiId() == ZB_RX_RESPONSE) {
    xbee.getResponse().getZBRxResponse(rx);
    uint8_t* data = rx.getData();
    
    if (data[0] == 0xaa && data[1] == 0x14) {
      uint32_t dirConfirmada = rx.getRemoteAddress64().getLsb();
      uint8_t idConfirmado = data[2];
      uint8_t accionConfirmada = data[3];

      // Buscar el índice para actualizar
      for (int k = 0; k < Entradas; k++) {
        if (direcciones[k] == dirConfirmada && ids[k] == idConfirmado) {
          C[k] = accionConfirmada;
          E[k] = 0;
          T[k] = 0;
          actualizarPuntoLCD(k, C[k], false);
          break;
        }
      }
    }
  }
}

void dibujarInterfazFija() {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("E1: 0 0 0 0 0 0 0 0");
  lcd.setCursor(0, 1); lcd.print("E2: 0 0 0 0 0 0 0 0");
  lcd.setCursor(0, 2); lcd.print("E3: 0 0 0 0 0 0 0 0");
  lcd.setCursor(0, 3); lcd.print("SOLUCIONES WIGA V4");
}

void actualizarPuntoLCD(int index, int estado, bool intentando) {
  int row = index / 8;
  int col = 4 + (index % 8) * 2;
  lcd.setCursor(col, row);
  if (intentando) lcd.print("?");
  else lcd.print(estado);
}
