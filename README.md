# TapeLAB

> **Professional Cassette Analyzer**  
> Firmware pentru **STM32F401**, dedicat măsurării, calibrării și diagnosticării deck-urilor de casetă și a semnalelor audio analogice.

TapeLAB este un instrument autonom cu ecran tactil care reunește într-un singur aparat funcții de măsurare, analiză și calibrare pentru echipamente audio analogice.

**Proiect inițiat și dezvoltat de Ovidiu – YO6PIR**

---

## Platformă hardware

| Componentă | Alegere |
|---|---|
| Microcontroler | STM32F401RCT6 / STM32F401RCTx |
| Framework | Arduino STM32 Core |
| Display | ILI9341, 2.4", 320 × 240 px |
| Touch | XPT2046 rezistiv |
| Grafică | TFT_eSPI |
| RTC | Modul `Rtc` |
| Stocare | EEPROM I²C |
| Intrări audio | Stereo, ADC, calibrare DC |
| Generator | AD9833 |

---

## Funcții principale

### ☑ Sistem / UI

- Splash Screen și Boot Check real
- Main Menu și navigare Touch
- calibrare Touch în 5 puncte
- salvare/încărcare parametri
- RTC
- System Information / About
- interfață modulară și scenografie unitară

### ☑ Audio Engine

- achiziție audio stereo la aproximativ **49.4 kS/s**
- ADC verificat la boot
- calibrare automată DC Offset
- RMS stereo
- măsurare frecvență audio
- calibrare și corecție de frecvență
- bargraph L/R cu Peak Hold și OVER
- Balance indicator

### ☑ Playback Analyzer

- nivel L/R
- RMS și frecvență
- Peak Hold tip Sony ES
- Smooth Decay / Hold Delay
- OVER
- indicare vârfuri peste 0 dB
- Balance L/R

### ◩ Waveform / Oscilloscope

Prima versiune a Waveform Analyzer a fost funcțională, cu trigger, timebase și măsurare de frecvență.

- eșantionare fixă la 40 kHz
- trigger automat
- timebase audio
- măsurare de frecvență prin media perioadelor

**Stare actuală:** arhitectura veche de Waveform Analyzer este în curs de retragere și înlocuire. Următoarea versiune va folosi o arhitectură de oscilloscope orientată spre stabilitatea trasei, inspirată din proiectul japonez analizat recent (**Japan-TL**). Implementarea nouă nu a fost încă introdusă.

### ☑ Spectrum Analyzer

- FFT CMSIS-DSP
- 256 coloane / 256 bin-uri
- spectru 0…20 kHz
- scară dB și mod log pentru muzică
- 16 benzi
- identificarea peak-ului
- Zoom / recenter pe fereastră de aproximativ 2 kHz
- FFT Analyzer Phase 1–4 finalizate

### ◩ Transport Analysis

- măsurare frecvență cu interpolare
- viteză și eroare de viteză
- deviație instantanee
- Wow & Flutter RMS
- compensarea zgomotului de fond

### ☑ Tape Testing / 2HEAD / 3HEAD

- teste de răspuns în frecvență
- TAPE EQ cu 16 frecvențe
- vizualizare L/R și Combined
- Dolby CHECK pentru OFF / Dolby B / Dolby C
- teste pe configurații 2-head și 3-head
- măsurători RAW ADC 12-bit pentru păstrarea preciziei numerice
- teste de nivel, bias, EQ și S/N

### ◩ ATC — Automatic / Assisted Tape Calibration

Fluxul urmărit este:

**BIAS → EQ → LEVEL**

cu cicluri de tip:

**SYNC → RECORD → END → REWIND → PLAY → FFT**

Firmware-ul oferă măsurarea și direcția de reglaj, iar operatorul face ajustarea mecanică și repetă testul.

### ☑ Generator / calibrare

- generator AD9833
- calibrare prin loopback
- sweep pe benzi de frecvență
- verificare și memorare calibrare
- infrastructură pentru calibrarea nivelurilor

### ☑ Test History

- categorii pentru teste EQ și Dolby
- numerotare și metadata
- istoric de rezultate
- infrastructură cu RAM pools fixe

---

## Istoricul dezvoltării

### Iulie 2026 — Fundația

- STM32F401 + ILI9341 + XPT2046
- TFT_eSPI
- structură modulară
- Splash, Boot, Main Menu
- Touch Calibration
- Audio Engine

### 22–27 iulie 2026 — Primele analizoare

- Playback Analyzer
- Spectrum Analyzer
- primul Waveform Analyzer
- Transport Analysis
- Audio Settings
- calibrare frecvență și validări hardware

### August 2026 — Consolidare

- rafinarea UI și a analizatoarelor
- extinderea măsurătorilor audio
- pregătirea funcțiilor de calibrare și testare a casetelor

### Septembrie 2026 — Tape testing

- separare 2HEAD / 3HEAD
- TAPE EQ și Combined View
- Dolby CHECK OFF / B / C
- S/N și Noise analysis
- loopback calibration
- Test History
- ATC și fluxurile de calibrare
- decizia de trecere de la 24C32 la 24C02 pentru EEPROM

### Octombrie 2026 — FFT și noua arhitectură de oscilloscope

- FFT Analyzer Phase 1 → 4
- spectru 256 coloane, Zoom și recenter
- eliminarea experimentelor de notch/compression care nu erau necesare
- analiza arhitecturii DSO138 și a vechiului oscilloscope japonez
- definirea direcției **Japan-TL** pentru următoarea versiune de Waveform Analyzer
- pregătirea retragerii codului vechi de oscilloscope înaintea implementării noii arhitecturi

---

## Roadmap

| Domeniu | Stare |
|---|---|
| Core / Display / Touch | ☑ Funcțional |
| Audio Engine | ☑ Funcțional |
| Playback Analyzer | ☑ Funcțional |
| Spectrum Analyzer | ☑ Funcțional / în rafinare |
| Transport Analysis | ◩ Funcțional / în rafinare |
| TAPE EQ | ☑ Funcțional |
| Dolby CHECK | ◩ Funcțional / în validare hardware |
| 2HEAD / 3HEAD | ◩ În dezvoltare și validare |
| Generator + loopback calibration | ☑ Funcțional / în validare |
| ATC | ◩ În dezvoltare |
| Waveform / Oscilloscope | ◩ Reproiectare Japan-TL |
| Record Calibration | ☐ În dezvoltare |
| Azimuth Assistant | ☐ Planificat |
| Frequency Response | ☐ În dezvoltare |
| THD | ☐ Planificat |
| Save / Recall Profiles | ☐ În dezvoltare |

---

## Principii de dezvoltare

- funcțiile validate nu se modifică fără motiv măsurabil;
- modulele independente nu sunt modificate inutil;
- rezultatele de măsurare sunt păstrate numeric;
- hardware-ul este validat separat de build-ul software;
- modificările importante sunt urmate de build și verificare de memorie;
- prioritatea este corecția controlată și validarea pe hardware.

---

## Starea proiectului

TapeLAB a evoluat de la o interfață de analiză audio la un instrument complex pentru testarea și calibrarea deck-urilor de casetă, cu subsisteme funcționale pentru Playback, Spectrum, Transport, TAPE EQ, Dolby și calibrare.

În prezent proiectul este în etapa de consolidare și extindere a măsurătorilor, cu două direcții importante: **testarea completă 2HEAD/3HEAD** și **reconstrucția oscilloscope-ului în arhitectura Japan-TL**.

**Proiectul este în dezvoltare activă.**

*Ultima actualizare: 04.10.2026*
