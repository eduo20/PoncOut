# muneco_guante.py
# Muñequito que reacciona a lo que detecta el guante LittleMac.
#  - Mira las MISMAS teclas que el guante le manda al juego (golpes, esquive, cubrirse, estrella, levantarse).
#  - Muestra los 10 pasos de la calibración (avisos por atajo Ctrl+Alt+Shift + tecla) y hace la pose que te pide.
#  - Al terminar la calibración, sigue replicando en vivo todos tus movimientos.
# Requisito:  pip install pynput
# Probar sin el guante:  python muneco_guante.py --demo
import ctypes
import math
import queue
import signal
import sys
import time
import tkinter as tk
import traceback
from collections import deque

from pynput import keyboard

# =========================== AJUSTES ===========================
ESCALA = 1.0                  # tamaño del muñeco (0.7 = más chico, 1.3 = más grande)
POSICION = "abajo-derecha"    # abajo-derecha | abajo-izquierda | arriba-derecha | arriba-izquierda | centro
MARGEN = 20                   # píxeles de separación con el borde de la pantalla
FPS = 60

ESPERA_GUARDIA_S = 2          # segundos que el ESP espera antes de medir la guardia (TIEMPO_ESPERA_CALIB del ESP)
GOLPES_POR_FASE = 4           # debe ser igual a GOLPES_POR_FASE del ESP

# Estas teclas deben ser las MISMAS que en el bloque TECLAS del código del ESP
TECLA_GOLPE_DER   = "d"
TECLA_GOLPE_IZQ   = "f"
TECLA_LEVANTARSE  = "d"                 # se manda junto con ARRIBA en ráfaga
TECLA_MOD_CABEZA  = keyboard.Key.up     # golpe a la cabeza = tecla del golpe + esta
TECLA_ESTRELLA    = keyboard.Key.enter
TECLA_ESQUIVE_DER = keyboard.Key.right
TECLA_ESQUIVE_IZQ = keyboard.Key.left
TECLA_CUBRIRSE    = keyboard.Key.down
# ===============================================================

FONDO = "#010101"             # este color se vuelve transparente (no lo uses en el dibujo)
W, H, CX = 420, 380, 210      # tamaño del lienzo y centro del muñeco

# Avisos de estado del ESP (Ctrl+Alt+Shift + tecla): tecla virtual de Windows -> código
CODIGO_POR_VK = {
    0x2C: 0,    # Print Screen
    0x91: 1,    # Scroll Lock
    0x13: 2,    # Pause
    0x2D: 3,    # Insert
    0x24: 4,    # Home
    0x21: 5,    # Page Up
    0x23: 6,    # End
    0x22: 7,    # Page Down
    0x6F: 8,    # Teclado numérico /
    0x6A: 9,    # Teclado numérico *
    0x6D: 10,   # Teclado numérico -
    0x6B: 11,   # Teclado numérico +
    0x70: 12,   # F1
    0x71: 13,   # F2
    0x72: 14,   # F3
    0x73: 15,   # F4 (Paso 10/10: golpes a la cabeza izquierda)
    0x74: 16,  # F5 (golpe de calibración registrado)
}

# código -> (texto, color de fondo, segundos que se muestra; 0 = hasta el siguiente aviso, cuenta atrás en segundos; 0 = sin)
MENSAJES = {
    0:  ("Guante conectado: pulsa el botón BOOT para empezar la calibración", "#0d47a1", 0, 0),
    1:  ("Un sensor no responde: revisa los cables", "#b71c1c", 0, 0),
    2:  ("Paso 1/10: ponte en GUARDIA y no te muevas", "#0d47a1", 0, ESPERA_GUARDIA_S),
    3:  ("Guardia lista", "#1b5e20", 3, 0),
    4:  ("Paso 2/10: inclínate a la DERECHA y mantén", "#e65100", 0, 3.5),
    5:  ("Vuelve a GUARDIA", "#0d47a1", 0, 2.5),
    6:  ("Paso 3/10: inclínate a la IZQUIERDA y mantén", "#e65100", 0, 3.5),
    7:  ("Paso 4/10: mano DERECHA al PECHO (la otra quieta) y mantén", "#e65100", 0, 3.5),
    8:  ("Paso 5/10: mano IZQUIERDA al PECHO (la otra quieta) y mantén", "#e65100", 0, 3.5),
    9:  ("Paso 6/10: CÚBRETE con las DOS manos y mantén", "#e65100", 0, 4.5),
    10: ("Calibración lista: ¡a jugar!", "#1b5e20", 6, 0),
    11: ("Calibración con avisos: algún gesto se confunde. Mira el monitor serial o repite", "#b71c1c", 0, 0),
    12: ("Paso 7/10: lanza golpes al CUERPO con la mano izquierda", "#e65100", 0, 0),
    13: ("Paso 8/10: lanza golpes a la CABEZA con la mano izquierda", "#e65100", 0, 0),
    14: ("Paso 9/10: lanza golpes al CUERPO con la mano DERECHA", "#e65100", 0, 0),
    15: ("Paso 10/10: lanza golpes a la CABEZA con la mano DERECHA", "#e65100", 0, 0),
}

# pose que el muñeco hace mientras la calibración te la pide
POSE_POR_CODIGO = {4: "der", 6: "izq", 7: "pecho_der", 8: "pecho_izq", 9: "cubrir"}

# pasos de golpes de la calibración: código -> (mano, altura)
FASE_GOLPES = {12: ("der", "cuerpo"), 13: ("der", "cabeza"), 14: ("izq", "cuerpo"), 15: ("izq", "cabeza")}


# ------------------------- utilidades -------------------------
def S(v):
    return v * ESCALA


def mezcla(a, b, t):
    return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t)


def vk_de(tecla):
    vk = getattr(tecla, "vk", None)
    if vk is None and hasattr(tecla, "value"):
        vk = getattr(tecla.value, "vk", None)
    return vk


def normalizar(tecla):
    """Letra en minúscula si es un carácter; si no, el objeto Key tal cual."""
    ch = getattr(tecla, "char", None)
    return ch.lower() if isinstance(ch, str) else tecla


def tipo_modificador(tecla):
    if tecla in (keyboard.Key.ctrl, keyboard.Key.ctrl_l, keyboard.Key.ctrl_r):
        return "ctrl"
    if tecla in (keyboard.Key.alt, keyboard.Key.alt_l, keyboard.Key.alt_r, keyboard.Key.alt_gr):
        return "alt"
    if tecla in (keyboard.Key.shift, keyboard.Key.shift_l, keyboard.Key.shift_r):
        return "shift"
    return None


# ------------------- hilo del teclado -> cola -------------------
cola = queue.Queue()      # eventos que pasan del hilo del teclado a la ventana
abajo = set()             # teclas presionadas ahora (para ignorar la auto-repetición)
modificadores = set()


def al_presionar(tecla):
    mod = tipo_modificador(tecla)
    if mod:
        modificadores.add(mod)
        return

    # 1) Aviso de estado del ESP: Ctrl+Alt+Shift + tecla rara
    if {"ctrl", "alt", "shift"} <= modificadores:
        codigo = CODIGO_POR_VK.get(vk_de(tecla))
        if codigo is not None:
            cola.put(("estado", codigo))
        return

    # 2) Teclas del juego (las mismas que el guante le manda al juego)
    k = normalizar(tecla)
    if k in abajo:                        # tecla mantenida: ignora la auto-repetición
        return
    con_arriba = TECLA_MOD_CABEZA in abajo
    abajo.add(k)
    cola.put(("abajo", k, con_arriba))


def al_soltar(tecla):
    mod = tipo_modificador(tecla)
    if mod:
        modificadores.discard(mod)
        return
    k = normalizar(tecla)
    abajo.discard(k)
    cola.put(("arriba", k))


# ------------------------- estado del muñeco -------------------------
class Estado:
    def __init__(self):
        self.listo = False            # ¿ya hay guardia?
        self.pose = None              # pose que muestra la calibración
        self.esquive = 0              # -1 izquierda, 0 ninguno, +1 derecha
        self.t_esq = 0.0
        self.cubrir = False
        self.t_cub = 0.0
        self.levantarse = False
        self.t_lev = 0.0
        self.rafaga = deque()         # momentos de las pulsaciones (tecla + arriba) para detectar "levantarse"
        self.golpes = []              # golpes en animación: [mano, altura, t0]
        self.etiqueta = ""
        self.t_etiqueta = 0.0
        self.banner = None            # (texto, color)
        self.banner_fin = 0.0
        self.banner_codigo = None
        self.cuenta_fin = 0.0         # cuenta atrás que se muestra junto al mensaje
        self.fase_golpe = None        # (mano, altura) mientras la calibración pide golpes
        self.golpes_n = 0             # golpes registrados en esa fase
        self.celebrar_t0 = None
        # valores suavizados que se dibujan
        self.esq = 0.0
        self.cub = 0.0
        self.lev = 0.0
        self.pd = 0.0
        self.pi = 0.0


est = Estado()
est.banner = ("Esperando al guante: enciende el ESP y conecta el Bluetooth", "#37474f")
est.banner_codigo = -1


def al_estado(codigo):
    ahora = time.perf_counter()

    # Golpe de calibración registrado por el ESP: cuenta y repite el golpe en el muñeco
    if codigo == 16:
        if est.fase_golpe:
            est.golpes_n += 1
            mano, altura = est.fase_golpe
            est.golpes.append([mano, altura, ahora])
            est.etiqueta = "GOLPE %s (%s)" % ("DERECHO" if mano == "der" else "IZQUIERDO", altura)
            est.t_etiqueta = ahora
        return

    texto, color, segundos, cuenta = MENSAJES[codigo]
    est.banner = (texto, color)
    est.banner_fin = ahora + segundos if segundos > 0 else 0.0
    est.cuenta_fin = ahora + cuenta if cuenta else 0.0
    est.banner_codigo = codigo
    est.pose = POSE_POR_CODIGO.get(codigo)
    est.fase_golpe = FASE_GOLPES.get(codigo)
    est.golpes_n = 0
    est.listo = codigo not in (0, 1, 2)
    if codigo == 10:
        est.celebrar_t0 = ahora


def al_tecla_abajo(k, con_arriba):
    ahora = time.perf_counter()

    if k == TECLA_ESTRELLA:
        est.listo = True
        est.golpes.append(["der", "estrella", ahora])
        est.etiqueta, est.t_etiqueta = "¡GOLPE ESTRELLA!", ahora

    elif k == TECLA_ESQUIVE_DER:
        est.listo = True
        est.esquive, est.t_esq = 1, ahora

    elif k == TECLA_ESQUIVE_IZQ:
        est.listo = True
        est.esquive, est.t_esq = -1, ahora

    elif k == TECLA_CUBRIRSE:
        est.listo = True
        est.cubrir, est.t_cub = True, ahora

    elif k in (TECLA_GOLPE_DER, TECLA_GOLPE_IZQ):
        est.listo = True
        # Levantarse = ráfaga de (tecla + arriba): con 3 seguidas en 0.6 s se considera ráfaga
        if con_arriba and k == TECLA_LEVANTARSE:
            est.rafaga.append(ahora)
            while est.rafaga and ahora - est.rafaga[0] > 0.6:
                est.rafaga.popleft()
            if len(est.rafaga) >= 3:
                est.levantarse = True
            if est.levantarse:
                est.t_lev = ahora
                return
        mano = "der" if k == TECLA_GOLPE_DER else "izq"
        altura = "cabeza" if con_arriba else "cuerpo"
        est.golpes.append([mano, altura, ahora])
        est.etiqueta = "GOLPE %s (%s)" % ("DERECHO" if mano == "der" else "IZQUIERDO", altura)
        est.t_etiqueta = ahora

    else:
        return

    # Un aviso de espera o de error ya no tiene sentido si el guante está funcionando
    if est.banner_codigo in (-1, 1, 11):
        est.banner = None


def al_tecla_arriba(k):
    if k == TECLA_ESQUIVE_DER and est.esquive == 1:
        est.esquive = 0
    elif k == TECLA_ESQUIVE_IZQ and est.esquive == -1:
        est.esquive = 0
    elif k == TECLA_CUBRIRSE:
        est.cubrir = False


def procesar_evento(ev):
    if ev[0] == "estado":
        al_estado(ev[1])
    elif ev[0] == "abajo":
        al_tecla_abajo(ev[1], ev[2])
    elif ev[0] == "arriba":
        al_tecla_arriba(ev[1])


def actualizar_estado(ahora, dt):
    # Tiempos máximos, por si se pierde una tecla "soltada"
    if est.esquive and ahora - est.t_esq > 6.0:
        est.esquive = 0
    if est.cubrir and ahora - est.t_cub > 11.0:
        est.cubrir = False
    if est.levantarse and ahora - est.t_lev > 0.45:
        est.levantarse = False
        est.rafaga.clear()
    if est.banner and est.banner_fin and ahora > est.banner_fin:
        est.banner = None
        est.banner_codigo = None

    # Objetivos (la pose de la calibración manda sobre lo demás mientras dura)
    obj_esq = float(est.esquive)
    obj_cub = 1.0 if est.cubrir else 0.0
    obj_pd = obj_pi = 0.0
    if est.pose == "der":
        obj_esq = 1.0
    elif est.pose == "izq":
        obj_esq = -1.0
    elif est.pose == "cubrir":
        obj_cub = 1.0
    elif est.pose == "pecho_der":
        obj_pd = 1.0
    elif est.pose == "pecho_izq":
        obj_pi = 1.0
    obj_lev = 1.0 if est.levantarse else 0.0

    k = 1.0 - math.exp(-dt * 14.0)    # suavizado independiente de los FPS
    est.esq += (obj_esq - est.esq) * k
    est.cub += (obj_cub - est.cub) * k
    est.lev += (obj_lev - est.lev) * k
    est.pd += (obj_pd - est.pd) * k
    est.pi += (obj_pi - est.pi) * k


# ----------------------------- ventana -----------------------------
raiz = tk.Tk()
raiz.title("LittleMac")
raiz.protocol("WM_DELETE_WINDOW", lambda: None)  # evita cierre accidental por Alt+F4
raiz.overrideredirect(True)                      # sin bordes
raiz.attributes("-topmost", True)                # siempre encima
raiz.attributes("-transparentcolor", FONDO)      # el fondo se ve transparente
ANCHO, ALTO = int(S(W)), int(S(H))
sw, sh = raiz.winfo_screenwidth(), raiz.winfo_screenheight()
px = MARGEN if "izquierda" in POSICION else (sw - ANCHO - MARGEN if "derecha" in POSICION else (sw - ANCHO) // 2)
py = MARGEN if "arriba" in POSICION else (sh - ALTO - MARGEN - 40 if "abajo" in POSICION else (sh - ALTO) // 2)
raiz.geometry("%dx%d+%d+%d" % (ANCHO, ALTO, px, py))
lienzo = tk.Canvas(raiz, width=ANCHO, height=ALTO, bg=FONDO, highlightthickness=0)
lienzo.pack()


def no_robar_foco():
    """Que la ventana no le quite el foco al juego ni capture clics."""
    try:
        raiz.update_idletasks()
        user32 = ctypes.windll.user32
        hwnd = user32.GetParent(raiz.winfo_id()) or raiz.winfo_id()
        GWL_EXSTYLE = -20
        estilo = user32.GetWindowLongW(hwnd, GWL_EXSTYLE)
        # LAYERED | TRANSPARENT (los clics la atraviesan) | TOOLWINDOW | NOACTIVATE (no toma el foco)
        estilo |= 0x00080000 | 0x00000020 | 0x00000080 | 0x08000000
        user32.SetWindowLongW(hwnd, GWL_EXSTYLE, estilo)
    except Exception as error:
        print("No se pudo evitar el robo de foco:", error)


# ----------------------------- dibujo -----------------------------
def oval(x, y, r, **kw):
    if "width" in kw:
        kw["width"] = max(1, S(kw["width"]))
    lienzo.create_oval(S(x - r), S(y - r), S(x + r), S(y + r), **kw)


def poly(puntos, **kw):
    if "width" in kw:
        kw["width"] = max(1, S(kw["width"]))
    lienzo.create_polygon([S(v) for p in puntos for v in p], **kw)


def linea(puntos, **kw):
    if "width" in kw:
        kw["width"] = max(1, S(kw["width"]))
    lienzo.create_line([S(v) for p in puntos for v in p], capstyle="round", joinstyle="round", **kw)


def texto(x, y, txt, color="white", tam=16):
    f = ("Segoe UI", max(8, int(S(tam))), "bold")
    lienzo.create_text(S(x) + 2, S(y) + 2, text=txt, fill="#000000", font=f)
    lienzo.create_text(S(x), S(y), text=txt, fill=color, font=f)


def paleta(listo):
    if listo:
        return dict(piel="#f1c27d", camiseta="#f5f5f5", short="#2e7d32", guante="#d32f2f", contorno="#111111")
    return dict(piel="#9e9e9e", camiseta="#b0b0b0", short="#757575", guante="#616161", contorno="#222222")


def brazo(hombro, guante, lado, doblez, piel, contorno):
    mx, my = (hombro[0] + guante[0]) / 2, (hombro[1] + guante[1]) / 2
    codo = (mx + lado * 26 * doblez, my + 24 * doblez)
    linea([hombro, codo, guante], width=19, fill=contorno)
    linea([hombro, codo, guante], width=13, fill=piel)


def estrella_pts(cx, cy, r_ext, r_int, rot):
    pts = []
    for i in range(10):
        ang = rot + i * math.pi / 5 - math.pi / 2
        r = r_ext if i % 2 == 0 else r_int
        pts.append((cx + r * math.cos(ang), cy + r * math.sin(ang)))
    return pts


def dibujar(ahora):
    lienzo.delete("all")
    p = paleta(est.listo)
    piel, cam, sho, gua, con = p["piel"], p["camiseta"], p["short"], p["guante"], p["contorno"]

    # ---- celebración al terminar la calibración ----
    celebrando = est.celebrar_t0 is not None and ahora - est.celebrar_t0 < 1.4
    if est.celebrar_t0 is not None and not celebrando:
        est.celebrar_t0 = None

    # ---- geometría del cuerpo ----
    bob = math.sin(ahora * 3.0) * 2.0 if est.listo else 0.0
    ae = abs(est.esq)
    dx_cab = 95 * est.esq
    dx_hom = 70 * est.esq
    dx_cad = 25 * est.esq
    caida_cab = 14 * ae + 10 * est.cub + 62 * est.lev
    caida_hom = 8 * ae + 6 * est.cub + 58 * est.lev
    caida_cad = 3 * ae + 36 * est.lev

    hx, hy = CX + dx_cab, 92 + bob + caida_cab
    SL = (CX - 44 + dx_hom, 135 + bob + caida_hom)
    SR = (CX + 44 + dx_hom, 135 + bob + caida_hom)
    CL = (CX - 30 + dx_cad, 225 + caida_cad)
    CR = (CX + 30 + dx_cad, 225 + caida_cad)
    PL = (CX - 42, 322)
    PR = (CX + 42, 322)

    # ---- piernas y zapatos ----
    for cad, pie, lado in ((CL, PL, -1), (CR, PR, 1)):
        rod = ((cad[0] + pie[0]) / 2 + lado * 16 * est.lev, (cad[1] + pie[1]) / 2)
        linea([cad, rod, pie], width=21, fill=con)
        linea([cad, rod, pie], width=15, fill=piel)
        oval(pie[0] + lado * 5, pie[1] + 5, 14, fill="#3e2723", outline=con, width=2)

    # ---- short y torso ----
    poly([(CL[0] - 4, CL[1] - 14), (CR[0] + 4, CR[1] - 14), (CR[0] + 12, CR[1] + 34), (CL[0] - 12, CL[1] + 34)],
         fill=sho, outline=con, width=3)
    poly([SL, SR, (CR[0] + 6, CR[1] - 10), (CL[0] - 6, CL[1] - 10)], fill=cam, outline=con, width=3)
    linea([(hx, hy + 20), ((SL[0] + SR[0]) / 2, SL[1] + 4)], width=16, fill=piel)   # cuello

    # ---- posición de los guantes ----
    gd = (SR[0] + 18, SR[1] - 18)                                    # guardia
    gi = (SL[0] - 18, SL[1] - 18)
    gd = mezcla(gd, (hx + 15, hy + 8), est.cub)                      # cubrirse
    gi = mezcla(gi, (hx - 15, hy + 8), est.cub)
    gd = mezcla(gd, (CX + 16 + dx_hom, SR[1] + 40), est.pd)          # mano al pecho (calibración)
    gi = mezcla(gi, (CX - 16 + dx_hom, SL[1] + 40), est.pi)
    ph = math.sin(ahora * 2 * math.pi * 4.5)                         # levantarse: golpea rápido
    gd = mezcla(gd, (SR[0] + 12, SR[1] - 8 - 32 * ph), est.lev)
    gi = mezcla(gi, (SL[0] - 12, SL[1] - 8 + 32 * ph), est.lev)
    if celebrando:                                                   # brazos arriba
        salto = abs(math.sin((ahora - est.celebrar_t0) * 9)) * 14
        gd = (SR[0] + 40, SR[1] - 105 - salto)
        gi = (SL[0] - 40, SL[1] - 105 - salto)

    # ---- golpes en animación ----
    ext = {"der": 0.0, "izq": 0.0}
    objetivo = {}
    anillos = []
    estrella_u = None
    for g in list(est.golpes):
        mano, altura, t0 = g
        dur = 0.9 if altura == "estrella" else 0.34
        u = (ahora - t0) / dur
        if u >= 1.0:
            est.golpes.remove(g)
            continue
        extension = max(0.0, math.sin(math.pi * u)) ** 0.6
        lado = 1 if mano == "der" else -1
        ty = {"cabeza": 72, "cuerpo": 172, "estrella": 48}[altura]
        tx = CX + lado * 10 + dx_hom
        if extension >= ext[mano]:
            ext[mano] = extension
            objetivo[mano] = (tx, ty)
        if 0.28 < u < 0.75:
            anillos.append((tx, ty, 22 + 70 * (u - 0.28) / 0.47))
        if altura == "estrella":
            estrella_u = u

    glove = {"der": gd, "izq": gi}
    for mano, destino in objetivo.items():
        glove[mano] = mezcla(glove[mano], destino, ext[mano])
    radio = {"der": 20 + 17 * ext["der"], "izq": 20 + 17 * ext["izq"]}

    # ---- brazos ----
    brazo(SL, glove["izq"], -1, 1 - ext["izq"], piel, con)
    brazo(SR, glove["der"], 1, 1 - ext["der"], piel, con)

    # ---- cabeza ----
    oval(hx, hy, 27, fill=piel, outline=con, width=3)
    lienzo.create_arc(S(hx - 27), S(hy - 27), S(hx + 27), S(hy + 27), start=25, extent=130,
                      style="chord", fill="#3e2723", outline=con, width=max(1, S(2)))
    if est.listo:
        oval(hx - 9, hy - 2, 3, fill="#111111")
        oval(hx + 9, hy - 2, 3, fill="#111111")
        linea([(hx - 7, hy + 12), (hx + 7, hy + 12)], width=3, fill="#7b1f1f")
    else:
        linea([(hx - 13, hy - 2), (hx - 5, hy - 2)], width=3, fill="#111111")     # ojos cerrados
        linea([(hx + 5, hy - 2), (hx + 13, hy - 2)], width=3, fill="#111111")
        texto(hx + 40, hy - 30, "zZz", "#cfd8dc", 14)

    # ---- guantes (primero los quietos, al final los que golpean) ----
    for mano in sorted(("der", "izq"), key=lambda m: ext[m]):
        color = "#ffc107" if (mano == "der" and estrella_u is not None) else gua
        oval(glove[mano][0], glove[mano][1], radio[mano], fill=color, outline=con, width=3)
        oval(glove[mano][0] - radio[mano] * 0.3, glove[mano][1] - radio[mano] * 0.3, radio[mano] * 0.22,
             fill="#ffffff" if est.listo else "#9e9e9e")

    # ---- aros de impacto y estrella ----
    for ax, ay, r in anillos:
        oval(ax, ay, r, outline="#ffeb3b", width=5, fill="")
    if estrella_u is not None:
        tam = 20 + 24 * math.sin(math.pi * estrella_u)
        poly(estrella_pts(CX + 10 + dx_hom, 34, tam, tam * 0.45, estrella_u * 2 * math.pi),
             fill="#ffd600", outline="#ff6f00", width=3)

    # ---- texto de la acción ----
    if est.levantarse:
        texto(CX, 20, "¡LEVÁNTATE!" if int(ahora * 6) % 2 == 0 else "¡LEVÁNTATE!!", "#ffeb3b", 20)
    elif est.cubrir or est.pose == "cubrir":
        texto(CX, 20, "CUBRIÉNDOTE", "#80d8ff", 18)
    elif est.esquive or est.pose in ("der", "izq"):
        lado_txt = est.esquive or (1 if est.pose == "der" else -1)
        texto(CX, 20, "ESQUIVE " + ("DERECHA" if lado_txt > 0 else "IZQUIERDA"), "#ffab40", 18)
    elif ahora - est.t_etiqueta < 0.7 and est.etiqueta:
        texto(CX, 20, est.etiqueta, "#ffffff", 16)
    elif not est.listo:
        texto(CX, 20, "Esperando guardia...", "#cfd8dc", 16)

    # ---- barra de mensajes de la calibración ----
    if est.banner:
        mensaje, color = est.banner
        if est.cuenta_fin > ahora:
            mensaje += "    [%d]" % math.ceil(est.cuenta_fin - ahora)
        if est.fase_golpe:
            mensaje += "    [%d/%d]" % (est.golpes_n, GOLPES_POR_FASE)
        lienzo.create_rectangle(S(10), S(H - 54), S(W - 10), S(H - 8), fill=color, outline="#000000", width=2)
        lienzo.create_text(S(CX), S(H - 31), text=mensaje, fill="white", width=S(W - 40),
                           font=("Segoe UI", max(8, int(S(12))), "bold"), justify="center")


# ----------------------------- demo -----------------------------
DEMO = [
    (0.2, ("estado", 0)), (0.6, ("estado", 2)), (1.0, ("estado", 3)),
    (2.0, ("abajo", "d", False)), (2.1, ("arriba", "d")),
    (3.0, ("abajo", "d", True)), (3.1, ("arriba", "d")),
    (4.0, ("abajo", "f", False)), (4.1, ("arriba", "f")),
    (5.0, ("abajo", "f", True)), (5.1, ("arriba", "f")),
    (6.5, ("abajo", keyboard.Key.right, False)), (8.0, ("arriba", keyboard.Key.right)),
    (8.7, ("abajo", keyboard.Key.left, False)), (10.2, ("arriba", keyboard.Key.left)),
    (11.0, ("abajo", keyboard.Key.down, False)), (13.0, ("arriba", keyboard.Key.down)),
    (14.0, ("abajo", keyboard.Key.enter, False)), (14.1, ("arriba", keyboard.Key.enter)),
] + [(16.0 + i * 0.11, ("abajo", "d", True)) for i in range(22)] + [
    (19.5, ("estado", 4)), (22.0, ("estado", 6)), (24.5, ("estado", 7)),
    (27.0, ("estado", 8)), (29.5, ("estado", 9)),
    (34.0, ("estado", 12)), (35.5, ("estado", 16)), (37.0, ("estado", 16)), (38.5, ("estado", 16)), (40.0, ("estado", 16)),
    (42.0, ("estado", 15)), (43.5, ("estado", 16)), (45.0, ("estado", 16)), (46.5, ("estado", 16)), (48.0, ("estado", 16)),
    (50.0, ("estado", 10)), (55.0, ("estado", 3)),
]
DURACION_DEMO = 58.0


def programar_demo():
    for t, ev in DEMO:
        raiz.after(int(t * 1000), lambda ev=ev: cola.put(ev))
    raiz.after(int(DURACION_DEMO * 1000), programar_demo)


# ----------------------------- bucle principal -----------------------------
ultimo = time.perf_counter()


def tick():
    global ultimo
    ahora = time.perf_counter()
    dt = min(0.1, ahora - ultimo)
    ultimo = ahora
    try:
        while True:
            procesar_evento(cola.get_nowait())
    except queue.Empty:
        pass
    actualizar_estado(ahora, dt)
    dibujar(ahora)
    raiz.after(int(1000 / FPS), tick)


signal.signal(signal.SIGINT, signal.SIG_IGN)
no_robar_foco()
if "--demo" in sys.argv:
    print("Modo demo: el muñeco repite todas las animaciones (cierra la consola para salir)")
    programar_demo()
else:
    keyboard.Listener(on_press=al_presionar, on_release=al_soltar).start()
    print("Escuchando al guante... (cierra esta consola para salir)")

try:
    tick()
    raiz.mainloop()
except Exception:
    traceback.print_exc()
    input("Error fatal. Presiona Enter para cerrar...")