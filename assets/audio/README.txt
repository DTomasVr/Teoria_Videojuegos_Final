Audio del juego (SDL3_mixer, API MIX_*).

Deja aqui tus archivos de sonido. El motor es tolerante a fallos: si un archivo no
existe, el juego corre igual pero sin ese sonido/musica.

--- EFECTOS (SFX) ---
Se cargan PREDESCODIFICADOS con Audio::load(nombre, ruta) y se disparan con
Audio::play(nombre). Nombres que ya usa el juego:
  death.wav  -> al morir el clon
  dash.wav   -> al hacer el Dash

--- MUSICA DE FONDO (en bucle) ---
Se reproduce con Audio::playMusic(ruta): un tema en bucle en una pista propia que
sustituye al anterior (si ya suena esa ruta, no se reinicia -> musica fluida entre
escenas). Formato recomendado .ogg (tambien vale .wav u otros de SDL3_mixer).
Rutas que YA espera el codigo (pon aqui los archivos con estos nombres exactos):
  music_menu.ogg       -> menu principal
  music_cinematic.ogg  -> todas las cinematicas (intro, entradas de camara, jefe, victoria)
  music_camara01.ogg   -> Camara 01 (El Pozo)
  music_camara02.ogg   -> Camara 02 (La Trinchera)
  music_camara03.ogg   -> Camara 03 (El Suelo de Matanza)
  music_boss.ogg       -> combate HERCULES-1

Si prefieres .wav (o cualquier otra extension), avisa y se cambia la ruta en el codigo.
