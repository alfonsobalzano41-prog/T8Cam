# T8Cam

Plugin audio (AU / VST3) e app che mostra la tua webcam dentro il DAW.
L'audio passa intatto: il plugin lo usa solo per far reagire il video.

**Funzioni**
- Scelta della webcam (anche Continuity Camera dell'iPhone e camere USB)
- Specchio, Bianco e nero
- **Pulse**: l'immagine pulsa a tempo con l'audio della traccia
- Finestra ridimensionabile; le impostazioni si salvano nel progetto

**Formati**
- macOS (Apple Silicon e Intel): AU, VST3, app Standalone
- Windows: VST3, app Standalone

## Installazione su Mac

1. Apri la pagina [Actions](../../actions) (o [Releases](../../releases)), scegli l'ultima build riuscita e scarica `T8Cam-macOS`.
2. Scompatta lo zip e fai doppio clic su `install-mac.command`.
   Se macOS lo blocca: tasto destro → Apri → Apri.
3. Riavvia il DAW.
   - **Logic**: Effetti audio → T8 → T8Cam, su qualsiasi traccia.
   - **FL Studio**: Plugin database → Effects → T8Cam (se non c'è: Options → Manage plugins → Find plugins).

## Permesso webcam su macOS

macOS chiede il permesso della webcam **a nome del DAW**, non del plugin.
Se il DAW non ha dichiarato di usare la webcam, macOS lo chiude non appena un plugin ci prova.
T8Cam controlla prima di aprirla: se il DAW non può usare la webcam non la tocca,
mostra un messaggio e un pulsante **"Apri l'app T8Cam"**, che apre la webcam in una finestra a parte.

## Compilare da soli

Serve CMake 3.22+ e Xcode (Mac) o Visual Studio 2022 (Windows). JUCE viene scaricato in automatico.

```bash
cmake -S . -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release
```

I file compilati finiscono in `build/T8Cam_artefacts/Release/`.

## Struttura

| File | Cosa fa |
|---|---|
| `Source/PluginProcessor.*` | Audio passante, misura del livello, parametri salvati |
| `Source/PluginEditor.*` | Barra in alto (camera, Specchio, B/N, Pulse) |
| `Source/CameraView.*` | Disegna la webcam con gli effetti e i messaggi |
| `Source/CameraPermission_mac.mm` | Permesso webcam e cattura video AVFoundation (macOS) |
| `Source/CameraSource.cpp` | Cattura webcam su Windows (JUCE CameraDevice) |
