#include <Wire.h>
#include <BleKeyboard.h>
#include <Preferences.h>

BleKeyboard bleKeyboard("LittleMac S3", "Espressif", 100);
Preferences prefs;   // memoria permanente del ESP (guarda la calibración)

// 📌 PINES I2C: un bus por mano
const int SDA_PIN  = 8;   // Bus 1 (Wire)  -> mano derecha
const int SCL_PIN  = 9;
const int SDA2_PIN = 10;  // Bus 2 (Wire1) -> mano izquierda (según tu conexión actual)
const int SCL2_PIN = 11;
const uint32_t VELOCIDAD_I2C = 100000;   // 100 kHz: estable con cable de timbre/largo

// 🔔 AVISOS CON COMPONENTES (opcionales). -1 si no se usan.
const int PIN_BUZZER = -1;
const int PIN_LED    = -1;

// 🖥️ AVISOS EN LA PANTALLA DE LA PC (muneco_guante.py)
const bool USAR_PANTALLA_PC = true;

// 🔘 BOTÓN BOOT: corto = solo guardia | largo (3 s) = calibración completa
const int PIN_BOTON_CALIB = 0;
const unsigned long BOTON_LARGO_MS = 3000;

// =========================================================================
// ⚙️ MODO: false = manda todas las teclas (para el muñeco en vivo y Punch-Out!!)
// =========================================================================
const bool MODO_PRUEBA = false;

// =========================================================================
// ⌨️ TECLAS: asignadas 1 a 1 con el juego y muneco_guante.py
// =========================================================================
const uint8_t TECLA_GOLPE_DER          = 'd';
const uint8_t TECLA_GOLPE_IZQ          = 'f';
const uint8_t TECLA_MODIFICADOR_CABEZA = KEY_UP_ARROW;  // golpe a la cabeza = tecla del golpe + arriba
const uint8_t TECLA_ESTRELLA           = KEY_RETURN;
const uint8_t TECLA_ESQUIVE_DER        = KEY_RIGHT_ARROW;
const uint8_t TECLA_ESQUIVE_IZQ        = KEY_LEFT_ARROW;
const uint8_t TECLA_LEVANTARSE         = 'd';           // se manda junto con ARRIBA en ráfaga
const uint8_t TECLA_CUBRIRSE           = KEY_DOWN_ARROW;
// =========================================================================

const int ACCION_NINGUNA    = 0;
const int ACCION_ESTRELLA   = 1;
const int ACCION_LEVANTARSE = 2;

// =========================================================================
// 🎛️ PERILLAS DE GOLPES (compartidas por las dos manos)
// =========================================================================
int   UMBRAL_DISPARO_GOLPE   = 95;
int   UMBRAL_RETORNO_GUARDIA = 14;
float ANGULO_CABEZA_GRADOS   = 29.50;   // solo se usa si la calibración de golpes no pudo medir esa mano
const unsigned long CANDADO_RETORNO_BRAZO = 450;
const unsigned long VENTANA_DECISION      = 70;
const unsigned long DURACION_TECLA        = 35;
const int  SIGNO_AVANCE    = 0;
const bool MOSTRAR_GUARDIA = true;

// =========================================================================
// 🖐️ PERILLAS DEL PECHO (golpe estrella y levantarse)
// =========================================================================
const float UMBRAL_PECHO_NORM     = 0.50;   // mínimo; la calibración puede subirlo por mano
const float RETORNO_PECHO_NORM    = 0.30;
const float AMPLITUD_MINIMA_PECHO = 8.0;
const unsigned long TIEMPO_CARGA_ESTRELLA   = 400;
const unsigned long TIEMPO_CARGA_LEVANTARSE = 150;
const unsigned long PERIODO_RAFAGA_MS       = 110;
const unsigned long DURACION_RAFAGA_MS      = 45;
const unsigned long MAX_LEVANTARSE_MS       = 15000;
const unsigned long BLOQUEO_POST_PECHO_MS   = 1000;
const bool LEVANTARSE_SOLO_MENSAJE = MODO_PRUEBA;

// =========================================================================
// 🛡️ PERILLAS DE CUBRIRSE (las dos manos)
// =========================================================================
const bool  CUBRIRSE_SOLO_MENSAJE   = MODO_PRUEBA;
const bool  CUBRIRSE_MANTENER_TECLA = true;
const unsigned long DURACION_TOQUE_CUBRIR = 150;
const float UMBRAL_CUBRIR_NORM      = 0.55;   // mínimo; la calibración puede subirlo
const float RETORNO_CUBRIR_NORM     = 0.30;
const float PARECIDO_MIN_CUBRIR     = 0.75;
const float AMPLITUD_MINIMA_CUBRIR  = 8.0;
const unsigned long TIEMPO_CARGA_CUBRIR  = 200;
const unsigned long MAX_CUBRIR_MS        = 10000;
const unsigned long DURACION_FASE_CUBRIR = 4500;

// =========================================================================
// 🌀 PERILLAS DEL ESQUIVE
// =========================================================================
const bool  ESQUIVE_SOLO_MENSAJE    = MODO_PRUEBA;
const bool  ESQUIVE_MANTENER_TECLA  = true;
const unsigned long DURACION_ESQUIVE = 150;
const unsigned long MAX_ESQUIVE_MS   = 5000;
const int   EJE_ESQUIVE             = -1;
const float UMBRAL_ESQUIVE_NORM     = 0.45;
const float UMBRAL_UNA_MANO_NORM    = 0.70;
const float MIN_POR_MANO_NORM       = 0.35;   // mínimo; la calibración puede subirlo
const float MAX_DIF_NORM            = 0.40;
const float MIN_GRADOS_ESQUIVE      = 6.0;
const float POSICION_MIN_NORM       = 0.25;
const float RETORNO_POSICION_NORM   = 0.30;
const float TAU_BASE_ESQUIVE        = 1.0;
const float TAU_SUAVIZADO           = 0.08;
const float AMPLITUD_MINIMA         = 5.0;
const unsigned long TIEMPO_CONFIRMA_ESQUIVE = 130;
const unsigned long COOLDOWN_ESQUIVE        = 500;
const unsigned long BLOQUEO_GESTOS_MS       = 600;
const bool DEBUG_ESQUIVE       = false;
const bool DEBUG_DELTA_ESQUIVE = false;

// =========================================================================
// 🥊 CALIBRACIÓN DE GOLPES Y MÁRGENES (se mide en la calibración guiada)
// =========================================================================
const bool  CALIB_GOLPES_AL_ARRANCAR = true;
const int   GOLPES_MAX               = 8;      // tamaño de las listas (no cambiar)
const int   GOLPES_POR_FASE          = 4;      // golpes que pide cada paso (1-8). Debe ser igual en muneco_guante.py
const unsigned long GOLPES_TIMEOUT_MS = 25000; // si no llegan los golpes, pasa al siguiente paso
const unsigned long GOLPES_INICIO_MS  = 1500;  // ms desde que pide: antes no cuenta (te estás acomodando)
const float SEPARACION_MIN_CABEZA     = 8.0;   // °: cabeza y cuerpo deben distinguirse al menos esto con cada mano
const float CONFLICTO_MAX             = 0.85;  // si un margen calculado pasa de esto, dos gestos se confunden
const float BLOQUEO_POST_GOLPE_MIN    = 600;   // ms mínimos que se bloquea el esquive tras un golpe
const float BLOQUEO_POST_GOLPE_MAX    = 1800;  // ms máximos

// --- Calibración ---
const bool USAR_CALIB_GUARDADA      = false; // false = siempre hace la calibración guiada con el muñeco al iniciar
const uint32_t VERSION_CALIB        = 2;
const bool CALIB_LEAN_AL_ARRANCAR   = true;
const bool CALIB_PECHO_AL_ARRANCAR  = true;
const bool CALIB_CUBRIR_AL_ARRANCAR = true;
const unsigned long TIEMPO_ESPERA_CALIB     = 2000; // ms tras pulsar BOOT para tomar la guardia
const unsigned long TIEMPO_ESPERA_RECALIB   = 3000;
const unsigned long TIEMPO_QUIETO_CALIB     = 600;
const unsigned long INICIO_MEDICION_LEAN    = 1200;
const float         GIRO_QUIETO             = 30.0;
const float         RANGO_QUIETO_GRADOS     = 4.0;
// =========================================================================

struct Mano {
  const char* nombre = "";
  TwoWire*    bus    = &Wire;
  int      addr          = 0x68;
  uint8_t  teclaGolpe    = 'd';
  float signoPitch       = 1.0;                    // sentido de "subir" para la cabeza (la calibración de golpes lo aprende)
  float umbralCabeza     = ANGULO_CABEZA_GRADOS;   // ° para distinguir cabeza de cuerpo (calibrado por mano)
  bool  golpesCalibrados = false;                  // true si se midieron golpes de esta mano (usa la guardia como referencia)
  bool  golpesSeparados  = false;                  // true si cabeza y cuerpo se distinguen bien con esta mano
  unsigned long bloqueoPostGolpeMs = 700;          // el esquive se bloquea este tiempo tras un golpe de esta mano
  int   ejeEsq           = 0;
  float signoEsquive     = 1.0;
  float amplitud         = 20.0;
  bool  respondeLean     = true;
  int   accionPecho      = ACCION_NINGUNA;
  int   ejePecho         = 1;
  float signoPecho       = -1.0;
  float amplitudPecho    = 40.0;
  float umbralPecho      = UMBRAL_PECHO_NORM;      // margen del pecho de esta mano (la calibración lo ajusta)
  bool  respondePecho    = true;
  float cvP              = 0;
  float cvR              = 0;
  bool  respondeCubrir   = false;

  // estado
  float pitch = 0, roll = 0, pitch_base = 0, roll_base = 0;
  bool  calibrado          = false;
  bool  golpeEnCurso       = false;
  bool  golpePendiente     = false;
  bool  golpeCalib         = false;     // el golpe en curso es de calibración (no manda teclas)
  float elevacionMaxGrados = -90.0;
  float ayInicio           = 0;
  float esquiveGrados      = 0;
  float esq_ref_p = 0, esq_ref_r = 0;
  float esq_suma_p = 0, esq_suma_r = 0;
  float esq_minP = 0, esq_maxP = 0, esq_minR = 0, esq_maxR = 0;
  int   esq_n = 0;
  bool  esq_listo = false;
  unsigned long esq_t0 = 0;
  float giroSuave = 0, rangoActual = 0;
  float sumRp = 0, sumLp = 0, sumRr = 0, sumLr = 0;
  int   nR = 0, nL = 0;
  float leanRp = 0, leanRr = 0, leanLp = 0, leanLr = 0;
  float sumCp = 0, sumCr = 0;
  int   nC = 0;
  float sumVp = 0, sumVr = 0;
  int   nV = 0;
  float cubrirNorm = 0;
  bool  enPecho    = false;
  bool  pechoCrudo = false;
  bool  latchPecho = false;
  bool  ocupada    = false;
  // calibración de golpes
  float cbMax[GOLPES_MAX], cbMin[GOLPES_MAX];   // elevación máx/mín de cada golpe al cuerpo
  float chMax[GOLPES_MAX], chMin[GOLPES_MAX];   // ... y a la cabeza
  int   nCB = 0, nCH = 0;
  float elevMaxRaw = 0, elevMinRaw = 0, fuerzaPico = 0;
  bool  asentando = false;                       // midiendo cuánto tarda la mano en quedarse quieta tras un golpe
  unsigned long tQuieto = 0, tInicioRetorno = 0, tUltimoGolpeCalib = 0;
  float retornoMaxMs = 0;
  float sumSway = 0;  int nSway = 0;             // cuánto se mueve esta mano mientras la OTRA hace el gesto de pecho
  uint8_t teclaActiva = 0;
  bool    modActiva   = false;
  unsigned long liberarEn = 0;
  unsigned long dtAnterior = 0, tiempoUltimoGolpe = 0, cooldownEstrella = 0;
  unsigned long tiempoInicioPecho = 0, ultimoReporte = 0, inicioVentana = 0;
  unsigned long lecturasOK = 0, erroresI2C = 0, ultimaLecturaOk = 0, ultimoDiag = 0;
};

Mano manoDer, manoIzq;

// Márgenes que calcula la calibración (empiezan en los valores mínimos)
float minPorManoEf   = MIN_POR_MANO_NORM;
float umbralCubrirEf = UMBRAL_CUBRIR_NORM;
bool  hayConflicto   = false;

bool esquiveEnCurso = false;
int  ladoEsquive = 0;
int  esquivePendiente = 0;
bool esqIniciado = false;
int  leanFase = 0;   // 0 inactivo | 1 guardia | 2-4 inclinación | 5-8 pecho | 9-10 cubrirse | 11-14 golpes
unsigned long tFaseLean = 0;
unsigned long bloqueaGestosHasta  = 0;
unsigned long bloqueaEsquiveHasta = 0;
unsigned long bloqueaGolpesHasta  = 0;
unsigned long esquiveInicio = 0, inicioEsquive = 0, ultimoEsquive = 0, ultimoDebug = 0, ultimoReporteEsq = 0;
unsigned long inicioCalibEsq = TIEMPO_ESPERA_CALIB;

bool levantarseActivo = false;
bool rafagaPresionada = false;
unsigned long inicioLevantarse = 0, tPresionRafaga = 0;

bool cubrirseActivo = false;
bool teclaCubrirPresionada = false;
unsigned long inicioCubrir = 0, liberarCubrirEn = 0, ultimoReporteCubrir = 0;

uint8_t teclaEsquiveActiva = 0;
unsigned long liberarEsquiveEn = 0;
unsigned long liberarEstrellaEn = 0;

int  pitidosPendientes = 0;
bool pitidoEncendido   = false;
unsigned long tPitido = 0, duracionPitido = 120;
const unsigned long PAUSA_PITIDO = 120;
unsigned long ultimoAvisoSensor = 0;
bool guardiaAnunciada = false;

enum CodigoPC {
  PC_ENCENDIDO = 0,
  PC_ERROR_SENSOR,
  PC_TOMANDO_GUARDIA,
  PC_GUARDIA_LISTA,
  PC_INCLINA_DER,
  PC_VUELVE_GUARDIA,
  PC_INCLINA_IZQ,
  PC_PECHO_DER,
  PC_PECHO_IZQ,
  PC_CUBRETE,
  PC_CALIB_OK,
  PC_CALIB_AVISO,
  PC_GOLPES_CUERPO_DER,   // 12  (Ctrl+Alt+Shift+F1)
  PC_GOLPES_CABEZA_DER,   // 13  (F2)
  PC_GOLPES_CUERPO_IZQ,   // 14  (F3)
  PC_GOLPES_CABEZA_IZQ,   // 15  (F4)
  PC_GOLPE_REGISTRADO     // 16  (F5)
};
const int NUM_CODIGOS_PC = 17;

// PrtSc, ScrLk, Pause, Insert, Home, PgUp, End, PgDn, Num/, Num*, Num-, Num+, F1, F2, F3, F4, F5
const uint8_t TECLAS_PC[17] = { 0xCE, 0xCF, 0xD0, 0xD1, 0xD2, 0xD3, 0xD5, 0xD6, 0xDC, 0xDD, 0xDE, 0xDF,
                                0xC2, 0xC3, 0xC4, 0xC5, 0xC6 };

const int COLA_PC_TAM = 8;
uint8_t colaPC[COLA_PC_TAM];
int  colaPCIni = 0, colaPCN = 0;
bool teclaPCPresionada = false;
uint8_t teclaPCActual = 0;
unsigned long tTeclaPC = 0;

void pitar(int n, unsigned long duracion) {
  pitidosPendientes = n;
  duracionPitido    = duracion;
  pitidoEncendido   = false;
  tPitido           = 0;
  if (PIN_BUZZER >= 0) digitalWrite(PIN_BUZZER, LOW);
}

void actualizarPitidos() {
  if (PIN_BUZZER < 0) return;
  unsigned long ahora = millis();
  if (pitidoEncendido) {
    if (ahora - tPitido >= duracionPitido) {
      digitalWrite(PIN_BUZZER, LOW);
      pitidoEncendido = false;
      tPitido = ahora;
      pitidosPendientes--;
    }
  } else if (pitidosPendientes > 0 && ahora - tPitido >= PAUSA_PITIDO) {
    digitalWrite(PIN_BUZZER, HIGH);
    pitidoEncendido = true;
    tPitido = ahora;
  }
}

void avisarPC(int codigo) {
  if (!USAR_PANTALLA_PC || codigo < 0 || codigo >= NUM_CODIGOS_PC) return;
  if (colaPCN >= COLA_PC_TAM) {
    colaPCIni = (colaPCIni + 1) % COLA_PC_TAM;
    colaPCN--;
  }
  colaPC[(colaPCIni + colaPCN) % COLA_PC_TAM] = (uint8_t)codigo;
  colaPCN++;
}

void actualizarPantallaPC() {
  if (!USAR_PANTALLA_PC) return;
  unsigned long ahora = millis();

  if (teclaPCPresionada) {
    if (ahora - tTeclaPC >= 50) {
      bleKeyboard.release(teclaPCActual);
      bleKeyboard.release(KEY_LEFT_SHIFT);
      bleKeyboard.release(KEY_LEFT_ALT);
      bleKeyboard.release(KEY_LEFT_CTRL);
      teclaPCPresionada = false;
      tTeclaPC = ahora;
    }
    return;
  }

  if (colaPCN > 0 && bleKeyboard.isConnected() && ahora - tTeclaPC >= 250) {
    uint8_t codigo = colaPC[colaPCIni];
    colaPCIni = (colaPCIni + 1) % COLA_PC_TAM;
    colaPCN--;
    teclaPCActual = TECLAS_PC[codigo];
    bleKeyboard.press(KEY_LEFT_CTRL);
    bleKeyboard.press(KEY_LEFT_ALT);
    bleKeyboard.press(KEY_LEFT_SHIFT);
    bleKeyboard.press(teclaPCActual);
    teclaPCPresionada = true;
    tTeclaPC = ahora;
  }
}

void avisar(int pitidos, unsigned long duracion, int codigoPC) {
  if (pitidos > 0) pitar(pitidos, duracion);
  if (codigoPC >= 0) avisarPC(codigoPC);
}

void actualizarLED() {
  if (PIN_LED < 0) return;
  unsigned long t = millis();
  bool calibrando = (leanFase != 0) || !manoDer.esq_listo || !manoIzq.esq_listo;
  bool encendido;
  if (calibrando)                       encendido = ((t / 150) % 2) == 0;
  else if (!bleKeyboard.isConnected())  encendido = (t % 1000) < 100;
  else                                  encendido = true;
  digitalWrite(PIN_LED, encendido ? HIGH : LOW);
}

void bloquearEsquive(unsigned long ms) {
  unsigned long t = millis() + ms;
  if (t > bloqueaEsquiveHasta) bloqueaEsquiveHasta = t;
}

void enviarGolpe(Mano &m, bool cabeza) {
  if (!bleKeyboard.isConnected()) return;
  if (cabeza) bleKeyboard.press(TECLA_MODIFICADOR_CABEZA);
  bleKeyboard.press(m.teclaGolpe);
  m.teclaActiva = m.teclaGolpe;
  m.modActiva   = cabeza;
  m.liberarEn   = millis() + DURACION_TECLA;
}

void soltarGolpe(Mano &m) {
  if (m.liberarEn != 0 && millis() >= m.liberarEn) {
    bleKeyboard.release(m.teclaActiva);
    if (m.modActiva) bleKeyboard.release(TECLA_MODIFICADOR_CABEZA);
    m.liberarEn = 0;
    m.modActiva = false;
  }
}

void enviarEstrella() {
  if (!bleKeyboard.isConnected()) return;
  bleKeyboard.press(TECLA_ESTRELLA);
  liberarEstrellaEn = millis() + 45;
}

void presionarEsquive(uint8_t tecla) {
  if (!bleKeyboard.isConnected()) return;
  bleKeyboard.press(tecla);
  teclaEsquiveActiva = tecla;
  liberarEsquiveEn = ESQUIVE_MANTENER_TECLA ? 0 : millis() + DURACION_ESQUIVE;
}

void soltarEsquive() {
  if (teclaEsquiveActiva != 0) {
    bleKeyboard.release(teclaEsquiveActiva);
    teclaEsquiveActiva = 0;
  }
  liberarEsquiveEn = 0;
}

void soltarRafaga() {
  if (rafagaPresionada) {
    bleKeyboard.release(TECLA_LEVANTARSE);
    bleKeyboard.release(TECLA_MODIFICADOR_CABEZA);
    rafagaPresionada = false;
  }
}

void iniciarLevantarse(const char* nombre) {
  levantarseActivo = true;
  inicioLevantarse = millis();
  tPresionRafaga   = inicioLevantarse - PERIODO_RAFAGA_MS;
  Serial.printf("🆙 [%s] LEVANTARSE: ráfaga activa\n", nombre);
}

void terminarLevantarse(const char* motivo) {
  levantarseActivo = false;
  soltarRafaga();
  bloquearEsquive(BLOQUEO_POST_PECHO_MS);
  Serial.printf("↩️  Levantarse terminado (%s)\n", motivo);
}

void actualizarRafaga() {
  if (!levantarseActivo) {
    soltarRafaga();
    return;
  }
  unsigned long ahora = millis();
  if (ahora - inicioLevantarse > MAX_LEVANTARSE_MS) {
    terminarLevantarse("tiempo máximo");
    return;
  }
  if (rafagaPresionada) {
    if (ahora - tPresionRafaga >= DURACION_RAFAGA_MS) soltarRafaga();
  } else if (ahora - tPresionRafaga >= PERIODO_RAFAGA_MS) {
    if (!LEVANTARSE_SOLO_MENSAJE && bleKeyboard.isConnected()) {
      bleKeyboard.press(TECLA_MODIFICADOR_CABEZA);
      bleKeyboard.press(TECLA_LEVANTARSE);
      rafagaPresionada = true;
    }
    tPresionRafaga = ahora;
  }
}

void presionarCubrir() {
  if (CUBRIRSE_SOLO_MENSAJE || !bleKeyboard.isConnected()) return;
  bleKeyboard.press(TECLA_CUBRIRSE);
  teclaCubrirPresionada = true;
  liberarCubrirEn = CUBRIRSE_MANTENER_TECLA ? 0 : millis() + DURACION_TOQUE_CUBRIR;
}

void soltarCubrir() {
  if (teclaCubrirPresionada) {
    bleKeyboard.release(TECLA_CUBRIRSE);
    teclaCubrirPresionada = false;
  }
  liberarCubrirEn = 0;
}

void iniciarCubrirse(float a, float b) {
  unsigned long ahora = millis();
  cubrirseActivo      = true;
  inicioCubrir        = ahora;
  ultimoReporteCubrir = ahora;
  presionarCubrir();
  Serial.printf("🛡️  [CUBRIRSE] -> DER: %.0f%% | IZQ: %.0f%%\n", a * 100.0, b * 100.0);
}

void terminarCubrirse(const char* motivo) {
  cubrirseActivo = false;
  soltarCubrir();
  bloquearEsquive(BLOQUEO_POST_PECHO_MS);
  Serial.printf("↩️  [GUARDIA] Cubrirse terminado (%s)\n", motivo);
}

int16_t leer16(TwoWire &bus) {
  int16_t hi = bus.read();
  int16_t lo = bus.read();
  return (hi << 8) | lo;
}

int leerWhoAmI(TwoWire &bus, int addr) {
  bus.beginTransmission(addr);
  bus.write(0x75);
  if (bus.endTransmission(false) != 0) return -1;
  bus.requestFrom((uint8_t)addr, (size_t)1, true);
  if (bus.available() < 1) return -1;
  return bus.read();
}

bool iniciarMPU(TwoWire &bus, int addr, const char* nombre) {
  bool ok = false;
  for (int i = 0; i < 5 && !ok; i++) {
    bus.beginTransmission(addr);
    bus.write(0x6B);
    bus.write(0);
    ok = (bus.endTransmission() == 0);
    if (!ok) delay(100);
  }
  if (ok) {
    Serial.printf("MPU %s (0x%02X): OK | WHO_AM_I = 0x%02X\n", nombre, addr, leerWhoAmI(bus, addr));
  } else {
    Serial.printf("MPU %s (0x%02X): NO ENCONTRADO\n", nombre, addr);
  }
  return ok;
}

void diagnosticoMano(Mano &m) {
  unsigned long ahora = millis();
  if (ahora - m.ultimoDiag < 1000) return;
  m.ultimoDiag = ahora;

  if (ahora - m.ultimaLecturaOk > 1500) {
    Serial.printf("⚠️  [%s] SIN LECTURAS del sensor (OK: %lu | errores I2C: %lu)\n",
                  m.nombre, m.lecturasOK, m.erroresI2C);
    if (ahora - ultimoAvisoSensor > 5000) {
      ultimoAvisoSensor = ahora;
      avisar(4, 80, PC_ERROR_SENSOR);
    }
    m.bus->beginTransmission(m.addr);
    m.bus->write(0x6B);
    m.bus->write(0);
    m.bus->endTransmission();
    return;
  }

  if (!m.esq_listo) {
    if (ahora <= inicioCalibEsq) {
      Serial.printf("⏳ [%s] Ponte en GUARDIA y no te muevas... faltan %lu s\n", m.nombre, (inicioCalibEsq - ahora) / 1000 + 1);
    } else {
      Serial.printf("⏳ [%s] Esperando quietud -> giro: %.0f°/s | variación: %.1f°\n",
                    m.nombre, m.giroSuave, m.rangoActual);
    }
  }
}

void iniciarEsquive(int dir, float na, float nb, float pos) {
  unsigned long ahora = millis();
  esquiveEnCurso   = true;
  ladoEsquive      = dir;
  inicioEsquive    = ahora;
  ultimoReporteEsq = ahora;
  esquivePendiente = 0;
  bloqueaGestosHasta = ahora + BLOQUEO_GESTOS_MS;
  if (!ESQUIVE_SOLO_MENSAJE) presionarEsquive(dir > 0 ? TECLA_ESQUIVE_DER : TECLA_ESQUIVE_IZQ);
  Serial.printf("🌀 [ESQUIVE %s] -> DER: %.2f | IZQ: %.2f | posición: %.2f\n",
                dir > 0 ? "DERECHA" : "IZQUIERDA", na, nb, pos);
}

void terminarEsquive(const char* motivo) {
  esquiveEnCurso = false;
  ladoEsquive    = 0;
  ultimoEsquive  = millis();
  esqIniciado    = false;
  soltarEsquive();
  Serial.printf("↩️  [GUARDIA] Esquive terminado (%s)\n", motivo);
}

void reiniciarRefEsquive(Mano &m) {
  m.esq_listo     = false;
  m.esq_suma_p    = 0;
  m.esq_suma_r    = 0;
  m.esq_n         = 0;
  m.esquiveGrados = 0;
}

void recalibrarEsquive() {
  reiniciarRefEsquive(manoDer);
  reiniciarRefEsquive(manoIzq);
  esquiveEnCurso   = false;
  ladoEsquive      = 0;
  esquivePendiente = 0;
  esqIniciado      = false;
  leanFase         = 0;
  guardiaAnunciada = false;
  soltarEsquive();
  if (levantarseActivo) terminarLevantarse("recalibración");
  if (cubrirseActivo)   terminarCubrirse("recalibración");
  inicioCalibEsq   = millis() + TIEMPO_ESPERA_RECALIB;
  avisar(1, 400, PC_TOMANDO_GUARDIA);
  Serial.println("\n🔄 Tomando guardia en 3 s: ponte en GUARDIA y quédate quieto...\n");
}

void iniciarCalibLean() {
  recalibrarEsquive();
  leanFase = 1;
}

void calcularLean(Mano &m) {
  float mRp = (m.nR > 0) ? m.sumRp / m.nR : 0;
  float mLp = (m.nL > 0) ? m.sumLp / m.nL : 0;
  float mRr = (m.nR > 0) ? m.sumRr / m.nR : 0;
  float mLr = (m.nL > 0) ? m.sumLr / m.nL : 0;

  m.leanRp = mRp;  m.leanRr = mRr;
  m.leanLp = mLp;  m.leanLr = mLr;

  float spanP = mRp - mLp;
  float spanR = mRr - mLr;

  int eje = EJE_ESQUIVE;
  if (eje < 0) eje = (fabs(spanP) >= fabs(spanR)) ? 0 : 1;
  float span = (eje == 0) ? spanP : spanR;
  float amp  = fabs(span) / 2.0;

  m.ejeEsq       = eje;
  m.respondeLean = (amp >= AMPLITUD_MINIMA);
  if (m.respondeLean) {
    m.signoEsquive = (span >= 0) ? 1.0 : -1.0;
    m.amplitud     = amp;
  }
}

void calcularPecho(Mano &m) {
  float mp = (m.nC > 0) ? m.sumCp / m.nC : 0;
  float mr = (m.nC > 0) ? m.sumCr / m.nC : 0;

  int   eje = (fabs(mp) >= fabs(mr)) ? 0 : 1;
  float v   = (eje == 0) ? mp : mr;

  m.ejePecho      = eje;
  m.respondePecho = (fabs(v) >= AMPLITUD_MINIMA_PECHO);
  if (m.respondePecho) {
    m.signoPecho    = (v >= 0) ? 1.0 : -1.0;
    m.amplitudPecho = fabs(v);
  }
}

void calcularCubrir(Mano &m) {
  float vp  = (m.nV > 0) ? m.sumVp / m.nV : 0;
  float vr  = (m.nV > 0) ? m.sumVr / m.nV : 0;
  float mag = sqrt(vp * vp + vr * vr);

  m.cvP = vp;
  m.cvR = vr;
  m.respondeCubrir = (mag >= AMPLITUD_MINIMA_CUBRIR);
}

// Qué tanto se parece un movimiento (dP, dR) a tu postura de cubrirte (0 si la dirección no coincide)
float normCubrir(Mano &m, float dP, float dR) {
  float mag    = sqrt(dP * dP + dR * dR);
  float cvMag2 = m.cvP * m.cvP + m.cvR * m.cvR;
  float punto  = dP * m.cvP + dR * m.cvR;
  float coseno = (mag > 0.5 && cvMag2 > 0.001) ? punto / (mag * sqrt(cvMag2)) : 0;
  return (coseno > PARECIDO_MIN_CUBRIR) ? constrain(punto / cvMag2, 0.0, 1.5) : 0;
}

// ---------------------------------------------------------------------
// 🥊 CALIBRACIÓN DE GOLPES
// ---------------------------------------------------------------------
// Guarda un golpe de calibración (no manda ninguna tecla)
void registrarGolpeCalibracion(Mano &m, bool cuerpo) {
  int &n = cuerpo ? m.nCB : m.nCH;
  m.tUltimoGolpeCalib = millis();
  m.asentando         = true;       // ahora mide cuánto tarda la mano en quedarse quieta
  m.tQuieto           = 0;
  m.tInicioRetorno    = m.inicioVentana;
  if (n >= GOLPES_POR_FASE || n >= GOLPES_MAX) return;   // ya hay suficientes

  float* mx = cuerpo ? m.cbMax : m.chMax;
  float* mn = cuerpo ? m.cbMin : m.chMin;
  mx[n] = m.elevMaxRaw;
  mn[n] = m.elevMinRaw;
  n++;
  avisarPC(PC_GOLPE_REGISTRADO);
  Serial.printf("   ✓ [%s] golpe %s %d/%d | fuerza %d | elevación %.1f° a %.1f°\n",
                m.nombre, cuerpo ? "cuerpo" : "cabeza", n, GOLPES_POR_FASE,
                (int)m.fuerzaPico, m.elevMinRaw, m.elevMaxRaw);
}

void calcularGolpes(Mano &m) {
  m.asentando = false;
  if (m.nCB < 2 || m.nCH < 2) {
    m.golpesCalibrados = false;
    m.golpesSeparados  = false;
    m.umbralCabeza     = ANGULO_CABEZA_GRADOS;
    hayConflicto       = true;
    Serial.printf("   ⚠️ [%s] golpes: faltaron golpes (cuerpo %d, cabeza %d). Se usa el ángulo fijo de %.1f°\n",
                  m.nombre, m.nCB, m.nCH, ANGULO_CABEZA_GRADOS);
    return;
  }

  // Con signo +: la cabeza se reconoce por el MÁXIMO de elevación. Con signo -: por el MÍNIMO (con el signo cambiado).
  float sumBP = 0, sumHP = 0, sumBM = 0, sumHM = 0;
  float bMaxP = -1e9f, bMaxM = -1e9f, hMinP = 1e9f, hMinM = 1e9f;
  for (int i = 0; i < m.nCB; i++) {
    sumBP += m.cbMax[i];    bMaxP = fmax(bMaxP, m.cbMax[i]);
    sumBM += -m.cbMin[i];   bMaxM = fmax(bMaxM, -m.cbMin[i]);
  }
  for (int j = 0; j < m.nCH; j++) {
    sumHP += m.chMax[j];    hMinP = fmin(hMinP, m.chMax[j]);
    sumHM += -m.chMin[j];   hMinM = fmin(hMinM, -m.chMin[j]);
  }
  float mediaBP = sumBP / m.nCB, mediaHP = sumHP / m.nCH;
  float mediaBM = sumBM / m.nCB, mediaHM = sumHM / m.nCH;

  bool  plus   = ((mediaHP - mediaBP) >= (mediaHM - mediaBM));
  float mediaB = plus ? mediaBP : mediaBM;
  float mediaH = plus ? mediaHP : mediaHM;
  float bMax   = plus ? bMaxP : bMaxM;     // el golpe al cuerpo que más subió
  float hMin   = plus ? hMinP : hMinM;     // el golpe a la cabeza que menos subió
  float limpio = hMin - bMax;              // > 0: ningún golpe al cuerpo llega tan alto como el más bajo a la cabeza

  m.signoPitch       = plus ? 1.0 : -1.0;
  m.umbralCabeza     = (limpio > 0) ? (bMax + hMin) / 2.0 : (mediaB + mediaH) / 2.0;
  m.golpesCalibrados = true;
  m.golpesSeparados  = ((mediaH - mediaB) >= SEPARACION_MIN_CABEZA);

  // Cuánto se bloquea el esquive tras un golpe: 1.2 veces lo que tardó la mano en quedarse quieta
  float bloqueo = (m.retornoMaxMs > 0) ? m.retornoMaxMs * 1.2f : 700.0f;
  m.bloqueoPostGolpeMs = (unsigned long)constrain(bloqueo, BLOQUEO_POST_GOLPE_MIN, BLOQUEO_POST_GOLPE_MAX);

  Serial.printf("   [%s] golpes: cuerpo %.1f° | cabeza %.1f° | signo %+.0f | umbral cabeza %.1f° | hueco entre grupos %.1f° | regreso máx %.0f ms -> bloqueo esquive %lu ms\n",
                m.nombre, mediaB, mediaH, m.signoPitch, m.umbralCabeza, limpio, m.retornoMaxMs, m.bloqueoPostGolpeMs);
  if (!m.golpesSeparados) {
    hayConflicto = true;
    Serial.printf("   ⚠️ [%s] cabeza y cuerpo se distinguen poco (menos de %.0f°). Sube más la mano en los golpes a la cabeza.\n",
                  m.nombre, SEPARACION_MIN_CABEZA);
  } else if (limpio <= 0) {
    Serial.printf("   ℹ️ [%s] algún golpe al cuerpo subió casi tanto como uno a la cabeza: puede haber confusiones ocasionales.\n", m.nombre);
  }
}

// ---------------------------------------------------------------------
// 📐 MÁRGENES ENTRE GESTOS (se calculan con lo medido en la calibración)
// ---------------------------------------------------------------------
void calcularMargenes() {
  Mano* manos[2] = { &manoDer, &manoIzq };

  // 1) Cuánto se mueve la mano QUIETA (en el eje del esquive) mientras la otra hace el gesto de pecho
  float sway = 0;
  for (int i = 0; i < 2; i++) {
    if (manos[i]->nSway > 0) sway = fmax(sway, manos[i]->sumSway / manos[i]->nSway);
  }
  float necesarioMin = 2.0f * sway + 0.10f;
  minPorManoEf = constrain(fmax(MIN_POR_MANO_NORM, necesarioMin), MIN_POR_MANO_NORM, 0.80f);
  Serial.printf("   Margen esquive: la mano quieta se mueve %.2f mientras haces el pecho -> cada mano debe pasar de %.2f para contar\n",
                sway, minPorManoEf);
  if (necesarioMin > 0.80f) {
    hayConflicto = true;
    Serial.println("   ⚠️ Al hacer el pecho la otra mano se mueve demasiado: puede salir esquive. Procura mantener la otra mano quieta.");
  }

  // 2) Margen del pecho de cada mano: qué tanto activaría el pecho una inclinación completa
  for (int i = 0; i < 2; i++) {
    Mano &m = *manos[i];
    if (m.accionPecho == ACCION_NINGUNA || !m.respondePecho || !m.respondeLean) continue;
    float rEje = (m.ejePecho == 0) ? m.leanRp : m.leanRr;
    float lEje = (m.ejePecho == 0) ? m.leanLp : m.leanLr;
    float cruce = fmax(0.0f, fmax(m.signoPecho * rEje, m.signoPecho * lEje)) / m.amplitudPecho;
    float necesario = 1.3f * cruce + 0.10f;
    m.umbralPecho = constrain(fmax(UMBRAL_PECHO_NORM, necesario), UMBRAL_PECHO_NORM, 0.90f);
    Serial.printf("   Margen pecho [%s]: una inclinación completa activa el pecho al %.0f%% -> umbral %.2f\n",
                  m.nombre, cruce * 100.0f, m.umbralPecho);
    if (necesario > CONFLICTO_MAX) {
      hayConflicto = true;
      Serial.printf("   ⚠️ [%s] inclinarte se parece demasiado al gesto de pecho. Prueba llevar la mano al pecho de otra forma.\n", m.nombre);
    }
  }

  // 3) Margen de cubrirse: cuánto lo activarían las inclinaciones y el gesto de pecho
  if (manoDer.respondeCubrir && manoIzq.respondeCubrir) {
    float cruce = 0;
    for (int i = 0; i < 2; i++) {
      Mano &m = *manos[i];
      float cp = (m.nC > 0) ? m.sumCp / m.nC : 0.0f;
      float cr = (m.nC > 0) ? m.sumCr / m.nC : 0.0f;
      cruce = fmax(cruce, normCubrir(m, m.leanRp, m.leanRr));
      cruce = fmax(cruce, normCubrir(m, m.leanLp, m.leanLr));
      cruce = fmax(cruce, normCubrir(m, cp, cr));
    }
    float necesario = 1.3f * cruce + 0.10f;
    umbralCubrirEf = constrain(fmax(UMBRAL_CUBRIR_NORM, necesario), UMBRAL_CUBRIR_NORM, 0.90f);
    Serial.printf("   Margen cubrirse: otros gestos lo activan al %.0f%% -> umbral %.2f\n", cruce * 100.0f, umbralCubrirEf);
    if (necesario > CONFLICTO_MAX) {
      hayConflicto = true;
      Serial.println("   ⚠️ Cubrirte se parece demasiado a otro gesto. Prueba una postura más distinta.");
    }
  }
}

// ---------------------------------------------------------------------
// 💾 GUARDAR / CARGAR LA CALIBRACIÓN
// ---------------------------------------------------------------------
const char* clave(const char* prefijo, const char* nombre) {
  static char buf[20];
  snprintf(buf, sizeof(buf), "%s%s", prefijo, nombre);
  return buf;
}

void guardarManoEnFlash(Mano &m, const char* p) {
  prefs.putInt  (clave(p, "ejeEsq"),   m.ejeEsq);
  prefs.putFloat(clave(p, "sgnEsq"),   m.signoEsquive);
  prefs.putFloat(clave(p, "amp"),      m.amplitud);
  prefs.putBool (clave(p, "respLean"), m.respondeLean);
  prefs.putInt  (clave(p, "ejePec"),   m.ejePecho);
  prefs.putFloat(clave(p, "sgnPec"),   m.signoPecho);
  prefs.putFloat(clave(p, "ampPec"),   m.amplitudPecho);
  prefs.putBool (clave(p, "respPec"),  m.respondePecho);
  prefs.putFloat(clave(p, "cvP"),      m.cvP);
  prefs.putFloat(clave(p, "cvR"),      m.cvR);
  prefs.putBool (clave(p, "respCub"),  m.respondeCubrir);
  prefs.putFloat(clave(p, "sgnPit"),   m.signoPitch);
  prefs.putFloat(clave(p, "umbCab"),   m.umbralCabeza);
  prefs.putBool (clave(p, "golCal"),   m.golpesCalibrados);
  prefs.putFloat(clave(p, "umbPec"),   m.umbralPecho);
  prefs.putUInt (clave(p, "bloqPG"),   (uint32_t)m.bloqueoPostGolpeMs);
}

void cargarManoDeFlash(Mano &m, const char* p) {
  m.ejeEsq         = prefs.getInt  (clave(p, "ejeEsq"),   m.ejeEsq);
  m.signoEsquive   = prefs.getFloat(clave(p, "sgnEsq"),   m.signoEsquive);
  m.amplitud       = prefs.getFloat(clave(p, "amp"),      m.amplitud);
  m.respondeLean   = prefs.getBool (clave(p, "respLean"), m.respondeLean);
  m.ejePecho       = prefs.getInt  (clave(p, "ejePec"),   m.ejePecho);
  m.signoPecho     = prefs.getFloat(clave(p, "sgnPec"),   m.signoPecho);
  m.amplitudPecho  = prefs.getFloat(clave(p, "ampPec"),   m.amplitudPecho);
  m.respondePecho  = prefs.getBool (clave(p, "respPec"),  m.respondePecho);
  m.cvP            = prefs.getFloat(clave(p, "cvP"),      m.cvP);
  m.cvR            = prefs.getFloat(clave(p, "cvR"),      m.cvR);
  m.respondeCubrir = prefs.getBool (clave(p, "respCub"),  m.respondeCubrir);
  m.signoPitch       = prefs.getFloat(clave(p, "sgnPit"), m.signoPitch);
  m.umbralCabeza     = prefs.getFloat(clave(p, "umbCab"), m.umbralCabeza);
  m.golpesCalibrados = prefs.getBool (clave(p, "golCal"), m.golpesCalibrados);
  m.umbralPecho      = prefs.getFloat(clave(p, "umbPec"), m.umbralPecho);
  m.bloqueoPostGolpeMs = prefs.getUInt(clave(p, "bloqPG"), (uint32_t)m.bloqueoPostGolpeMs);
}

void guardarCalibracion() {
  if (!prefs.begin("littlemac", false)) return;
  prefs.putUInt("version", VERSION_CALIB);
  guardarManoEnFlash(manoDer, "d_");
  guardarManoEnFlash(manoIzq, "i_");
  prefs.putFloat("minMano", minPorManoEf);
  prefs.putFloat("umbCubr", umbralCubrirEf);
  prefs.end();
  Serial.println("💾 Calibración guardada en la memoria del ESP");
}

bool cargarCalibracion() {
  if (!prefs.begin("littlemac", true)) return false;
  bool ok = (prefs.getUInt("version", 0) == VERSION_CALIB);
  if (ok) {
    cargarManoDeFlash(manoDer, "d_");
    cargarManoDeFlash(manoIzq, "i_");
    minPorManoEf   = prefs.getFloat("minMano", minPorManoEf);
    umbralCubrirEf = prefs.getFloat("umbCubr", umbralCubrirEf);
  }
  prefs.end();
  return ok;
}

void finalizarCalibracion() {
  guardarCalibracion();
  bool todoOk = manoDer.respondeLean && manoIzq.respondeLean &&
                manoDer.respondePecho && manoIzq.respondePecho &&
                (!CALIB_CUBRIR_AL_ARRANCAR || (manoDer.respondeCubrir && manoIzq.respondeCubrir)) &&
                !hayConflicto;
  avisar(3, 120, todoOk ? PC_CALIB_OK : PC_CALIB_AVISO);
  Serial.println(todoOk ? "\n✅ Calibración lista\n" : "\n⚠️ Calibración terminada con avisos (revisa los mensajes de arriba)\n");

  leanFase    = 0;
  esqIniciado = false;
  if (esquiveEnCurso) terminarEsquive("calibración");
}

// Calcula golpes y márgenes con todo lo medido y termina la calibración
void calcularTodoYFinalizar() {
  hayConflicto = false;
  if (CALIB_GOLPES_AL_ARRANCAR) {
    Serial.println("\n✅ Calibración de golpes:");
    calcularGolpes(manoDer);
    calcularGolpes(manoIzq);
  }
  Serial.println("\n📐 Márgenes entre gestos:");
  calcularMargenes();
  finalizarCalibracion();
}

// Empieza uno de los 4 pasos de golpes: 11 = cuerpo DER | 12 = cabeza DER | 13 = cuerpo IZQ | 14 = cabeza IZQ
void iniciarFaseGolpes(int fase, unsigned long ahora) {
  leanFase = fase;
  tFaseLean = ahora;
  Mano &mc = (fase <= 12) ? manoDer : manoIzq;
  bool cuerpo = (fase == 11 || fase == 13);
  if (cuerpo) mc.nCB = 0; else mc.nCH = 0;
  mc.asentando = false;
  mc.tQuieto = 0;
  mc.tUltimoGolpeCalib = ahora;
  avisar(1, 400, PC_GOLPES_CUERPO_DER + (fase - 11));
  Serial.printf("\n🥊 Lanza %d golpes %s con la mano %s...\n",
                GOLPES_POR_FASE, cuerpo ? "al CUERPO" : "a la CABEZA", mc.nombre);
}

void continuarConGolpes(unsigned long ahora) {
  if (CALIB_GOLPES_AL_ARRANCAR) iniciarFaseGolpes(11, ahora);
  else calcularTodoYFinalizar();
}

void actualizarCalibLean() {
  if (leanFase == 0) return;
  unsigned long ahora = millis();
  bloqueaGestosHasta = ahora + 300;

  if (leanFase == 1) {
    if (manoDer.esq_listo && manoIzq.esq_listo) {
      manoDer.sumRp = manoDer.sumLp = manoDer.sumRr = manoDer.sumLr = 0;  manoDer.nR = manoDer.nL = 0;
      manoIzq.sumRp = manoIzq.sumLp = manoIzq.sumRr = manoIzq.sumLr = 0;  manoIzq.nR = manoIzq.nL = 0;
      leanFase = 2; tFaseLean = ahora;
      avisar(1, 400, PC_INCLINA_DER);
      Serial.println("\n➡️  Inclínate a la DERECHA y MANTÉN ~3 s...");
    }
  } else if (leanFase == 2 && ahora - tFaseLean > 3500) {
    leanFase = 3; tFaseLean = ahora;
    avisar(1, 400, PC_VUELVE_GUARDIA);
    Serial.println("↩️  Vuelve a GUARDIA...");
  } else if (leanFase == 3 && ahora - tFaseLean > 2500) {
    leanFase = 4; tFaseLean = ahora;
    avisar(1, 400, PC_INCLINA_IZQ);
    Serial.println("⬅️  Inclínate a la IZQUIERDA y MANTÉN ~3 s...");
  } else if (leanFase == 4 && ahora - tFaseLean > 3500) {
    calcularLean(manoDer);
    calcularLean(manoIzq);
    if (CALIB_PECHO_AL_ARRANCAR) {
      leanFase = 5; tFaseLean = ahora;
      avisar(1, 400, PC_VUELVE_GUARDIA);
    } else if (CALIB_CUBRIR_AL_ARRANCAR) {
      leanFase = 9; tFaseLean = ahora;
      avisar(1, 400, PC_VUELVE_GUARDIA);
    } else {
      continuarConGolpes(ahora);
    }
  } else if (leanFase == 5 && ahora - tFaseLean > 2500) {
    manoDer.sumCp = manoDer.sumCr = 0;  manoDer.nC = 0;
    manoIzq.sumSway = 0;  manoIzq.nSway = 0;      // mientras la DERECHA hace el pecho, mide cuánto se mueve la IZQUIERDA
    leanFase = 6; tFaseLean = ahora;
    avisar(1, 400, PC_PECHO_DER);
    Serial.println("\n🖐️  Mano DERECHA al PECHO y MANTÉN ~3 s (la otra mano quieta)...");
  } else if (leanFase == 6 && ahora - tFaseLean > 3500) {
    leanFase = 7; tFaseLean = ahora;
    avisar(1, 400, PC_VUELVE_GUARDIA);
  } else if (leanFase == 7 && ahora - tFaseLean > 2500) {
    manoIzq.sumCp = manoIzq.sumCr = 0;  manoIzq.nC = 0;
    manoDer.sumSway = 0;  manoDer.nSway = 0;      // mientras la IZQUIERDA hace el pecho, mide cuánto se mueve la DERECHA
    leanFase = 8; tFaseLean = ahora;
    avisar(1, 400, PC_PECHO_IZQ);
    Serial.println("🖐️  Mano IZQUIERDA al PECHO y MANTÉN ~3 s (la otra mano quieta)...");
  } else if (leanFase == 8 && ahora - tFaseLean > 3500) {
    calcularPecho(manoDer);
    calcularPecho(manoIzq);
    if (CALIB_CUBRIR_AL_ARRANCAR) {
      leanFase = 9; tFaseLean = ahora;
      avisar(1, 400, PC_VUELVE_GUARDIA);
    } else {
      continuarConGolpes(ahora);
    }
  } else if (leanFase == 9 && ahora - tFaseLean > 2500) {
    manoDer.sumVp = manoDer.sumVr = 0;  manoDer.nV = 0;
    manoIzq.sumVp = manoIzq.sumVr = 0;  manoIzq.nV = 0;
    leanFase = 10; tFaseLean = ahora;
    avisar(1, 400, PC_CUBRETE);
    Serial.println("\n🛡️  CÚBRETE con las DOS manos y MANTÉN ~4 s...");
  } else if (leanFase == 10 && ahora - tFaseLean > DURACION_FASE_CUBRIR) {
    calcularCubrir(manoDer);
    calcularCubrir(manoIzq);
    continuarConGolpes(ahora);
  } else if (leanFase >= 11 && leanFase <= 14) {
    // Pasos de golpes: avanza cuando llegaron todos los golpes y la mano ya se quedó quieta (o por tiempo)
    Mano &mc = (leanFase <= 12) ? manoDer : manoIzq;
    bool cuerpo = (leanFase == 11 || leanFase == 13);
    bool completo = ((cuerpo ? mc.nCB : mc.nCH) >= GOLPES_POR_FASE);
    bool puedeSeguir = completo && !mc.asentando && (ahora - mc.tUltimoGolpeCalib > 800);
    if (puedeSeguir || (ahora - tFaseLean > GOLPES_TIMEOUT_MS)) {
      if (leanFase < 14) iniciarFaseGolpes(leanFase + 1, ahora);
      else               calcularTodoYFinalizar();
    }
  }
}

void procesarMano(Mano &m) {
  unsigned long tiempoActual = micros();
  float dt = (tiempoActual - m.dtAnterior) / 1000000.0;
  m.dtAnterior = tiempoActual;

  TwoWire &bus = *m.bus;

  bus.beginTransmission(m.addr);
  bus.write(0x3B);
  if (bus.endTransmission(false) != 0) { m.erroresI2C++; return; }
  bus.requestFrom((uint8_t)m.addr, (size_t)14, true);
  if (bus.available() < 14) { m.erroresI2C++; return; }

  int16_t raw_ax = leer16(bus);
  int16_t raw_ay = leer16(bus);
  int16_t raw_az = leer16(bus);
  leer16(bus);
  int16_t raw_gx = leer16(bus);
  int16_t raw_gy = leer16(bus);
  int16_t raw_gz = leer16(bus);

  bool todoCero = (raw_ax == 0 && raw_ay == 0 && raw_az == 0 && raw_gx == 0 && raw_gy == 0 && raw_gz == 0);
  bool todoUno  = (raw_ax == -1 && raw_ay == -1 && raw_az == -1 && raw_gx == -1 && raw_gy == -1 && raw_gz == -1);
  if (todoCero || todoUno) { m.erroresI2C++; return; }

  m.lecturasOK++;
  m.ultimaLecturaOk = millis();

  float ax = (raw_ax / 16384.0) * 9.81;
  float ay = (raw_ay / 16384.0) * 9.81;
  float az = (raw_az / 16384.0) * 9.81;
  float gx = raw_gx / 131.0;
  float gy = raw_gy / 131.0;

  float giroAbs = fmax(fabs(gx), fabs(gy));
  m.giroSuave += constrain(dt / 0.1, 0.0, 1.0) * (giroAbs - m.giroSuave);

  // Calibración de golpes: mide cuánto tarda la mano en volver a quedarse quieta después de un golpe
  if (m.asentando) {
    if (m.giroSuave < GIRO_QUIETO && !m.golpePendiente) {
      if (m.tQuieto == 0) m.tQuieto = millis();
      if (millis() - m.tQuieto > 200) {
        float ms = (float)(m.tQuieto - m.tInicioRetorno);
        if (ms > m.retornoMaxMs) m.retornoMaxMs = ms;
        m.asentando = false;
      }
    } else {
      m.tQuieto = 0;
    }
  }

  float pitch_acc = atan2(-ax, sqrt(ay * ay + az * az)) * 180.0 / M_PI;
  float roll_acc  = atan2(ay, az) * 180.0 / M_PI;

  m.pitch = 0.96 * (m.pitch + gy * dt) + 0.04 * pitch_acc;
  m.roll  = 0.96 * (m.roll  + gx * dt) + 0.04 * roll_acc;

  if (!m.calibrado) {
    m.pitch_base = m.pitch;
    m.roll_base  = m.roll;
    m.calibrado  = true;
  }

  // ¿Esta mano está haciendo ahora los golpes de la calibración?
  bool fasePunch   = (leanFase >= 11 && leanFase <= 14);
  bool manoCalibra = fasePunch && ((leanFase <= 12) ? (&m == &manoDer) : (&m == &manoIzq)) &&
                     (millis() - tFaseLean > GOLPES_INICIO_MS);

  // Elevación de la mano: contra la guardia (si ya se calibraron los golpes) o contra la referencia vieja
  bool  usaGuardia = m.esq_listo && (m.golpesCalibrados || fasePunch);
  float refElev    = usaGuardia ? m.esq_ref_p : m.pitch_base;
  float elevRaw    = m.pitch - refElev;
  float elevGrados = m.signoPitch * elevRaw;

  float ayGolpe = (SIGNO_AVANCE == 0) ? fabs(ay) : SIGNO_AVANCE * ay;
  int fuerza_avance_norm = constrain(map((long)(ayGolpe * 10), 0, 160, 0, 100), 0, 100);

  if (fuerza_avance_norm < UMBRAL_RETORNO_GUARDIA) m.golpeEnCurso = false;

  bool enCandadoRetorno = (millis() - m.tiempoUltimoGolpe < CANDADO_RETORNO_BRAZO);
  m.ocupada = enCandadoRetorno || m.golpePendiente;

  float pechoNorm = 0;
  if (!m.esq_listo) {
    m.esquiveGrados = 0;
    m.cubrirNorm    = 0;
    if (millis() > inicioCalibEsq) {
      float mnP = m.pitch, mxP = m.pitch, mnR = m.roll, mxR = m.roll;
      if (m.esq_n > 0) {
        mnP = fmin(m.esq_minP, m.pitch);  mxP = fmax(m.esq_maxP, m.pitch);
        mnR = fmin(m.esq_minR, m.roll);   mxR = fmax(m.esq_maxR, m.roll);
      }
      m.rangoActual = fmax(mxP - mnP, mxR - mnR);

      if ((m.giroSuave < GIRO_QUIETO) && (m.rangoActual < RANGO_QUIETO_GRADOS)) {
        if (m.esq_n == 0) m.esq_t0 = millis();
        m.esq_minP = mnP;  m.esq_maxP = mxP;
        m.esq_minR = mnR;  m.esq_maxR = mxR;
        m.esq_suma_p += m.pitch;
        m.esq_suma_r += m.roll;
        m.esq_n++;
        if (millis() - m.esq_t0 > TIEMPO_QUIETO_CALIB) {
          m.esq_ref_p = m.esq_suma_p / m.esq_n;
          m.esq_ref_r = m.esq_suma_r / m.esq_n;
          m.esq_listo = true;
          Serial.printf("✅ [%s] Guardia tomada (pitch %.1f° | roll %.1f°)\n", m.nombre, m.esq_ref_p, m.esq_ref_r);
        }
      } else {
        m.esq_suma_p = 0;
        m.esq_suma_r = 0;
        m.esq_n      = 0;
      }
    }
  } else {
    float dP = m.pitch - m.esq_ref_p;
    float dR = m.roll  - m.esq_ref_r;

    float dEsq = (m.ejeEsq == 0) ? dP : dR;
    m.esquiveGrados = m.signoEsquive * dEsq;

    float dPecho = (m.ejePecho == 0) ? dP : dR;
    pechoNorm = constrain(m.signoPecho * dPecho / m.amplitudPecho, -1.5, 1.5);

    m.cubrirNorm = m.respondeCubrir ? normCubrir(m, dP, dR) : 0;

    unsigned long enFase = millis() - tFaseLean;
    if (leanFase == 2 && enFase > INICIO_MEDICION_LEAN) { m.sumRp += dP; m.sumRr += dR; m.nR++; }
    if (leanFase == 4 && enFase > INICIO_MEDICION_LEAN) { m.sumLp += dP; m.sumLr += dR; m.nL++; }
    if (((leanFase == 6 && &m == &manoDer) || (leanFase == 8 && &m == &manoIzq)) && enFase > INICIO_MEDICION_LEAN) {
      m.sumCp += dP; m.sumCr += dR; m.nC++;
    }
    // La mano que se queda quieta mientras la otra hace el gesto de pecho: ¿cuánto se mueve en el eje del esquive?
    if (((leanFase == 6 && &m == &manoIzq) || (leanFase == 8 && &m == &manoDer)) &&
        enFase > INICIO_MEDICION_LEAN && m.respondeLean && m.amplitud > 1.0) {
      m.sumSway += fabs(m.esquiveGrados) / m.amplitud;
      m.nSway++;
    }
    if (leanFase == 10 && enFase > INICIO_MEDICION_LEAN) { m.sumVp += dP; m.sumVr += dR; m.nV++; }

    if (leanFase == 0 && !m.ocupada && !esquiveEnCurso && !levantarseActivo && !cubrirseActivo &&
        fabs(m.esquiveGrados) < 0.2 * m.amplitud && fabs(pechoNorm) < 0.2 && m.cubrirNorm < 0.2) {
      m.esq_ref_p += 0.0005 * dP;
      m.esq_ref_r += 0.0005 * dR;
    }
  }

  bool bloqueaEsquive = (esquiveEnCurso && !ESQUIVE_SOLO_MENSAJE) || (leanFase != 0);
  // Durante la calibración los golpes se bloquean, salvo los de la mano que está calibrando sus golpes
  bool bloqueaGolpes  = (bloqueaEsquive && !manoCalibra) || levantarseActivo || cubrirseActivo || (millis() < bloqueaGolpesHasta);

  Mano &otra = (&m == &manoDer) ? manoIzq : manoDer;
  bool pechoUsable = m.respondePecho && m.esq_listo && (m.accionPecho != ACCION_NINGUNA);

  m.pechoCrudo = pechoUsable && (pechoNorm > m.umbralPecho);
  bool pechoBloqueado = enCandadoRetorno || bloqueaEsquive || cubrirseActivo ||
                        (millis() < bloqueaGestosHasta) || otra.pechoCrudo;

  float umbralPecho  = m.enPecho ? RETORNO_PECHO_NORM : m.umbralPecho;
  bool  manoEnPecho  = pechoUsable && !pechoBloqueado && (pechoNorm > umbralPecho);
  m.enPecho = manoEnPecho;

  if (manoEnPecho) {
    if (m.tiempoInicioPecho == 0) m.tiempoInicioPecho = millis();
    unsigned long carga = (m.accionPecho == ACCION_ESTRELLA) ? TIEMPO_CARGA_ESTRELLA : TIEMPO_CARGA_LEVANTARSE;

    if (millis() - m.tiempoInicioPecho > carga && !m.latchPecho) {
      if (m.accionPecho == ACCION_ESTRELLA && (millis() - m.cooldownEstrella > 600)) {
        m.cooldownEstrella = millis();
        m.latchPecho = true;
        enviarEstrella();
        bloquearEsquive(BLOQUEO_POST_PECHO_MS);
        Serial.printf("\n⭐ [%s] ¡GOLPE ESTRELLA ACTIVADO! ⭐\n", m.nombre);
        m.tiempoInicioPecho = 0;
        return;
      }
      if (m.accionPecho == ACCION_LEVANTARSE) {
        m.latchPecho = true;
        iniciarLevantarse(m.nombre);
      }
    }
  } else {
    m.tiempoInicioPecho = 0;
    if (pechoNorm < RETORNO_PECHO_NORM) m.latchPecho = false;
    if (m.accionPecho == ACCION_LEVANTARSE && levantarseActivo) terminarLevantarse("la mano salió del pecho");
  }

  bool intentaGolpe = (fuerza_avance_norm > UMBRAL_DISPARO_GOLPE);

  if (intentaGolpe && !m.golpeEnCurso && !enCandadoRetorno && !manoEnPecho &&
      !m.golpePendiente && !bloqueaGolpes) {
    m.golpePendiente     = true;
    m.golpeEnCurso       = true;
    m.golpeCalib         = manoCalibra;
    m.inicioVentana      = millis();
    m.elevacionMaxGrados = elevGrados;
    m.elevMaxRaw         = elevRaw;
    m.elevMinRaw         = elevRaw;
    m.fuerzaPico         = fuerza_avance_norm;
    m.ayInicio           = ay;
  }

  if (m.golpePendiente) {
    if (elevGrados > m.elevacionMaxGrados) m.elevacionMaxGrados = elevGrados;
    if (elevRaw > m.elevMaxRaw) m.elevMaxRaw = elevRaw;
    if (elevRaw < m.elevMinRaw) m.elevMinRaw = elevRaw;
    if (fuerza_avance_norm > m.fuerzaPico) m.fuerzaPico = fuerza_avance_norm;

    if (millis() - m.inicioVentana >= VENTANA_DECISION) {
      m.golpePendiente    = false;
      m.tiempoUltimoGolpe = millis();

      if (m.golpeCalib) {
        // Calibrando: se mide el golpe, no se manda ninguna tecla
        registrarGolpeCalibracion(m, leanFase == 11 || leanFase == 13);
      } else {
        bool esGolpeCabeza = (m.elevacionMaxGrados > m.umbralCabeza);
        enviarGolpe(m, esGolpeCabeza);
        bloquearEsquive(m.bloqueoPostGolpeMs);   // el brazo regresando no debe contar como esquive

        Serial.printf("%s [%s] -> Fuerza: %d | ElevMax: %.1f° (umbral %.1f°)\n",
                      esGolpeCabeza ? "🥊 [GOLPE CABEZA]" : "🥊 [GOLPE CUERPO]",
                      m.nombre, fuerza_avance_norm, m.elevacionMaxGrados, m.umbralCabeza);
      }
    }
    return;
  }
}

void detectarEsquive() {
  static float baseA = 0, baseB = 0, sa = 0, sb = 0;
  static unsigned long tAnterior = 0;

  if (leanFase != 0 || !manoDer.esq_listo || !manoIzq.esq_listo) {
    esqIniciado = false;
    return;
  }

  bool vA = manoDer.respondeLean;
  bool vB = manoIzq.respondeLean;
  if (!vA && !vB) return;

  unsigned long ahora = millis();
  float a = manoDer.esquiveGrados;
  float b = manoIzq.esquiveGrados;

  if (!esqIniciado) {
    baseA = a; baseB = b; sa = a; sb = b;
    tAnterior = ahora; esqIniciado = true;
    return;
  }

  float dt    = (ahora - tAnterior) / 1000.0;
  tAnterior   = ahora;
  float kBase = constrain(dt / TAU_BASE_ESQUIVE, 0.0, 1.0);
  float kSuav = constrain(dt / TAU_SUAVIZADO, 0.0, 1.0);

  bool alguienGolpea = manoDer.ocupada || manoIzq.ocupada;

  sa += kSuav * (a - sa);
  sb += kSuav * (b - sb);
  if (!alguienGolpea) {
    baseA += kBase * (sa - baseA);
    baseB += kBase * (sb - baseB);
  }

  float da = sa - baseA;
  float db = sb - baseB;
  float na = constrain(da / manoDer.amplitud, -1.5, 1.5);
  float nb = constrain(db / manoIzq.amplitud, -1.5, 1.5);

  float pa = constrain(sa / manoDer.amplitud, -1.5, 1.5);
  float pb = constrain(sb / manoIzq.amplitud, -1.5, 1.5);

  int   n          = (vA ? 1 : 0) + (vB ? 1 : 0);
  float prom       = ((vA ? na : 0) + (vB ? nb : 0)) / n;
  float promPos    = ((vA ? pa : 0) + (vB ? pb : 0)) / n;
  float promGrados = ((vA ? da : 0) + (vB ? db : 0)) / n;

  if (vA && vB && !alguienGolpea) {
    if ((na > 0.25 && nb > 0.25) || (na < -0.25 && nb < -0.25)) {
      bloqueaGestosHasta = ahora + BLOQUEO_GESTOS_MS;
    }
  }

  if (esquiveEnCurso) {
    bloqueaGestosHasta = ahora + BLOQUEO_GESTOS_MS;
    bool enGuardia = fabs(promPos) < RETORNO_POSICION_NORM;
    if (enGuardia && (ahora - inicioEsquive > 250)) {
      terminarEsquive("volviste a la guardia");
    } else if (ahora - inicioEsquive > MAX_ESQUIVE_MS) {
      terminarEsquive("tiempo máximo");
    }
    return;
  }

  float umbral    = (n == 2) ? UMBRAL_ESQUIVE_NORM : UMBRAL_UNA_MANO_NORM;
  bool parecidas  = (n < 2) || (fabs(na - nb) < MAX_DIF_NORM);
  // La otra mano "quieta" puede moverse hasta lo medido en la calibración sin que cuente como esquive
  float quieta    = fmax(0.40f, minPorManoEf);
  bool gestoPecho = (manoDer.pechoCrudo && (!vB || fabs(nb) < quieta)) ||
                    (manoIzq.pechoCrudo && (!vA || fabs(na) < quieta));
  bool otrosGestos = levantarseActivo || cubrirseActivo || gestoPecho || (ahora < bloqueaEsquiveHasta);

  int dir = 0;
  if (!alguienGolpea && !otrosGestos && parecidas && fabs(promGrados) > MIN_GRADOS_ESQUIVE) {
    if (prom >  umbral && promPos >  POSICION_MIN_NORM && (!vA || na >  minPorManoEf) && (!vB || nb >  minPorManoEf)) dir =  1;
    if (prom < -umbral && promPos < -POSICION_MIN_NORM && (!vA || na < -minPorManoEf) && (!vB || nb < -minPorManoEf)) dir = -1;
  }

  if (dir != 0 && (ahora - ultimoEsquive > COOLDOWN_ESQUIVE)) {
    if (esquivePendiente != dir) {
      esquivePendiente = dir;
      esquiveInicio = ahora;
    } else if (ahora - esquiveInicio >= TIEMPO_CONFIRMA_ESQUIVE) {
      iniciarEsquive(dir, na, nb, promPos);
    }
  } else if (dir == 0) {
    esquivePendiente = 0;
  }
}

void detectarCubrirse() {
  static unsigned long tCarga = 0;
  static bool latch = false;

  bool listas = manoDer.respondeCubrir && manoIzq.respondeCubrir && manoDer.esq_listo && manoIzq.esq_listo;
  if (leanFase != 0 || !listas) {
    if (cubrirseActivo) terminarCubrirse(leanFase != 0 ? "calibración" : "sin datos");
    tCarga = 0;
    return;
  }

  unsigned long ahora = millis();
  float a = manoDer.cubrirNorm;
  float b = manoIzq.cubrirNorm;
  float menor = fmin(a, b);

  if (a > 0.25 && b > 0.25) {
    bloqueaGestosHasta = ahora + BLOQUEO_GESTOS_MS;
    bloqueaGolpesHasta = ahora + 400;
    bloquearEsquive(BLOQUEO_GESTOS_MS);
  }

  if (cubrirseActivo) {
    bloqueaGestosHasta = ahora + BLOQUEO_GESTOS_MS;
    if (menor < RETORNO_CUBRIR_NORM) {
      terminarCubrirse("saliste de la postura");
    } else if (ahora - inicioCubrir > MAX_CUBRIR_MS) {
      terminarCubrirse("tiempo máximo");
    }
    return;
  }

  if (menor < RETORNO_CUBRIR_NORM) latch = false;

  bool libre = !esquiveEnCurso && !levantarseActivo && !manoDer.ocupada && !manoIzq.ocupada &&
               !manoDer.enPecho && !manoIzq.enPecho;

  if (menor > umbralCubrirEf && libre && !latch) {
    if (tCarga == 0) tCarga = ahora;
    if (ahora - tCarga >= TIEMPO_CARGA_CUBRIR) {
      latch  = true;
      tCarga = 0;
      iniciarCubrirse(a, b);
    }
  } else {
    tCarga = 0;
  }
}

// ---------------------------------------------------------------------
// 🚀 SETUP Y LOOP
// ---------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
#endif
  delay(300);

  pinMode(PIN_BOTON_CALIB, INPUT_PULLUP);
  if (PIN_BUZZER >= 0) { pinMode(PIN_BUZZER, OUTPUT); digitalWrite(PIN_BUZZER, LOW); }
  if (PIN_LED >= 0)    { pinMode(PIN_LED, OUTPUT);    digitalWrite(PIN_LED, LOW); }

  Wire.begin(SDA_PIN, SCL_PIN, VELOCIDAD_I2C);
  Wire1.begin(SDA2_PIN, SCL2_PIN, VELOCIDAD_I2C);

  manoDer.nombre       = "DER";
  manoDer.bus          = &Wire1;          // <-- Antes decía &Wire (ahora apunta al sensor de tu mano derecha)
  manoDer.addr         = 0x68;
  manoDer.teclaGolpe   = TECLA_GOLPE_DER;
  manoDer.accionPecho  = ACCION_ESTRELLA;
  manoDer.ejeEsq       = 0;
  manoDer.signoEsquive = -1.0;
  manoDer.amplitud     = 20.0;

  manoIzq.nombre       = "IZQ";
  manoIzq.bus          = &Wire;           // <-- Antes decía &Wire1 (ahora apunta al sensor de tu mano izquierda)
  manoIzq.addr         = 0x68;
  manoIzq.teclaGolpe   = TECLA_GOLPE_IZQ;
  manoIzq.accionPecho  = ACCION_LEVANTARSE;
  manoIzq.signoPitch   = 1.0;
  manoIzq.ejeEsq       = 0;
  manoIzq.signoEsquive = 1.0;
  manoIzq.amplitud     = 20.0;

  bool okDer = iniciarMPU(*manoDer.bus, manoDer.addr, manoDer.nombre);
  bool okIzq = iniciarMPU(*manoIzq.bus, manoIzq.addr, manoIzq.nombre);
  delay(100);

  bleKeyboard.begin();

  // 1. Espera a que Windows conecte el Bluetooth antes de empezar nada
  Serial.println("\n⏳ Esperando enlace Bluetooth con la PC...");
  while (!bleKeyboard.isConnected()) {
    delay(200);
  }
  delay(1200); // margen para que Windows registre el teclado virtual

  // 2. Avisa a Python que ya conectó y espera a que presiones BOOT (pin 0)
  avisarPC(PC_ENCENDIDO);
  while (colaPCN > 0 || teclaPCPresionada) {
    actualizarPantallaPC();
    delay(20);
  }

  Serial.println("🔵 ¡Bluetooth Conectado! Pulsa el botón BOOT para iniciar calibración...");
  while (digitalRead(PIN_BOTON_CALIB) == HIGH) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == 'e' || c == 'E' || c == 'c' || c == 'C') break;
    }
    delay(30);
  }

  // Espera a que sueltes el botón BOOT para que el loop() no lo lea como pulsación extra
  while (digitalRead(PIN_BOTON_CALIB) == LOW) {
    delay(30);
  }
  delay(200);

  // 3. Arranca la calibración guiada completa desde la fase 1 (Guardia)
  leanFase = 1;
  inicioCalibEsq = millis() + TIEMPO_ESPERA_CALIB;

  manoDer.dtAnterior = micros();
  manoIzq.dtAnterior = micros();

  if (!okDer || !okIzq) {
    avisar(6, 80, PC_ERROR_SENSOR);
  } else {
    avisar(1, 300, PC_TOMANDO_GUARDIA);
  }
}

void loop() {
  procesarMano(manoDer);
  procesarMano(manoIzq);

  diagnosticoMano(manoDer);
  diagnosticoMano(manoIzq);

  actualizarCalibLean();
  detectarCubrirse();
  detectarEsquive();
  actualizarRafaga();

  actualizarPitidos();
  actualizarLED();
  actualizarPantallaPC();

  if (!guardiaAnunciada && manoDer.esq_listo && manoIzq.esq_listo) {
    guardiaAnunciada = true;
    if (leanFase == 0) avisar(2, 120, PC_GUARDIA_LISTA);
  }

  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'c' || c == 'C') recalibrarEsquive();
    if (c == 'e' || c == 'E') iniciarCalibLean();
  }

  static bool botonAnterior = HIGH;
  static unsigned long tBoton = 0;
  static bool largoDisparado = false;
  bool botonAhora = digitalRead(PIN_BOTON_CALIB);
  if (botonAnterior == HIGH && botonAhora == LOW) { tBoton = millis(); largoDisparado = false; }
  if (botonAhora == LOW && !largoDisparado && millis() - tBoton > BOTON_LARGO_MS) {
    largoDisparado = true;
    iniciarCalibLean();
  }
  if (botonAnterior == LOW && botonAhora == HIGH && !largoDisparado && millis() - tBoton > 50) {
    recalibrarEsquive();
  }
  botonAnterior = botonAhora;

  soltarGolpe(manoDer);
  soltarGolpe(manoIzq);
  if (liberarEstrellaEn != 0 && millis() >= liberarEstrellaEn) {
    bleKeyboard.release(TECLA_ESTRELLA);
    liberarEstrellaEn = 0;
  }
  if (liberarEsquiveEn != 0 && millis() >= liberarEsquiveEn) soltarEsquive();
  if (liberarCubrirEn  != 0 && millis() >= liberarCubrirEn)  soltarCubrir();

  delay(2);
}