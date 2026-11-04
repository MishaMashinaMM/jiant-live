# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
# (JIANT 0.8.0) the tutorial film's script, in Spanish: each chapter its number, title, the graphic tools/video/tutorial.html
# draws beside the FM-1's screen, the screen's action (tools/video/capture_tutorial.c, by the chapter's number) and the
# voice-over's lines. A line is (the words shown, cue): the cue is a step of the chapter's graphic (0, 1, 2 ...). What the
# voice says is the shown text through SAY (the names spelt out as they are said).
CHAPTERS = [
    dict(id='intro', k='', title='JIANT', g='intro', lines=[
        ("Hola. Esto es JIANT.", 0),
        ("Un firmware libre que convierte al FM-1 en un instrumento completo: seis pistas, ocho motores de síntesis, "
         "un secuenciador profundo y efectos, todo dentro de este pequeño teclado.", 1),
        ("Antes de empezar, gracias de corazón a todas las personas que apoyan el proyecto en Ko-fi. "
         "Gracias a ustedes, JIANT sigue creciendo.", 2),
        ("En este tutorial vamos a recorrer el sistema completo, paso a paso.", 3),
    ]),
    dict(id='install', k='01', title='INSTALAR', g='install', lines=[
        ("Primero, la instalación. Abrí el instalador web en Chrome o en Edge, "
         "y conectá el FM-1 directo a la computadora, con un cable USB de datos.", 0),
        ("Apretá Install y esperá a que termine, sin desenchufar.", 1),
        ("Si venís de otro firmware custom, volvé primero al oficial con el botón del instalador.", 2),
    ]),
    dict(id='panel', k='02', title='EL PANEL', g='panel', lines=[
        ("Este es el panel. Arriba hay tres encoders.", 0),
        ("ALGORITHM elige la pista. PRESETS recorre los sonidos. Y SELECT recorre las páginas de la sección donde estás.", 1),
        ("Las cuatro perillas controlan las cuatro tarjetas de la pantalla. "
         "Si las girás rápido barren todo el rango; despacio, ajustan fino.", 2),
        ("Los botones abren las páginas: ENV, LFO, FX, SCL, EDIT, GLO, SAVE, ARP y SEQ. "
         "Y las veintisiete teclas tocan la pista elegida.", 3),
    ]),
    dict(id='screen', k='03', title='LA PANTALLA', g='screen', lines=[
        ("La pantalla tiene siempre el mismo orden. Arriba, el encabezado: "
         "la reproducción, el tempo, la carga del procesador y la batería.", 0),
        ("Debajo, cuatro tarjetas, una por perilla. El valor que estás girando se enciende en naranja.", 1),
        ("Al medio, el gráfico de la página. Y abajo, el nombre de la página y dónde estás.", 2),
        ("Los colores funcionan como una cámara térmica: frío es poco, caliente es mucho. "
         "Violeta es lo seleccionado; naranja, lo activo.", 3),
    ]),
    dict(id='home', k='04', title='HOME', g='home', lines=[
        ("Apretá HOME y llegás al ecosistema: una criatura por pista.", 0),
        ("Cada especie corresponde a un motor, y su forma sale de sus cuatro perillas. "
         "Cuando la pista suena, la criatura se hincha y se calienta con su propio audio.", 1),
        ("La pista elegida se dibuja más grande. Una pista muteada se apaga.", 2),
    ]),
    dict(id='layers', k='05', title='TOCAR Y MANTENER', g='layers', lines=[
        ("Toda la navegación tiene dos gestos. Tocar un botón abre sus páginas; tocarlo otra vez pasa a la siguiente.", 0),
        ("Mantenerlo abre su capa: mientras lo sostenés, las teclas y las perillas cambian de función, "
         "y un mapa te muestra qué hace cada una. Soltás, y volvés a donde estabas.", 1),
        ("Por ejemplo, mantené GLO para mutear pistas con las teclas negras, o mantené FX para tocar efectos en vivo.", 2),
    ]),
    dict(id='engines', k='06', title='OCHO MOTORES', g='engines', lines=[
        ("JIANT tiene ocho motores, todos de síntesis, sin samples. Cualquier motor puede ir en cualquier pista.", 0),
        ("ANALOG: dos osciladores con filtro por voz. FM6: síntesis FM de seis operadores, con treinta y dos algoritmos. "
         "PHASE: distorsión de fase.", 1),
        ("LOFI: sonidos de chip, bytebeat y floatbeat. VOICE: una voz por formantes. "
         "WHEEL: un órgano de drawbars con parlante rotativo.", 2),
        ("NOISE: ruido coloreado y metálico. Y DRUM-X, la batería sintetizada.", 3),
        ("Girá PRESETS para recorrer los sonidos de todos los motores en una sola lista. "
         "Y si querés algo nuevo, mantené EDIT y girá SELECT: el sonido se tira a los dados.", 4),
    ]),
    dict(id='signal', k='07', title='EL CAMINO DEL SONIDO', g='signal', lines=[
        ("Así viaja el sonido dentro de una pista.", 0),
        ("El motor genera la voz. El filtro la esculpe: pasa bajos, pasa altos, pasa banda o peine. "
         "La envolvente y el LFO la mueven en el tiempo.", 1),
        ("Después pasa por la distorsión de la pista, con seis tipos, entre ellos un emulador de amplificador.", 2),
        ("El nivel y el paneo la ubican en la mezcla, y tres envíos la mandan al chorus, al delay y a la reverb, "
         "que comparten todas las pistas.", 3),
        ("Al final, el master: un limitador siempre activo, con clip, punch y ducking.", 4),
    ]),
    dict(id='drumx', k='08', title='DRUM-X', g='drumx', lines=[
        ("DRUM-X es una batería de ocho sonidos generados en tiempo real: "
         "bombo, redoblante, clap, dos hi-hats, tom, rim y cowbell.", 0),
        ("Cada sonido tiene dos lados, A y B. La perilla MORPH lleva todo el kit de un lado al otro: "
         "es la perilla para tocar en vivo.", 1),
        ("Hay nueve kits de fábrica, de la 909 y la 808 a industrial y acústico. Y cada sonido se edita a fondo: "
         "afinación, ruido, capas, realimentación, y su propio envío a un efecto.", 2),
    ]),
    dict(id='seq', k='09', title='EL SECUENCIADOR', g='seq', lines=[
        ("Cada pista tiene su propio secuenciador de sesenta y cuatro pasos, con su largo, su división, su swing y su gate. "
         "Patrones de distinto largo giran unos contra otros.", 0),
        ("En una pista melódica, la página STEP es un piano roll: cada paso guarda hasta cuatro notas, con acento y slide.", 1),
        ("Podés escribir paso a paso, o grabar en vivo con REC y PLAY.", 2),
        ("Y cada paso puede llevar componentes: probabilidad, ratchets, ecos, arpegios, "
         "o notas que caminan por la escala vuelta a vuelta.", 3),
        ("Con PLAYHEADS, hasta tres cabezales más leen el mismo patrón a otra velocidad, o al revés. "
         "Y MUTATE transforma el sonido y el patrón cada tantos compases.", 4),
    ]),
    dict(id='mod', k='10', title='MODULACIÓN', g='mod', lines=[
        ("La modulación le da vida. Cada pista tiene una envolvente y un LFO, "
         "con sus destinos: afinación, filtro, forma y amplitud.", 0),
        ("La matriz MOD suma cuatro rutas más por pista, con fuentes como la velocidad, "
         "el secuenciador de modulación o el ruido, y destinos que llegan hasta los efectos.", 1),
        ("Y las cuatro macros: mantené LFO, y las perillas pasan a ser M1 a M4, que mueven muchos parámetros a la vez.", 2),
    ]),
    dict(id='fx', k='11', title='EFECTOS', g='fx', lines=[
        ("Los efectos. Un delay analógico o digital, con pitch y ping pong. "
         "Una reverb con cinco modelos, entre ellos SHIMMER, RESO afinada a la escala, y CLOUD, granular.", 0),
        ("Un chorus estéreo, y el SLICER, que corta el audio en ritmos sobre las pistas que elijas.", 1),
        ("Y en vivo: mantené FX, y cada tecla es un efecto momentáneo. Repeticiones, filtros, octavas, stutters.", 2),
        ("Todo lo que tocás se puede grabar en un carril que se repite con el tema.", 3),
    ]),
    dict(id='gen', k='12', title='GENERADOR Y DADOS', g='gen', lines=[
        ("¿Sin ideas? En HOME girá SELECT y se abre el generador. Elegí un género, apretá OCT más, "
         "y aparece un tema entero: motores, sonidos, partes, escala, tempo y efectos.", 0),
        ("Los dados funcionan en cada parte: mantené SEQ, LFO o FX, y girá SELECT para variar solo eso.", 1),
        ("Y si te arrepentís, mantené GLO y apretá la tecla G4: deshacer.", 2),
    ]),
    dict(id='arrange', k='13', title='GUARDAR Y ARMAR TEMAS', g='arrange', lines=[
        ("Para guardar, cada proyecto tiene cuatro secciones, de la A a la D. Guardá una idea en A, y una variación en B.", 0),
        ("En la página ARRANGE encadenás secciones en filas, cada una con su largo en compases. "
         "Apretá PLAY, y el tema se reproduce en orden.", 1),
        ("Y en vivo, mantené SAVE: las teclas disparan secciones en el compás siguiente.", 2),
    ]),
    dict(id='usb', k='14', title='USB Y MIDI', g='usb', lines=[
        ("El FM-1 también es una interfaz de audio USB. En el modo STEMS manda ocho canales: "
         "las seis pistas por separado y los efectos, directo a tu DAW.", 0),
        ("Con IN+OUT, la computadora o el celular suenan a través del FM-1. "
         "Y el nuevo modo STEMS+IN junta las dos cosas: grabás los stems, y tu voz pasa por los efectos de la última pista.", 1),
        ("Además, MIDI por USB, por Bluetooth y por TRS, con clock para sincronizarte con otros equipos.", 2),
    ]),
    dict(id='web', k='15', title='EN LA WEB', g='web', lines=[
        ("En la web hay tres herramientas: el emulador, que corre JIANT en el navegador; el instalador; "
         "y el editor, para manejar sonidos, bancos y backups desde la computadora.", 0),
        ("Desde el emulador también podés renderizar los stems de una canción de tu backup, más rápido que en tiempo real.", 1),
    ]),
    dict(id='outro', k='', title='GRACIAS', g='outro', lines=[
        ("Eso es JIANT. El manual completo, en español y en inglés, está en la página del proyecto.", 0),
        ("Si este trabajo resuena con vos, podés apoyarlo en Ko-fi, o en Cafecito desde Argentina.", 1),
        ("Gracias por mirar, y gracias otra vez a quienes lo hacen posible. Nos vemos en la próxima.", 2),
    ]),
]

# how the voice says what the screen spells (the order matters: the longer names first)
SAY = [
    ('STEMS+IN', 'stems más in'), ('IN+OUT', 'in más out'), ('FM-1', 'efe eme uno'), ('FM6', 'efe eme seis'),
    ('DRUM-X', 'drum equis'), ('M1 a M4', 'eme uno a eme cuatro'), ('Ko-fi', 'Kofi'), ('JIANT', 'Jiant'),
    ('USB', 'u ese be'), ('LFO', 'ele efe o'), ('FX', 'efe equis'), ('SCL', 'escala'), ('ENV', 'envolvente'),
    ('GLO', 'glo'), ('SEQ', 'sec'), ('DAW', 'dau'), ('TRS', 'te erre ese'), ('MIDI', 'midi'), ('G4', 'ge cuatro'),
    ('OCT más', 'octava más'), ('MOD', 'mod'), ('STEP', 'step'), ('PLAYHEADS', 'pleijeds'), ('MUTATE', 'miuteit'),
    ('PRESETS', 'presets'), ('SELECT', 'selec'), ('ALGORITHM', 'álgoritm'), ('HOME', 'jom'), ('SAVE', 'seiv'),
    ('EDIT', 'edit'), ('ARP', 'arp'), ('REC', 'rec'), ('PLAY', 'plei'), ('Install', 'instol'), ('ARRANGE', 'arreinch'),
    ('SLICER', 'slaicer'), ('SHIMMER', 'shimer'), ('CLOUD', 'claud'), ('RESO', 'reso'), ('MORPH', 'morf'),
    ('ANALOG', 'análog'), ('PHASE', 'feis'), ('LOFI', 'lo fai'), ('VOICE', 'vois'), ('WHEEL', 'uil'),
    ('NOISE', 'nois'), ('STEMS', 'stems'), ('bytebeat', 'baitbit'), ('floatbeat', 'floutbit'), ('drawbars', 'drobars'),
    ('hi-hats', 'jai jats'), ('cowbell', 'caubel'), ('ratchets', 'rachets'), ('stutters', 'státers'),
    ('ping pong', 'ping pong'), ('custom', 'castom'), ('Chrome', 'crom'), ('Edge', 'edch'), ('808', 'ocho cero ocho'),
    ('909', 'nueve cero nueve'), ('Bluetooth', 'blutuz'), ('slide', 'slaid'), ('swing', 'suing'), ('gate', 'geit'),
    ('clock', 'clok'), ('backups', 'bakaps'), ('Cafecito', 'cafecito'), ('punch', 'panch'), ('ducking', 'daking'),
    ('clip', 'clip'), ('chorus', 'corus'), ('delay', 'dilei'), ('pitch', 'pich'), ('stems', 'stems'),
    ('rim', 'rim'), ('tom', 'tom'), ('clap', 'clap'),
]


def say(text):
    import re
    for a, b in SAY:
        text = re.sub(r'(?<![\w-])' + re.escape(a) + r'(?![\w-])', b, text)
    return text
