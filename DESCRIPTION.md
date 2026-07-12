# Fiche d'Identité & Architecture - RythmSlicer VST3

Ce document sert de guide de contexte système pour les agents d'IA (comme Gemini) travaillant sur le projet **RythmSlicer**. Il décrit l'architecture du code, les technologies utilisées et les règles de développement.

---

## 1. Vue d'Ensemble du Projet

* **Nom officiel :** RythmSlicer
* **Créateur / Éditeur :** ARTSEN
* **Version actuelle :** v1.1.0 (Édition Performance)
* **Technologie :** C++ (norme C++20), Framework JUCE 8.0.14
* **Formats cibles :** Plugin VST3 (64-bit) & Application Autonome (Standalone)
* **Design visuel :** Cyberpunk Sombre & Néon (Cyan électrique `#00E5FF` et Rose fluo `#FFFF006E`)

---

## 2. Fonctionnalités Clés

* **Slicing Temporel Warp-Synced :** Découpe automatique d'un sample audio importé (WAV/AIFF) selon une grille de subdivisions rythmiques définies par l'hôte ($1\text{ Bar}$ jusqu'à $1/32$ de note).
* **Audition Interactive :** Sélection visuelle d'une tranche au clic sur la forme d'onde, avec écoute isolée ("Écouter Tranche") ou enchaînée ("Écouter la Suite").
* **Synchronisation d'Hôte (SYNC DAW) :** Suivi en temps réel du tempo (BPM) et de l'état de lecture (Play/Pause) du séquenceur (Ableton, FL Studio, etc.).
* **Moteur d'Accidents Aléatoires (DSP) :**
  * **Randomisation (0% à 100%) :** Probabilité de perturber l'ordre chronologique des tranches.
  * **Répétition (Stutter - 0% à 100%) :** Probabilité de forcer la répétition (bouclage) de la tranche en cours.
* **Anti-Clics Numériques :** Algorithme de micro-fades de transition (fondu d'entrée/sortie de $5\text{ ms}$) appliqué lors des sauts temporels d'une tranche à l'autre pour éliminer les pop audio.
* **Drag & Drop Direct-to-DAW (WAV Export) :** Rendu offline de la boucle rythmique altérée et génération d'un fichier WAV temporaire permettant à l'utilisateur de glisser-déposer le résultat directement dans les pistes de son DAW.

---

## 3. Structure du Code Source (`RythmSlicer.cpp`)

Le projet est intentionnellement structuré sous la forme d'un fichier C++ unique et autonome pour simplifier la portabilité et le travail des agents d'IA. Il est divisé en trois sections majeures :

### A. Composants Graphiques Auxiliaires
* **`ModernKnobLookAndFeel` :** Hérite de `juce::LookAndFeel_V4`. Redessine de façon vectorielle les boutons rotatifs sous forme de jauges néon cyberpunk de style "donut" avec curseurs blancs fins.
* **`WavDragComponent` :** Gère l'interaction de souris pour déclencher l'export offline et lance le protocole système `juce::DragAndDropContainer::performExternalDragDropOfFiles` pour le glisser-déposer vers le DAW.

### B. Le Processeur Audio (`SlicerAudioProcessor`)
* **DSP & Threads :** Utilise un `juce::SpinLock` (`bufferLock`) pour garantir un accès thread-safe ultra-rapide aux données du fichier audio sans bloquer le thread de traitement temps réel.
* **Filtre de Sécurité :** Limite le chargement des fichiers audio à une durée maximale de 300 secondes (5 minutes) pour protéger la mémoire vive.
* **`processBlock` :** Gère la synchronisation du playhead de l'hôte, le calcul de probabilité d'enchaînement des tranches, et applique les micro-fades d'anti-clic.
* **Render Engine (`renderLoopToWavFile`) :** Recrée un sous-système de traitement "offline" simulant le comportement probabiliste du plugin pour compiler une boucle WAV figée et calée au BPM actuel.

### C. L'Éditeur Graphique (`SlicerAudioProcessorEditor`)
Construit l'interface utilisateur, attache les curseurs et sélecteurs de grille aux paramètres DSP via `juce::AudioProcessorValueTreeState::SliderAttachment` (gérant l'automation et la sauvegarde d'état).
* **`WaveformVisualizer` :** Composant interne qui dessine la forme d'onde et gère la surbrillance de la tranche active/sélectionnée.

---

## 4. Règles Cruciales pour les Agents IA de Modification

### Compatibilité JUCE 8
* Ne jamais utiliser `layouts.getMainOutput()`. Utiliser obligatoirement `layouts.getMainOutputChannelSet()`.
* Ne jamais instancier `juce::Font` avec des tailles directes obsolètes. Passer systématiquement par `juce::FontOptions`.
* Ne jamais utiliser l'ancienne sérialisation XML `getXmlSerializerForUtils`. Préférer `juce::copyXmlToBinary`.

### Silence MSVC (Zéro Warning)
* Toutes les variables de fonctions d'API non utilisées doivent être neutralisées à l'aide de `juce::ignoreUnused(nomVariable)`.

### Thread-Safety
* Ne jamais modifier ou réallouer `fileBuffer` en dehors de la protection du `bufferLock`.