# T8Cam

Plugin audio (AU / VST3) e app che mostra la tua webcam dentro il DAW, con effetti video,
scritte, immagini e GIF animate sopra il video, e preset.
L'audio passa intatto: il plugin lo usa solo per far reagire il video.

## Funzioni

- **Webcam**: scelta della camera (anche Continuity Camera dell'iPhone e camere USB), specchio.
- **Effetti** (menu a tendina + Intensità): Bianco e nero, Negativo, Pop art, Termico, Visione notturna,
  Glitch, Schizzo, Neon, Fumetto, Duotone, Caleidoscopio. Glitch reagisce anche all'audio.
- **Pulse**: l'immagine pulsa a tempo con l'audio della traccia.
- **Nascondi UI**: lascia solo il video, pulito. Per far tornare l'interfaccia: doppio clic sul video,
  oppure il pulsante "Mostra UI" che compare in alto a destra quando passi col mouse.
- **Media** (finestra separata): scritte, PNG, JPG e GIF animate sopra il video.
  - Trascina gli elementi sul video per spostarli; rotella o pinch del trackpad per ingrandirli.
  - Puoi trascinare i file direttamente dal Finder sul video.
  - Per le scritte: testo su più righe, carattere, colore, grassetto, ombra.
  - Per tutti: dimensione, opacità, rotazione, visibile, ordine (Su/Giù), duplica, elimina.
- **Preset**: 14 preset già pronti + i tuoi. "Salva" memorizza effetto, intensità, specchio, pulse e tutti i media.
  I preset di fabbrica senza scritte cambiano solo il look e lasciano i tuoi media dove sono.
- Tutto viene salvato anche nel progetto del DAW.

**Formati**
- macOS (Apple Silicon e Intel): AU, VST3, app Standalone
- Windows: VST3, app Standalone

## Installazione su Mac

1. Scarica `T8Cam-macOS.zip` dall'ultima [Release](../../releases/latest).
2. Scompatta lo zip e fai doppio clic su `install-mac.command`.
   Se macOS lo blocca: tasto destro → Apri → Apri.
3. Riavvia il DAW.
   - **Logic**: Audio FX → T8 → T8Cam, su qualsiasi traccia.
   - **FL Studio**: cerca T8Cam tra gli effetti (se non c'è: Options → Manage plugins → Find plugins).

## Aggiornamenti

Ogni nuova versione esce come **Release** su GitHub, con lo zip per Mac e quello per Windows.
Il plugin controlla da solo (al massimo ogni 12 ore) se c'è una versione nuova: se sì, compare
un avviso in basso con il pulsante **Scarica**. Puoi anche controllare a mano da **Menu → Controlla aggiornamenti**.

Per aggiornare: scarica lo zip, rilancia `install-mac.command` (sostituisce la versione vecchia) e riavvia il DAW.
I tuoi preset non vengono toccati.

### Pubblicare una nuova versione (per chi sviluppa)

1. Alza il numero in `CMakeLists.txt` (`project(T8Cam VERSION x.y.z ...)`).
2. Crea e pubblica un tag con lo stesso numero:
   ```bash
   git tag v0.3.0
   ```
   ```bash
   git push origin v0.3.0
   ```
3. GitHub Actions compila Mac e Windows e crea la Release con gli zip. I plugin già installati se ne accorgono da soli.

## Dove stanno i preset

- Mac: `~/Library/Application Support/T8Cam/Presets/` (file `.t8cam`, si possono copiare e condividere)
- Windows: `%APPDATA%\T8Cam\Presets\`

Menu → "Apri la cartella dei preset" apre direttamente la cartella.

## Permesso webcam su macOS

macOS chiede il permesso della webcam **a nome del DAW**, non del plugin.
Se il DAW non ha dichiarato di usare la webcam, macOS lo chiude non appena un plugin ci prova.
T8Cam controlla prima di aprirla: se il DAW non può usare la webcam non la tocca,
mostra un messaggio e un pulsante **"Apri l'app T8Cam"**, che apre la webcam in una finestra a parte.

## Compilare da soli

Serve CMake 3.22+ e Xcode (Mac) o Visual Studio 2022 (Windows). JUCE viene scaricato in automatico.

```bash
cmake -S . -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
```
```bash
cmake --build build --config Release
```

I file compilati finiscono in `build/T8Cam_artefacts/Release/`.

## Struttura

| File | Cosa fa |
|---|---|
| `Source/PluginProcessor.*` | Audio passante, livello, parametri e media salvati nel progetto |
| `Source/PluginEditor.*` | Barra in alto, modalità pulita, avviso aggiornamenti |
| `Source/CameraView.*` | Video con effetti, media trascinabili, file trascinati dal Finder |
| `Source/VideoEffects.*` | Gli effetti video |
| `Source/MediaModel.*` | Scritte e immagini: dati, disegno, posizione |
| `Source/MediaPanel.*` | Finestra "Media" |
| `Source/GifDecoder.*` | Lettore GIF animate (JUCE legge solo il primo fotogramma) |
| `Source/Presets.*` | Preset di fabbrica e preset dell'utente |
| `Source/UpdateChecker.*` | Controllo nuove versioni su GitHub |
| `Source/CameraPermission_mac.mm` | Permesso webcam e cattura video AVFoundation (macOS) |
| `Source/CameraSource.cpp` | Cattura webcam su Windows (JUCE CameraDevice) |
