# RythmSlicer v1.6.3 - Guide d'Utilisation Officiel

Bienvenue dans l'aide intégrée de **RythmSlicer**, le plugin de découpage audio interactif conçu pour dynamiser vos flux de production de drum loops et d'échantillons rythmiques.

---

## 1. Fonctionnalités Principales

### 1.1 Chargement de Sample
* **Bouton "Charger un Sample"** : Ouvre un explorateur de fichiers pour charger un fichier audio (`.wav`, `.aif`, `.aiff`).
* **Glisser-Déposer Direct** : Vous pouvez glisser un fichier audio directement depuis votre explorateur de fichiers ou votre DAW (comme Ableton Live, FL Studio) sur l'interface du visualiseur de forme d'onde.
* **Sécurité** : Pour préserver les performances en temps réel, les fichiers de plus de 5 minutes sont automatiquement rejetés.

### 1.2 Lecture & Contrôles de Transport
* `⏮` **Play from 0** : Lance la lecture à partir du tout début du sample (échantillon 0).
* `▶` **Play** : Reprend la lecture à l'endroit précis où se trouve actuellement le curseur de lecture.
* `⏸` **Stop / Pause** : Arrête instantanément la lecture et conserve la position actuelle du curseur de lecture.
* **SYNC DAW** : Synchronise automatiquement le tempo et le démarrage du plugin avec le transport de votre séquenceur (DAW). 
  * *Note : Cliquer sur un bouton de lecture manuelle (`▶` ou `⏮`) désactivera automatiquement la synchronisation pour vous donner le contrôle interne.*

---

## 2. Les Modes de Découpage (Slicing)

RythmSlicer vous propose deux algorithmes distincts de génération automatique de tranches :

### 2.1 Mode : Warp-Synced (Grille Rythmique)
Ce mode divise mathématiquement votre boucle en tranches de tailles égales calées sur le tempo.
* **Grille (Subdivision)** : Choisissez la résolution de découpe (de 1 Mesure à la triple-croche `1/32`).
* **Beats (Temps)** : Spécifiez la longueur totale de votre boucle en temps (par défaut 16 temps / 4 mesures).
* **BPM d'Origine** : Ajustez le tempo d'origine du sample pour caler précisément la grille.
* **Filtrage intelligent** : Le plugin détecte automatiquement les zones de silence total (ex: à la fin du fichier) et n'y place aucun marqueur inutile.

### 2.2 Mode : Transients (Détection d'Attaques)
Ce mode analyse le signal audio pour détecter les impacts physiques (les attaques de batterie) et y poser les marqueeurs de découpe.
* **Sensibilité (Transients)** : Potentiomètre rotatif réglant la finesse de la détection.
  * *Sensibilité Haute (ex: 80-100%)* : Détecte le moindre petit bruit, idéal pour isoler les ghost notes et les charleys.
  * *Sensibilité Basse (ex: 10-30%)* : Ne garde que les coups puissants (Kicks, Snares principaux).
* Les options de grille et de beats sont automatiquement masquées dans ce mode pour simplifier l'interface.

---

## 3. Interactions Directes sur la Forme d'Onde

Vous pouvez affiner et éditer manuellement vos tranches directement sur le visualiseur :
* **Déplacement de Marqueur** : Cliquez sur une ligne de découpe rose (le curseur change en icône de redimensionnement gauche-droite) et glissez-la pour ajuster précisément le début d'une tranche.
* **Suppression de Marqueur** : Double-cliquez sur une ligne rose pour la faire disparaître. Cela fusionne instantanément les deux tranches adjacentes.

---

## 4. Effets de Performance Créative

Ces effets modifient l'ordre de lecture des tranches pour créer de la variation en temps réel :

### 4.1 Randomisation (0% à 100%)
* À la fin de chaque tranche, ce potentiomètre définit la probabilité de sauter vers une autre tranche complètement au hasard au lieu de lire la suivante.
* *Exemple : À 50%, vous obtenez une boucle à moitié ordonnée et à moitié réarrangée aléatoirement.*

### 4.2 Répétition (Stutter) (0% à 100%)
* Définit la probabilité qu'une même tranche se répète (effet de bégaiement ou "stutter") au lieu de passer à la suite.
* *Exemple : Idéal pour créer des roulements de caisse claire en fin de boucle.*

---

## 5. Export vers le DAW (Drag & Drop WAV)

* **Bandeau "CLIQUEZ & GLISSEZ ICI VERS LE DAW (WAV)"** : 
  * Cliquez sur ce bouton, maintenez le clic enfoncé et glissez-le vers une piste audio de votre DAW (Ableton, FL Studio, Reaper, etc.).
  * RythmSlicer compile en arrière-plan le sample découpé avec vos réglages actuels (incluant le Stutter et la Randomisation) et génère un nouveau fichier `.wav` propre directement importé sur votre piste !
