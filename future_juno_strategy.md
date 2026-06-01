# Estrategia de Desarrollo Futuro: Emulación de Juno-60 y Juno-6 (Fork de ABD JUNiO 601)

Este documento detalla el análisis de diferencias entre los sintetizadores Roland Juno-6, Juno-60 y Juno-106, estableciendo una estrategia de desarrollo modular en JUCE 8 para reutilizar el motor de audio actual y crear tres plugins independientes.

---

## 1. Relación de Hardware y Diferencias de Componentes

### Juno-6 vs. Juno-60 (Las diferencias reales)
A nivel de **circuitería analógica de síntesis**, el Juno-6 y el Juno-60 son prácticamente idénticos (mismo filtro VCF basado en el chip **IR3109**, mismo generador de envolventes de hardware **IR3R01** y mismos osciladores discrete DCO). La diferencia radica en la implementación del control:
* **Presets y Memoria:** El Juno-60 añade memoria digital para 56 parches (presets) y una interfaz de control digital que digitaliza los faders. El Juno-6 es puramente directo (*What You See Is What You Hear*), por lo que sus faders son 100% analógicos continuos.
* **HPF (High Pass Filter):** 
  * En el **Juno-60**, el HPF es un conmutador físico de **4 posiciones discretas** (0, 1, 2, 3), donde el paso 0 es un bypass plano.
  * En el **Juno-6**, el HPF es un deslizador **continuo**.
* **Arpegiador:** Ambos modelos comparten exactamente el mismo arpegiador físico.
* **DCB (Digital Communication Bus):** Añadido en el Juno-60 para secuenciación externa, ausente en el Juno-6.

### Juno-60 vs. Juno-106
El Juno-106 introdujo cambios sustanciales para abaratar costes e integrar MIDI:
* **Filtros VCF/VCA:** Se sustituyó el chip IR3109 por los infames chips integrados **80017A** (que sufren de la "enfermedad" de la resina conductiva).
* **HPF:** Ambos tienen 4 posiciones, pero la posición "0" del Juno-106 añade un **realce activo de graves (bass boost)** alrededor de los 40-50 Hz, mientras que el Juno-60 es plano.
* **Envolventes:** El Juno-106 calcula sus envolventes mediante un microcontrolador digital de 8 bits (rampas lineales con ligeros rebotes). El Juno-6/60 utiliza envolventes analógicas generadas por hardware (curvas de carga exponencial RC) mucho más rápidas y percusivas en el ataque (*punch*).
* **Performance:** El Juno-106 elimina el arpegiador e introduce portamento polifónico y modo Unísono.

---

## 2. Estrategia de Código Modular (Motor Común y 3 Front-Ends)

Para evitar duplicar código (lo cual multiplicaría el mantenimiento por tres), la estrategia propuesta consiste en:

1. **Un Motor Común (`shared_synth_core`):**
   * Contiene los osciladores, envolventes duales (modo RC vs. modo digital), filtros intercambiables y el arpegiador.
   * Todos los parámetros (incluidos los de "circuit bending") se exponen en el `AudioProcessorValueTreeState` (APVTS).
2. **Estructura CMake en JUCE 8:**
   * Crear tres targets de compilación separados que enlacen a la biblioteca común pero usen editores de interfaz (`PluginEditor.cpp`) y hojas de especificaciones de hardware distintas.
3. **Control del HPF y Parámetros en el DSP:**
   * **Plugin Juno-106:** Usa tabla de coeficientes de 4 posiciones discretas con bass boost activo en la pos "0".
   * **Plugin Juno-60:** Usa tabla de coeficientes de 4 posiciones discretas sin bass boost (bypass en pos "0").
   * **Plugin Juno-6:** Expone el HPF al DSP de forma continua mapeado al slider de la interfaz.

---

## 3. Inspiración y DSP de Referencia: `ultramaster_kr106`

El proyecto clon [kr106](file:///d:/desarrollos/ABDSynths/synthClones/kr106) tiene gran parte de este trabajo modelado y es una excelente fuente de referencia técnica:

* **Estructura DSP (`Source/DSP/`):**
  * [KR106Voice.h](file:///d:/desarrollos/ABDSynths/synthClones/kr106/Source/DSP/KR106Voice.h): Implementación por voz del VCF (filtro TPT de 4 polos con saturación OTA en cascada y sobremuestreo 2x) y la envolvente dual.
  * [KR106_DSP_SetParam.h](file:///d:/desarrollos/ABDSynths/synthClones/kr106/Source/DSP/KR106_DSP_SetParam.h): Despacho de parámetros según el modelo (curvas J6/J60 vs. J106).
  * [KR106Chorus.h](file:///d:/desarrollos/ABDSynths/synthClones/kr106/Source/DSP/KR106Chorus.h): Simulación de BBD MN3009 con interpolación Hermite de 4 puntos y filtros pre/post verificados en ngspice.
* **Curvas de Envolventes (`KR106ADSR.h`):**
  * Muestra cómo simular la envolvente RC analógica del Juno-6/60 cargando hacia una meta virtual de 1.5 (con umbral de corte en 1.0) frente a la rampa lineal del 106.
* **Estabilidad del Reloj:**
  * Implementa micro-derivas de fase aleatorias (analog drift) para recrear el reloj analógico del Juno-6/60 frente al reloj de cuarzo del 106.

---

## 4. El Cuarto Sintetizador: "Super Juno SIX" (El Concepto Híbrido)

El "Super Juno SIX" se plantea como el sintetizador definitivo de la serie, combinando las mejores características de los tres modelos en un entorno híbrido y personalizable por el usuario.

### Filosofía de Diseño: Modular y Conmutable
En lugar de forzar una configuración fija, la interfaz del "Super Juno SIX" debe dar acceso a conmutadores y controles que permitan mezclar las tecnologías del motor común:

1. **Sección VCF (Filtro):**
   * Selector de modelo de filtro: **Juno-60 (IR3109)** (más brillante, autooscilación chillona, menos pérdida de graves con alta resonancia) vs. **Juno-106 (80017A)** (más suave, cálido).
2. **Sección HPF (Paso Alto):**
   * Deslizador continuo (Juno-6) con un conmutador independiente de **"Bass Boost"** (para aplicar el realce de graves activo del 106).
3. **Sección Chorus:**
   * Selector de carácter de Chorus: **J60** (clásico y orgánico) vs. **J106** (con su característico nivel de ruido y respuesta de impedancia). 
   * Control deslizante de **"BBD Noise / Wear"** para controlar la cantidad de soplido de los chips MN3009 emulados.
4. **Modulación y Control de Voz:**
   * Interruptor para alternar la curva de las envolventes ADSR entre **RC Analógica (J6/60)** y **Digital Linear (J106)**.
   * Potenciómetro de **"Analog Drift"** para controlar la deriva térmica y desfase de los DCOs analógicos (de 0% estable como el 106 a 100% inestable como el Juno-6 original).
5. **Herramientas de Performance Simultáneas:**
   * El Super Juno SIX permite usar el **Arpegiador** (J6/60) y el **Portamento / Unison** (J106) simultáneamente, expandiendo las posibilidades de interpretación que el hardware original limitaba por espacio y costes.

---

## 5. Análisis Comparativo Detallado de Módulos y Fuentes de Código

Para la refactorización y extracción del DSP de referencia, se han mapeado las clases actuales de `ABDJUNiO601` con sus homólogas en `kr106`:

### A. High Pass Filter (HPF)
* **Código Actual:** [JunoHPF.h](file:///d:/desarrollos/ABDSynths/ABDJUNiO601/Source/Synth/JunoHPF.h)
  * Implementa solo el modelo del Juno-106 con 4 posiciones discretas: Bass Boost (pos 0 con biquad bivalente de realce activo), FLAT (pos 1), HPF 236 Hz (pos 2) y HPF 754 Hz (pos 3).
* **Referencia en Clones:** [KR106_HPF.h](file:///d:/desarrollos/ABDSynths/synthClones/kr106/Source/DSP/KR106_HPF.h)
  * **Juno-60:** 4 posiciones (FLAT, 122 Hz, 269 Hz, 571 Hz).
  * **Juno-6:** Deslizador continuo mapeado por interpolación PCHIP de 11 puntos medidos en hardware (de 38.6 Hz a 1394.2 Hz) en `getJuno6HPFFreqPCHIP`.
* **Refactorización:** El motor unificado debe admitir una enumeración `HPFMode { J60, J106, J6Continuous }`. El modelo híbrido "Super Juno SIX" permitirá combinar el slider continuo del Juno-6 con un interruptor de "Bass Boost" del Juno-106.

### B. Generador de Envolventes (ADSR)
* **Código Actual:** [JunoADSR.h](file:///d:/desarrollos/ABDSynths/ABDJUNiO601/Source/Synth/JunoADSR.h) y [JunoADSR.cpp](file:///d:/desarrollos/ABDSynths/ABDJUNiO601/Source/Synth/JunoADSR.cpp)
  * Reproducción estricta del microcontrolador uPD7811G digital del Juno-106, que corre a ticks de 4.23ms e implementa el truncamiento característico del ALU de 8 bits en la multiplicación de envolvente.
* **Referencia en Clones:** [KR106ADSR.h](file:///d:/desarrollos/ABDSynths/synthClones/kr106/Source/DSP/KR106ADSR.h)
  * **Juno-6/60:** Curvas puramente analógicas basadas en el circuito RC del chip IR3R01. El ataque carga hacia una meta virtual de 1.2 (overshoot del comparador) para imitar la curvatura real, y la descarga (Decay/Release) decae hacia -0.1 (undershoot) para un corte preciso. Mapea la posición física del slider a constantes `tau` en segundos mediante tablas medidas (`AttackTauJ6`, `DecRelTauJ6`).
* **Refactorización:** Añadir un interruptor de modelo en la clase `JunoADSR` para conmutar entre el modelo analógico RC (J6/J60) y el modelo digital de 14 bits (J106).

### C. Filtro (VCF)
* **Código Actual:** [JunoVCF.h](file:///d:/desarrollos/ABDSynths/ABDJUNiO601/Source/Synth/JunoVCF.h) y [JunoVCF.cpp](file:///d:/desarrollos/ABDSynths/ABDJUNiO601/Source/Synth/JunoVCF.cpp)
  * ZDF TPT de 4 polos con saturación Padé 3/3 por etapa, ejecutado a la frecuencia de muestreo del host.
* **Referencia en Clones:** [KR106VCF.h](file:///d:/desarrollos/ABDSynths/synthClones/kr106/Source/DSP/KR106VCF.h)
  * Añade sobremuestreo interno (2x/4x) mediante filtros polifase IIR inline de 12 coeficientes (`Upsampler2x` y `Downsampler2x`).
  * Implementa la curva de resonancia del Juno-6 (`ResK_J6` + `SoftClipK` para limitar por encima de k=3.0) además de la del Juno-106 (`ResK_J106`).
  * Inyecta ruido térmico adaptativo (`mNoiseSeed` PRNG) que simula el bias del OTA para excitar la autooscilación limpia.
  * Modela saturación por etapa conmutable (`mOTASaturation`).
* **Refactorización:** Integrar el sobremuestreo polifase y la resonancia/saturación conmutable en el VCF compartido para soportar la respuesta sedosa del IR3109 y la más agresiva del 80017A.

### D. Chorus
* **Código Actual:** [ChorusBBD.h](file:///d:/desarrollos/ABDSynths/ABDJUNiO601/Source/Synth/ChorusBBD.h) y [ChorusBBD.cpp](file:///d:/desarrollos/ABDSynths/ABDJUNiO601/Source/Synth/ChorusBBD.cpp)
  * Línea de retardo física MN3009 con clicks de conmutación, leak noise y mains ripple.
* **Referencia en Clones:** [KR106Chorus.h](file:///d:/desarrollos/ABDSynths/synthClones/kr106/Source/DSP/KR106Chorus.h)
  * Calibración precisa del sumador inversor IC6 (`kDryGain = 0.863f` y `kWetGain = 1.257f`).
  * Modulación de ganancia por transferencia de carga (CTE Loss) según la frecuencia de reloj.
  * Desafinación del reloj BBD (`kBBDClockTrim = 0.015f`) para evitar cancelaciones artificiales en mono.
  * Resonador de baja frecuencia `ClickRing` (SVF Chamberlin sintonizado a 30Hz con Q=18) para la oscilación posterior a los clicks.
* **Refactorización:** Refinar el chorus actual importando el modelado físico de pérdidas y desafinaciones de reloj de `kr106`.

### E. DCO / Curvas de Modulación y Drift
* **Código Actual:** [JunoDCO.h](file:///d:/desarrollos/ABDSynths/ABDJUNiO601/Source/Synth/JunoDCO.h) y [JunoVoice.cpp](file:///d:/desarrollos/ABDSynths/ABDJUNiO601/Source/Synth/JunoVoice.cpp)
  * Generación de formas de onda básicas con un modelo de drift lineal.
* **Referencia en Clones:** [KR106Voice.h](file:///d:/desarrollos/ABDSynths/synthClones/kr106/Source/DSP/KR106Voice.h)
  * Tablas de mapeo físico para sliders analógicos (`dcoLfoDepth6`, `vcfLfoDepth6`, `vcfEnvDepth6`) que imitan el comportamiento logarítmico de los potenciómetros de carbono del Juno-6/60 frente al comportamiento del DAC del 106.
  * Modelo de deriva térmica (Analog Drift) de 3 osciladores de caminata aleatoria (`mWalkPhase` / `mWalkValue`) que reproduce el "desafinamiento orgánico".

---

## 6. Arpegiador y Modos de Voz (Unísono y Portamento)

### Arpegiador
* **Origen de Código:** [KR106Arpeggiator.h](file:///d:/desarrollos/ABDSynths/synthClones/kr106/Source/DSP/KR106Arpeggiator.h)
* **Comportamiento:** El Juno-6/60 cuenta con un arpegiador físico con modos (Up, Down, Up/Down) y rangos (1, 2 o 3 octavas). El rate se controla mediante un oscilador de relajación Schmitt-trigger (`arpRate`).
* **Implementación:** Se añadirá una clase `JunoArpeggiator` al motor común. Esta clase interceptará los eventos MIDI entrantes en el buffer de bloque antes de distribuirlos a las voces, disparando notas con un re-trigger estricto de compuerta (gate) para forzar el re-ataque de las envolventes.
* **Híbrido Super Juno:** El "Super Juno SIX" permitirá activar simultáneamente el Arpegiador y el Portamento/Unísono polifónico del Juno-106, algo imposible en el hardware original por límites de CPU/arquitectura.

### Modos de Asignación de Voz y Portamento
* **Comportamiento:**
  * El **Juno-106** dispone de portamento polifónico (glissando lineal en semitones por segundo mapeado en `portaRate`) y modo **Unísono** (apilando las 6 voces con desafinación).
  * El **Juno-6/60** asigna las voces de forma rotativa directa pero carece de unísono de fábrica y portamento.
* **Refactorización:** El asignador de voces (`VoiceAllocator` en el core) soportará asignación rotativa estándar, Unísono configurable y Portamento. En los front-ends de Juno-6 y Juno-60, los controles de Unison/Portamento se deshabilitarán y ocultarán para respetar la fidelidad histórica, manteniéndose activos en el Super Juno.

---

## 7. Gestión de SysEx y Control Digital

### Distinción por Modelo
1. **Juno-106:** Bidireccional completo. Utiliza mensajes de cambio de parámetro individual (IPR - 0x32) y reporte completo (APR - 0x30/0x31) en base a su protocolo SysEx nativo de 18 bytes.
2. **Juno-60:** Carece de MIDI nativo (utiliza el bus de comunicación digital DCB). En nuestro software, emularemos un puente digital clásico: el plugin responderá y enviará volcados de presets en formato SysEx del Juno-60 adaptado, mapeando sus 56 parches.
3. **Juno-6:** Es puramente analógico y no tiene presets ni SysEx en hardware. Sin embargo, para permitir la automatización y el guardado de estado en el DAW, el plugin del Juno-6 utilizará internamente el mismo mapa de parámetros SysEx del Juno-60, permitiendo integrarlo en sistemas de control MIDI externos.

---

## 8. Bancos de Presets y Conversión Dinámica

### Formato y Estructura
* Los presets del Juno-106 utilizan bloques de parámetros empaquetados de 18 bytes (donde varios interruptores se combinan en bits específicos dentro de dos bytes `SW1` y `SW2`).
* Los presets del Juno-60 tienen una distribución de parámetros diferente debido a la ausencia de ciertos controles del 106 (como portamento) y la presencia del arpegiador.

### Conversión de Parámetros
El motor común incluirá un convertidor de presets bidireccional:
* **Mapeo de VCF HPF:** 
  * Al pasar de Juno-106 a Juno-60, la posición `0` (Bass Boost del 106) se convertirá a la posición `0` del 60 (FLAT), pero activará un modificador de calibración interno para añadir el realce si se corre en modo híbrido. Las posiciones de corte (236Hz y 754Hz) se re-mapearán a los valores más cercanos del Juno-60 (269Hz y 571Hz) para preservar la brillantez relativa del parche.
* **Mapeo de Sub-oscilador:**
  * El valor lineal del DAC de sub-oscilador del 106 se re-escalará mediante la curva inversa del diodo shunt del Juno-60 para mantener el balance de volumen percibido.
* **Mapeo de Envolventes:**
  * Los tiempos se mapearán directamente usando sus respectivos sliders (0-127). El motor interno adaptará automáticamente las curvas de tiempo (`tau` exponencial vs incrementos lineales por tick).

---

## 9. Arquitectura de Calibración Dinámica (Dynamic Calibration Profiles)

Para evitar refactorizaciones y lograr que la configuración del "circuit bending" escale desde el principio, el motor compartirá un superconjunto unificado de parámetros de calibración expuestos en el `AudioProcessorValueTreeState` (APVTS).

### Definición Basada en Datos
Cada modelo de sintetizador cargará un `CalibrationProfile` que contiene la base de datos de comportamiento:
```cpp
struct CalibrationLimits {
    float minVal;
    float maxVal;
    float defaultVal;
    bool isExposed; // Controla la visibilidad del parámetro en la UI del modelo activo
};

struct CalibrationProfile {
    juce::String modelName;
    std::unordered_map<juce::String, CalibrationLimits> parameters;
};
```

### Micro-afinaciones y Parámetros Nuevos a Añadir
El sistema de calibración en `CalibrationSettings` actual se ampliará para soportar las siguientes micro-afinaciones dinámicas según el modelo activo:
* `vcfResKModel`: Conmutable entre `0` (curva IR3109) y `1` (curva 80017A).
* `bbdHissColoration`: Controla el realce de agudos/graves del soplido del Chorus.
* `bbdClockTrim`: Tolerancia de desajuste entre relojes BBD (para calibrar la anchura estéreo del Chorus).
* `driftWalkIntensity`: Factor de deriva térmica dinámica (caminata aleatoria de pitch).
* `arpClockRelaxation`: Coeficientes de histéresis del oscilador Schmitt-trigger del arpegiador.

### Interfaz Dinámica (`CalibrationPanel`)
El panel de la interfaz gráfica se generará de manera dinámica preguntando al procesador qué parámetros tienen `isExposed == true` para el perfil actual. De esta manera, el panel renderizará y enlazará automáticamente los sliders necesarios al APVTS mediante bucles dinámicos, eliminando la necesidad de programar layouts individuales para el menú de configuración de cada sintetizador.

---

## 10. Ruta de Desarrollo Incremental (Paso a Paso)

Para garantizar un código estable y minimizar la complejidad durante el desarrollo, la ruta recomendada es construir de **adentro hacia afuera**, utilizando el **Super Juno SIX como banco de pruebas inicial**:

```
[1. Desarrollo Iterativo en Super Juno]
   Diseñar el súper-motor de audio
   --> Implementar Filtro VCF conmutable (IR3109 vs. 80017A) y probar en UI.
   --> Implementar Envolvente dual (RC vs. Lineal) con selector en UI.
   --> Implementar Variación del HPF (Fijo + Boost vs. Continuo) y Chorus.
   --> Probar la integración de Arpegiador y Portamento simultáneos.
   
[2. Creación de los Modelos Específicos (Heredados)]
   Derivar los 3 plugins específicos cerrando parámetros en el Core:
   --> Juno-106: Selectores ocultos (modo 106 de fábrica), arpegiador deshabilitado.
   --> Juno-60: Selectores ocultos (modo 60 de fábrica), arpegiador activo.
   --> Juno-6: Igual al 60 pero ocultando sección de Presets y cambiando HPF a continuo.
```

---

## 11. Evolución del Sistema de Skins (Personalización Visual)

El sistema de skins actual se ampliará para adaptarse dinámicamente a las variaciones estéticas de cada modelo (colores, estilo de botones y serigrafía) sin romper el motor interno:

* **Separación Visual:** Cada plugin instanciará su respectivo `PluginEditor` aplicando la skin que corresponda de fábrica por modelo.
* **Carga Cruzada Opcional:** Dado que el Super Juno admite configuraciones híbridas, se mantendrá la posibilidad de cargar de manera cruzada la skin visual del Juno-6 en el motor del Juno-106, o viceversa, como una característica estética adicional y divertida dentro de las opciones generales del usuario.

---

## 12. Identidades de Compilación y Naming

Para facilitar la distribución de compilaciones individualizadas, el código soporta una macro centralizada `JUNO_TARGET_MODEL` (en `Source/Core/JunoModelConfig.h`) que conmuta el nombre y los textos explicativos en la interfaz de forma dinámica:
* **`JUNO_TARGET_MODEL = 0`:** `ABD JUNiO Super SIX` (El sintetizador híbrido/modular definitivo).
* **`JUNO_TARGET_MODEL = 1`:** `ABD JUNiO 601` (Emulación del Juno-106).
* **`JUNO_TARGET_MODEL = 2`:** `ABD JUNiO 06` (Emulación del Juno-60).
* **`JUNO_TARGET_MODEL = 3`:** `ABD JUNiO SIX` (Emulación del Juno-6).

Esta variable actualiza en caliente el título de la WebUI, la pantalla de carga (*splash*), el cuadro de información (*About*), la barra superior y los menús desplegables.

---

## 13. Avances Completados (Fase 1: Motor Común y Super Juno SIX)

Se han culminado todos los desarrollos de la Fase 1 en el banco de pruebas "Super Juno SIX":
1. **Refactorización del HPF:** Integración de modos discretos (J60/J106) e interpolación continua PCHIP (J6).
2. **Modelo ADSR RC Analógico:** Integración de envolventes RC exponenciales veloces con overshoot a `1.2f` y undershoot a `-0.1f` conmutables frente al modelo lineal uPD7811G.
3. **Optimización de VCF:** Sobremuestreo polifase IIR inline (1x/2x/4x), inyección de ruido térmico aleatorio para autooscilación desde silencio y selector de curvas de resonancia IR3109 vs. 80017A.
4. **Perfeccionamiento de Chorus:** Integración del filtro svf `ClickRing` (30Hz), ganancias del sumador inversor IC6 y CTE Loss.
5. **Arpegiador Integrado:** Motor de arpegio sincronizado a tempo de host o libre (free run) y completamente funcional.
6. **Ruteo Modular y Presets:** Panel WebUI dedicado en la pestaña **ROUTING** para seleccionar el modelo por sección (DCO, HPF, VCF, ADSR, Chorus, Arp, Poly), guardándose esta configuración de forma persistente dentro de cada preset.
7. **Personalización Dinámica de Nombre:** Bridge de intercomunicación que propaga el nombre del producto según la compilación a toda la interfaz WebUI.
8. **Independencia de Portamento y Unísono:** Separados por completo para admitir cualquier combinación en el asignador de voces, eliminando acoplamientos rígidos heredados.
9. **Perfiles de Calibración Dinámicos:** Selector `calibrationProfile` en la WebUI para cargar al vuelo calibraciones de fábrica auténticas (Juno-6, Juno-60 o Juno-106) incluyendo ganancias del DCO, curvas de VCA y HPF.
10. **Alineación de Frecuencias HPF:** Se añadió el parámetro de calibración de HPF Pos 1 (`hpfFreq1`) para modelar correctamente los 122 Hz del Juno-60 en contraste con el bypass del Juno-106.
11. **Salvaguardas e Importación Inteligente:** Introducción de un bloque de metadatos `__metadata__` en las calibraciones exportadas en JSON. Valida compatibilidad al importar para proteger binarios específicos y realiza auto-conmutación de perfil en el motor híbrido.

12. **Temas Gráficos e Imágenes Laterales (Skins):** Se diseñaron los skins dedicados para Juno-60 (madera cerezo), Juno-6 (madera caoba), Juno-106 (metal) y Classic Blue (Super Six) utilizando paneles laterales de imagen real cargados dinámicamente mediante pseudo-elementos absolutos en `#synth-app`, con el chasis dinámico adaptado a la variable de color `--bg-synth`.
13. **Asociación al Perfil y Auto-Sincronización:** El cambio de perfil de calibración global (`calibrationProfile`) actualiza instantáneamente el tema gráfico del chasis (`skinType`), sincronizando la era visual de forma transparente.
14. **Placas de Componente (Cinta de Pintor):** Las cintas adhesivas con indicación del modelo activo en cada módulo se subieron a `bottom: 6px` y `z-index: 101` para evitar solapamiento con el teclado, aplicando colores diferenciados por modelo (beige J106, ocre J6 y blanco roto J60).
15. **Mejoras de Layout y Controles del Frontal:** Distribución y alineamiento vertical/horizontal del Portamento, integración de botones ON/OFF e interruptor Sync del Arpegiador sin solapamientos, e implementación de la Opción C de controles de Arpegio (tres knobs alineados con etiquetas de texto rotuladas alrededor de cada selector discreto que se iluminan al seleccionarse).

---

## 14. Próximos Pasos: Persistencia y Ocultación en Específicos

### A. Persistencia y Gestión de Presets
* **Guardado Selectivo:** El sistema de guardado de presets en `PresetManager` y el ValueTree almacenará de forma robusta las asignaciones específicas de ruteo modular (`modelDCO`, `modelHPF`, etc.) sin necesidad de volcar la base de datos completa de calibración, manteniendo los archivos de presets ligeros e interoperables.

### B. Restricciones Físicas y Ocultación en Modelos Específicos
* **Ocultación Dinámica:** En las compilaciones individuales de Juno-6, Juno-60 y Juno-106, se deshabilitarán y ocultarán los selectores modulares en cascada y las pestañas redundantes, respetando los límites de hardware correspondientes a cada modelo.


