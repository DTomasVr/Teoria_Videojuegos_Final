SFX del juego (SDL3_mixer, API MIX_*).

Deja aqui tus efectos de sonido en .wav (u otros formatos que soporte SDL3_mixer).
El motor los carga PREDESCODIFICADOS con Audio::load(nombre, ruta) y los dispara con
Audio::play(nombre). Si un archivo no existe, el juego corre igual pero sin ese sonido.

Nombres que ya usa la escena de prueba (Bullet Hell, tecla 4):
  death.wav  -> al morir el clon
  dash.wav   -> al hacer el Dash

Puedes anadir mas (disparo de torreta, siseo del lanzallamas, etc.) segun el GDD 8.2.
