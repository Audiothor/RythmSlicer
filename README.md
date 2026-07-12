# RythmSlicer v1.6.3

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

### 4. Glisser-Déposer Direct (WAV Drag & Drop)
* Glissez le bouton en bas directement vers votre DAW pour y déposer la boucle modifiée au format audio `.wav` haute qualité compilée instantanément en tâche de fond.

### 5. Aide Utilisateur Intégrée
* Un bouton **"?"** en haut à droite ouvre un overlay d'aide déroulant complet contenant le manuel utilisateur interactif (tiré de `help.md`).

---

## 🛠️ Installation et Compilation

### Prérequis
* **JUCE 8** (Projucer)
* **Visual Studio 2026** (ou version compatible avec MSVC C++)

### Compilation avec VS Code
Le dépôt contient un fichier `.vscode/tasks.json` configuré pour compiler automatiquement le plugin sous Windows avec MSBuild :
1. Ouvrez le projet dans VS Code.
2. Lancez le build avec `Ctrl+Shift+B` (ou tâche `Compiler RythmSlicer (Release)`).
3. Le binaire compiled `RythmSlicer.vst3` est automatiquement copié dans le dossier `Binaries/` du projet.

---

## 📝 Licence
Projet sous licence libre (Freeware). Conçu et développé par **ARTSEN** & **Antigravity**.
