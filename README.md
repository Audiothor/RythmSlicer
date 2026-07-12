# RythmSlicer v1.6.5

**RythmSlicer** est un plugin audio VST3 de découpage rythmique (slicer) interactif et créatif optimisé pour JUCE 8. Il permet de manipuler vos boucles audio (drum loops, breaks) en temps réel, de réorganiser les tranches de manière aléatoire ou avec effet de stutter, et de glisser-déposer le résultat directement dans votre séquenceur (DAW).

![RythmSlicer GUI Mockup](https://raw.githubusercontent.com/Audiothor/RythmSlicer/main/DESCRIPTION.md) *(Note: pour voir l'interface, ouvrez le plugin)*

---

## 🚀 Fonctionnalités Principales

### 1. Deux Algorithmes de Découpage (Slicing)
* **Mode Warp-Synced** : Division rythmique stricte et synchronisée au tempo de votre séquenceur (ex: découpe en croches, doubles-croches). Élimine intelligemment les zones de silence.
* **Mode Transients** : Analyse dynamique de l'enveloppe de volume pour détecter les attaques (kicks, snares, cymbale). La granularité est ajustable via le potentiomètre rotatif **Sensibilité**.

### 2. Interactions Temporelles Directes
* **Déplacement de tranches** : Ajustez à la souris la position des marqueurs de tranches (lignes roses) directement sur le visualiseur.
* **Fusion de tranches** : Double-cliquez sur un marqueur de tranche pour le supprimer et fusionner les tranches adjacentes.

### 3. Effets de Performance Créative
* **Randomisation** : Probabilité de sauter de manière aléatoire vers une autre tranche lors de la lecture pour des variations infinies.
* **Répétition (Stutter)** : Probabilité de répéter la même tranche (effet roll/bégaiement).

### 4. Enregistrement Temps Réel & Glisser-Déposer (WAV Drag & Drop)
* Cliquez sur le bandeau en bas pour démarrer l'enregistrement en temps réel de votre boucle avec tous les effets de stutter ou randomisation.
* Cliquez à nouveau pour arrêter. Le bouton devient vert et indique que le fichier WAV est prêt.
* Glissez le bouton vert directement vers votre DAW pour y déposer la boucle audio.
* Cliquez à nouveau sur le bouton pour écraser et lancer un nouvel enregistrement.

### 5. Aide Utilisateur Intégrée
* Un bouton **"?"** en haut à droite ouvre un overlay d'aide déroulant complet contenant le manuel utilisateur interactif (tiré de `help.md`).

---

## 🛠️ Installation et Compilation

### Prérequis
* **JUCE 8** (Projucer)
* **Visual Studio 2026** (ou version compatible avec MSVC C++)

### Compilation
Le dépôt contient le fichier de configuration universel **`RythmSlicer.jucer`** à la racine, configuré pour exporter les cibles vers Windows et macOS.

#### Windows (VS Code)
Le dépôt contient un fichier `.vscode/tasks.json` configuré pour compiler automatiquement le plugin sous Windows avec MSBuild :
1. Ouvrez le projet dans VS Code.
2. Lancez le build avec `Ctrl+Shift+B` (ou exécutez la tâche `Compiler RythmSlicer (Release)`).
3. Le binaire VST3 compilé est copié dans le dossier `Binaries/RythmSlicer.vst3`.

#### macOS (Xcode)
Le projet est entièrement compatible macOS :
1. Ouvrez le fichier `RythmSlicer.jucer` dans l'application **Projucer** (JUCE 8).
2. Projucer détectera l'exportateur **Xcode (macOS)** déjà configuré. Cliquez sur le bouton d'exportation pour ouvrir le projet dans Xcode.
3. Lancez la compilation (`Cmd + B`) dans Xcode en configuration Release. Le VST3 macOS sera créé dans le dossier de build de Xcode, prêt pour être importé dans Logic Pro, Ableton, etc.

---

## 📝 Licence
Projet distribué sous **Licence MIT** (libre d'utilisation, modification et distribution). Conçu et développé par **AUDIOTHOR - Freeware** & **Antigravity**.

*Note : Les copyrights des frameworks et bibliothèques tierces utilisés (JUCE, Microsoft, Git, Standard C++) sont décrits en fin du guide d'aide (`help.md`).*
