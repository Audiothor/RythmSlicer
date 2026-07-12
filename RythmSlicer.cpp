/**
 * ==============================================================================
 * RythmSlicer.cpp
 * Version 1.6.3 - Optimisé pour JUCE 8
 * Design par ARTSEN - Version v1.6.3 (Édition Performance)
 * Intègre le moteur de synchronisation d'hôte (DAW), des potentiomètres
 * graphiques personnalisés et le moteur de glisser-déposer de rendu WAV direct
 * vers le DAW.
 * ==============================================================================
 */

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

// ID des paramètres du plugin
#define GRID_ID "grid"
#define GRID_NAME "Subdivision"
#define BEATS_ID "beats"
#define BEATS_NAME "Nombre de Beats"
#define BPM_ID "bpm"
#define BPM_NAME "BPM d'Origine"
#define RANDOM_ID "random"
#define RANDOM_NAME "Randomisation"
#define REPEAT_ID "repeat"
#define REPEAT_NAME "Repetition"
#define SYNC_ID "sync"
#define SYNC_NAME "Host Sync"
#define MODE_ID "slicingMode"
#define MODE_NAME "Slicing Mode"
#define SENSITIVITY_ID "sensitivity"
#define SENSITIVITY_NAME "Sensibilite"

// Limite de taille de sample de sécurité : 5 minutes (300 secondes)
#define MAX_SAMPLE_DURATION_SECONDS 300.0

// ==============================================================================
// CUSTOM LOOK AND FEEL POUR LES POTENTIOMÈTRES NEON CYBERPUNK
// ==============================================================================
class ModernKnobLookAndFeel : public juce::LookAndFeel_V4 {
public:
  ModernKnobLookAndFeel(juce::Colour activeAccent)
      : accentColour(activeAccent) {}

  void drawRotarySlider(juce::Graphics &g, int x, int y, int width, int height,
                        float sliderPosProportional, float rotaryStartAngle,
                        float rotaryEndAngle,
                        juce::Slider & /*slider*/) override {
    auto bounds =
        juce::Rectangle<int>(x, y, width, height).toFloat().reduced(12);
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    auto toX = bounds.getCentreX();
    auto toY = bounds.getCentreY();
    auto ux = toX - radius;
    auto uy = toY - radius;
    auto widthF = radius * 2.0f;

    // Cercle de fond du potentiomètre
    g.setColour(juce::Colour(0xFF1E1E24));
    g.fillEllipse(ux, uy, widthF, widthF);
    g.setColour(juce::Colour(0xFF33333F));
    g.drawEllipse(ux, uy, widthF, widthF, 2.0f);

    // Arc actif coloré
    auto angle = rotaryStartAngle +
                 sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    juce::Path arcPath;
    arcPath.addCentredArc(toX, toY, radius - 5.0f, radius - 5.0f, 0.0f,
                          rotaryStartAngle, angle, true);
    g.setColour(accentColour);
    g.strokePath(arcPath,
                 juce::PathStrokeType(5.0f, juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));

    // Index/Pointeur blanc élégant
    juce::Path pointer;
    pointer.startNewSubPath(toX, toY);
    pointer.lineTo(toX + (radius - 10.0f) * std::sin(angle),
                   toY - (radius - 10.0f) * std::cos(angle));
    g.setColour(juce::Colours::white);
    g.strokePath(pointer,
                 juce::PathStrokeType(3.0f, juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));
  }

private:
  juce::Colour accentColour;
};

// ==============================================================================
// COMPOSANT INTERACTIF DE GLISSER-DÉPOSER DE WAV POUR LE DAW
// ==============================================================================
class WavDragComponent : public juce::Component {
public:
  WavDragComponent(std::function<juce::File()> renderCallback)
      : onRenderTrigger(renderCallback) {}

  void paint(juce::Graphics &g) override {
    auto bounds = getLocalBounds().toFloat();

    // Couleur de fond dynamique au survol
    g.setColour(isMouseOver ? juce::Colour(0x2200E5FF)
                            : juce::Colour(0xFF111115));
    g.fillRoundedRectangle(bounds, 6.0f);

    g.setColour(isMouseOver ? juce::Colour(0xFF00E5FF)
                            : juce::Colour(0xFF2D2D3A));
    g.drawRoundedRectangle(bounds, 6.0f, 1.5f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));

    g.drawText("CLIQUEZ & GLISSEZ ICI VERS LE DAW (WAV)", getLocalBounds(),
               juce::Justification::centred);
  }

  void mouseEnter(const juce::MouseEvent &) override {
    isMouseOver = true;
    repaint();
  }
  void mouseExit(const juce::MouseEvent &) override {
    isMouseOver = false;
    repaint();
  }

  void mouseDrag(const juce::MouseEvent &e) override {
    if (!hasDragged && e.getDistanceFromDragStart() > 10) {
      hasDragged = true;
      if (onRenderTrigger) {
        juce::File tempWavFile = onRenderTrigger();
        if (tempWavFile.existsAsFile()) {
          // Lancer l'action de drag natif vers le DAW (Ableton, FL Studio...)
          juce::DragAndDropContainer::performExternalDragDropOfFiles(
              {tempWavFile.getFullPathName()}, false, this);
        }
      }
    }
  }

  void mouseUp(const juce::MouseEvent &) override { hasDragged = false; }

private:
  std::function<juce::File()> onRenderTrigger;
  bool isMouseOver = false;
  bool hasDragged = false;
};

// ==============================================================================
// 1. LE PROCESSEUR AUDIO (Logique DSP, gestion de fichier et séquenceur)
// ==============================================================================
class SlicerAudioProcessor : public juce::AudioProcessor,
                             public juce::ChangeBroadcaster {
public:
  SlicerAudioProcessor()
      : AudioProcessor(
            BusesProperties()
                .withInput("Input", juce::AudioChannelSet::stereo(), true)
                .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
        state(*this, nullptr, "PARAMETERS", createParameterLayout()) {
    formatManager.registerBasicFormats();

    gridParam = state.getRawParameterValue(GRID_ID);
    beatsParam = state.getRawParameterValue(BEATS_ID);
    bpmParam = state.getRawParameterValue(BPM_ID);
    randomParam = state.getRawParameterValue(RANDOM_ID);
    repeatParam = state.getRawParameterValue(REPEAT_ID);
    syncParam = state.getRawParameterValue(SYNC_ID);
    modeParam = state.getRawParameterValue(MODE_ID);
    sensitivityParam = state.getRawParameterValue(SENSITIVITY_ID);
  }

  ~SlicerAudioProcessor() override {}

  juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    juce::StringArray gridOptions = {
        "1 Bar (Mesure)", "1/2 (Blanche)",        "1/4 (Noire)",
        "1/8 (Croche)",   "1/16 (Double-Croche)", "1/32 (Triple-Croche)"};
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(GRID_ID, 1), GRID_NAME, gridOptions, 2));

    layout.add(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID(BEATS_ID, 1), BEATS_NAME, 1, 256, 16));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(BPM_ID, 1), BPM_NAME,
        juce::NormalisableRange<float>(40.0f, 240.0f, 0.1f), 120.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(RANDOM_ID, 1), RANDOM_NAME,
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(REPEAT_ID, 1), REPEAT_NAME,
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 0.0f));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID(SYNC_ID, 1), SYNC_NAME, false));

    juce::StringArray modeOptions = {"Warp-Synced", "Transients"};
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(MODE_ID, 1), MODE_NAME, modeOptions, 0));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(SENSITIVITY_ID, 1), SENSITIVITY_NAME,
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 50.0f));

    return layout;
  }

  bool loadFile(const juce::File &file) {
    stopSequence();
    std::unique_ptr<juce::AudioFormatReader> reader(
        formatManager.createReaderFor(file));

    if (reader != nullptr) {
      double duration = reader->lengthInSamples / reader->sampleRate;

      if (duration > MAX_SAMPLE_DURATION_SECONDS) {
        hasErrorFlag = true;
        errorMessage = "Fichier rejete : Fichier trop lourd !\nLa limite de "
                       "securite est de 5 minutes (" +
                       juce::String((int)MAX_SAMPLE_DURATION_SECONDS) +
                       "s).\nCe fichier fait " + juce::String((int)duration) +
                       "s.";
        sampleLoaded = false;

        sendChangeMessage();
        return false;
      }

      const juce::SpinLock::ScopedLockType sl(bufferLock);
      fileBuffer.setSize((int)reader->numChannels,
                         (int)reader->lengthInSamples);
      reader->read(&fileBuffer, 0, (int)reader->lengthInSamples, 0, true, true);

      sampleRateOfFile = reader->sampleRate;
      sampleLoaded = true;
      hasErrorFlag = false;
      errorMessage = "";
      activeSliceIndex.store(0);
      currentPlaybackSample.store(0);

      // Force slices regeneration
      {
        const juce::SpinLock::ScopedLockType sl2(slicesLock);
        lastGridIndex = -1;
        lastBeatsCount = -1.0f;
        lastSlicingMode = -1;
        lastSensitivity = -1.0f;
        slicePositions.clear();
      }

      sendChangeMessage();
      return true;
    }

    hasErrorFlag = true;
    errorMessage = "Impossible de decompresser ou lire ce fichier audio.";
    sendChangeMessage();
    return false;
  }

  void getAudioBufferCopy(juce::AudioBuffer<float> &destBuffer) {
    const juce::SpinLock::ScopedLockType sl(bufferLock);
    destBuffer.makeCopyOf(fileBuffer);
  }

  bool isSampleLoaded() const { return sampleLoaded; }
  bool hasError() const { return hasErrorFlag; }
  juce::String getErrorMessage() const { return errorMessage; }
  int getFileBufferLength() const {
    return sampleLoaded ? fileBuffer.getNumSamples() : 0;
  }

  void detectTransientsInternal() {
    slicePositions.clear();
    if (!sampleLoaded)
      return;

    const int totalSamples = fileBuffer.getNumSamples();
    const float *channelData = fileBuffer.getReadPointer(0);
    double sampleRate = sampleRateOfFile > 0 ? sampleRateOfFile : 44100.0;
    float sensitivity = sensitivityParam->load();

    // Calcul du pic global pour adapter le seuil de sensibilité au volume du
    // fichier
    float globalPeak = 0.0f;
    for (int s = 0; s < totalSamples; ++s) {
      float absVal = std::abs(channelData[s]);
      if (absVal > globalPeak)
        globalPeak = absVal;
    }

    if (globalPeak < 0.01f) {
      slicePositions.push_back(0);
      return; // Signal trop faible
    }

    // Traduction de la sensibilité (0-100) en seuil de détection relatif
    // Un réglage à 100% de sensibilité donne un seuil bas (~2% du pic global),
    // 0% donne un seuil haut (~45% du pic global)
    float threshold = globalPeak * (0.45f - (sensitivity / 100.0f) * 0.43f);
    if (threshold < 0.002f)
      threshold = 0.002f;

    // Temps de garde minimum entre deux tranches : 80ms
    int minSamplesBetweenSlices = (int)(sampleRate * 0.08);
    int lastSlicePos = 0;

    slicePositions.push_back(0);

    // Suiveur d'enveloppe DSP
    float env = 0.0f;
    float attackCoeff =
        (float)(1.0 - std::exp(-1.0 / (sampleRate * 0.01))); // Attaque 10ms
    float releaseCoeff =
        (float)(1.0 - std::exp(-1.0 / (sampleRate * 0.08))); // Release 80ms

    std::vector<float> envelope(totalSamples, 0.0f);
    for (int i = 0; i < totalSamples; ++i) {
      float input = std::abs(channelData[i]);
      if (input > env)
        env += attackCoeff * (input - env);
      else
        env += releaseCoeff * (input - env);
      envelope[i] = env;
    }

    // Fenêtre d'analyse de la pente : 15ms (nombre d'échantillons
    // correspondant)
    int slopeWindow = (int)(sampleRate * 0.015);
    if (slopeWindow < 5)
      slopeWindow = 5;

    // Analyse de la dérivée de l'enveloppe
    for (int i = slopeWindow; i < totalSamples - slopeWindow; ++i) {
      float slope = envelope[i] - envelope[i - slopeWindow];

      if (slope > threshold && (i - lastSlicePos) > minSamplesBetweenSlices) {
        // Caler précisément le début sur le minimum local d'enveloppe des 25ms
        // précédentes
        int exactStart = i;
        float minVal = envelope[i];
        int searchRange = (int)(sampleRate * 0.025);

        for (int j = i - 1; j > i - searchRange && j >= 0; --j) {
          if (envelope[j] < minVal) {
            minVal = envelope[j];
            exactStart = j;
          }
        }

        slicePositions.push_back(exactStart);
        lastSlicePos = exactStart;
        i += minSamplesBetweenSlices; // Sauter le temps de garde
      }
    }
  }

  void regenerateSlicesInternal() {
    int mode = (int)modeParam->load();
    if (mode == 1) // Transient mode
    {
      detectTransientsInternal();
      return;
    }

    slicePositions.clear();
    if (!sampleLoaded)
      return;

    const int totalSamples = fileBuffer.getNumSamples();
    const float totalBeats = beatsParam->load();
    const int gridIndex = (int)gridParam->load();

    float beatSubdivision = 1.0f;
    switch (gridIndex) {
    case 0:
      beatSubdivision = 4.0f;
      break; // 1 Bar
    case 1:
      beatSubdivision = 2.0f;
      break; // 1/2 blanche
    case 2:
      beatSubdivision = 1.0f;
      break; // 1/4 noire
    case 3:
      beatSubdivision = 0.5f;
      break; // 1/8 croche
    case 4:
      beatSubdivision = 0.25f;
      break; // 1/16 double-croche
    case 5:
      beatSubdivision = 0.125f;
      break; // 1/32 triple-croche
    default:
      beatSubdivision = 1.0f;
    }

    float slicesCountFloat = totalBeats / beatSubdivision;
    int numSlices = juce::jmax(1, (int)std::floor(slicesCountFloat));
    double samplesPerSlice = (double)totalSamples / slicesCountFloat;

    float globalPeak = 0.0f;
    const float *channelData = fileBuffer.getReadPointer(0);
    for (int s = 0; s < totalSamples; ++s) {
      float absVal = std::abs(channelData[s]);
      if (absVal > globalPeak)
        globalPeak = absVal;
    }

    float silenceThreshold = globalPeak * 0.02f;
    if (silenceThreshold < 0.005f)
      silenceThreshold = 0.005f;

    slicePositions.push_back(0);

    for (int i = 1; i <= numSlices; ++i) {
      int sliceSamplePos = (int)std::round(i * samplesPerSlice);
      if (sliceSamplePos < totalSamples) {
        int nextSliceEnd = (int)std::round((i + 1) * samplesPerSlice);
        int endLimit = std::min(nextSliceEnd, totalSamples);

        float slicePeak = 0.0f;
        for (int s = sliceSamplePos; s < endLimit; ++s) {
          float absVal = std::abs(channelData[s]);
          if (absVal > slicePeak)
            slicePeak = absVal;
        }

        if (slicePeak >= silenceThreshold) {
          slicePositions.push_back(sliceSamplePos);
        }
      }
    }
  }

  std::vector<int> getSlicePositionsInSamples() {
    const juce::SpinLock::ScopedLockType sl(slicesLock);

    int currentGrid = (int)gridParam->load();
    float currentBeats = beatsParam->load();
    int currentMode = (int)modeParam->load();
    float currentSens = sensitivityParam->load();

    if (currentGrid != lastGridIndex || currentBeats != lastBeatsCount ||
        currentMode != lastSlicingMode || currentSens != lastSensitivity ||
        slicePositions.empty()) {
      lastGridIndex = currentGrid;
      lastBeatsCount = currentBeats;
      lastSlicingMode = currentMode;
      lastSensitivity = currentSens;
      regenerateSlicesInternal();
    }

    return slicePositions;
  }

  void deleteSliceMarker(int index) {
    const juce::SpinLock::ScopedLockType sl(slicesLock);
    if (index > 0 && index < (int)slicePositions.size()) {
      slicePositions.erase(slicePositions.begin() + index);
      sendChangeMessage();
    }
  }

  void moveSliceMarker(int index, int newSamplePos) {
    const juce::SpinLock::ScopedLockType sl(slicesLock);
    if (index > 0 && index < (int)slicePositions.size() - 1) {
      int minLimit = slicePositions[index - 1] + 100;
      int maxLimit = slicePositions[index + 1] - 100;
      if (minLimit < maxLimit) {
        slicePositions[index] = juce::jlimit(minLimit, maxLimit, newSamplePos);
      }
    } else if (index == (int)slicePositions.size() - 1 && index > 0) {
      int minLimit = slicePositions[index - 1] + 100;
      int maxLimit = fileBuffer.getNumSamples() - 100;
      if (minLimit < maxLimit) {
        slicePositions[index] = juce::jlimit(minLimit, maxLimit, newSamplePos);
      }
    }
    sendChangeMessage();
  }

  int getCurrentPlaybackSample() const { return currentPlaybackSample.load(); }

  int getActiveSliceIndex() const { return activeSliceIndex.load(); }

  void startSequence(bool fromBeginning = true) {
    if (!sampleLoaded)
      return;

    auto slices = getSlicePositionsInSamples();
    if (slices.empty())
      return;

    if (fromBeginning) {
      activeSliceIndex.store(0);
      int startSample = slices[0];
      int endSample =
          (slices.size() > 1) ? slices[1] : fileBuffer.getNumSamples();

      currentPlaybackSample.store(startSample);
      currentSliceEndSample.store(endSample);

      fadeSamplesRemaining = fadeLength;
      isFadeIn = true;
    } else {
      int currentPos = currentPlaybackSample.load();
      if (currentPos < 0 || currentPos >= fileBuffer.getNumSamples()) {
        activeSliceIndex.store(0);
        int startSample = slices[0];
        int endSample =
            (slices.size() > 1) ? slices[1] : fileBuffer.getNumSamples();

        currentPlaybackSample.store(startSample);
        currentSliceEndSample.store(endSample);

        fadeSamplesRemaining = fadeLength;
        isFadeIn = true;
      } else {
        int foundSlice = 0;
        for (int i = 0; i < (int)slices.size(); ++i) {
          int startS = slices[i];
          int endS = (i + 1 < (int)slices.size()) ? slices[i + 1]
                                                  : fileBuffer.getNumSamples();
          if (currentPos >= startS && currentPos < endS) {
            foundSlice = i;
            break;
          }
        }
        activeSliceIndex.store(foundSlice);

        int endSample = (foundSlice + 1 < (int)slices.size())
                            ? slices[foundSlice + 1]
                            : fileBuffer.getNumSamples();
        currentSliceEndSample.store(endSample);
      }
    }

    isPlayingSequence.store(true);
  }

  void stopSequence() { isPlayingSequence.store(false); }

  bool isCurrentlyPlaying() const { return isPlayingSequence.load(); }

  void determineNextSlice() {
    auto slices = getSlicePositionsInSamples();
    if (slices.empty()) {
      isPlayingSequence.store(false);
      return;
    }

    int totalSlices = (int)slices.size();
    int curIdx = activeSliceIndex.load();

    float randProbability = randomParam->load() / 100.0f;
    float repeatProbability = repeatParam->load() / 100.0f;

    int nextIdx = curIdx;

    if (randomGenerator.nextFloat() < repeatProbability) {
      nextIdx = curIdx;
    } else {
      if (randomGenerator.nextFloat() < randProbability) {
        nextIdx = randomGenerator.nextInt(totalSlices);
      } else {
        nextIdx = (curIdx + 1) % totalSlices;
      }
    }

    activeSliceIndex.store(nextIdx);

    int startSample = slices[nextIdx];
    int endSample = (nextIdx + 1 < totalSlices) ? slices[nextIdx + 1]
                                                : fileBuffer.getNumSamples();

    currentPlaybackSample.store(startSample);
    currentSliceEndSample.store(endSample);

    fadeSamplesRemaining = fadeLength;
    isFadeIn = true;
  }

  void prepareToPlay(double sampleRate, int samplesPerBlock) override {
    juce::ignoreUnused(samplesPerBlock);
    fadeLength = (int)(sampleRate * 0.005);
  }

  void releaseResources() override {}

  bool isBusesLayoutSupported(const BusesLayout &layouts) const override {
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
        layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
      return false;
    return true;
  }

  // ==============================================================================
  // EXPORT INTELLIGENT DIRECTEMENT COMPILÉ EN TÂCHE DE FOND EN WAV
  // ==============================================================================
  juce::File renderLoopToWavFile() {
    if (!sampleLoaded)
      return {};

    juce::File tempDir =
        juce::File::getSpecialLocation(juce::File::tempDirectory);
    juce::File wavFile = tempDir.getChildFile("RythmSlicer_Export_ARTSEN.wav");
    if (wavFile.existsAsFile())
      wavFile.deleteFile();

    const int totalBeats = (int)beatsParam->load();
    float activeBpm = bpmParam->load();

    // Tenter d'utiliser le BPM du DAW si disponible
    if (syncParam->load() > 0.5f) {
      if (auto *hostPlayHead = getPlayHead()) {
        if (auto posOpt = hostPlayHead->getPosition()) {
          if (auto bpmOpt = posOpt->getBpm())
            activeBpm = static_cast<float>(*bpmOpt);
        }
      }
    }

    double sampleRate = sampleRateOfFile > 0 ? sampleRateOfFile : 44100.0;
    double secondsPerBeat = 60.0 / activeBpm;
    double totalDurationSeconds = totalBeats * secondsPerBeat;
    int totalSamplesToRender = (int)(totalDurationSeconds * sampleRate);

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::FileOutputStream> outStream(
        wavFile.createOutputStream());
    if (outStream == nullptr)
      return {};

#pragma warning(push)
#pragma warning(disable : 4996)
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wavFormat.createWriterFor(outStream.release(), sampleRate,
                                  fileBuffer.getNumChannels(), 16, {}, 0));
#pragma warning(pop)

    if (writer == nullptr)
      return {};

    auto slices = getSlicePositionsInSamples();
    if (slices.empty())
      return {};

    int bouncePlayPos = slices[0];
    int bounceEndPos =
        (slices.size() > 1) ? slices[1] : fileBuffer.getNumSamples();
    int bounceActiveSlice = 0;
    int bounceFadeSamplesRemaining = fadeLength;
    bool bounceIsFadeIn = true;
    juce::Random bounceRand;

    int chunkSize = 512;
    juce::AudioBuffer<float> chunkBuffer(fileBuffer.getNumChannels(),
                                         chunkSize);

    int samplesRendered = 0;
    while (samplesRendered < totalSamplesToRender) {
      int samplesToProcess =
          std::min(chunkSize, totalSamplesToRender - samplesRendered);
      chunkBuffer.clear();

      const juce::SpinLock::ScopedLockType sl(bufferLock);
      const int fileLength = fileBuffer.getNumSamples();
      const int fileChannels = fileBuffer.getNumChannels();

      int written = 0;
      while (written < samplesToProcess) {
        if (bouncePlayPos >= bounceEndPos || bouncePlayPos >= fileLength) {
          int curIdx = bounceActiveSlice;
          float randProbability = randomParam->load() / 100.0f;
          float repeatProbability = repeatParam->load() / 100.0f;

          int nextIdx = curIdx;
          if (bounceRand.nextFloat() < repeatProbability) {
            nextIdx = curIdx;
          } else {
            if (bounceRand.nextFloat() < randProbability)
              nextIdx = bounceRand.nextInt((int)slices.size());
            else
              nextIdx = (curIdx + 1) % (int)slices.size();
          }

          bounceActiveSlice = nextIdx;
          bouncePlayPos = slices[nextIdx];
          bounceEndPos = (nextIdx + 1 < (int)slices.size())
                             ? slices[nextIdx + 1]
                             : fileLength;
          bounceFadeSamplesRemaining = fadeLength;
          bounceIsFadeIn = true;
        }

        int toCopy =
            std::min(samplesToProcess - written, bounceEndPos - bouncePlayPos);
        if (toCopy <= 0)
          break;

        for (int channel = 0; channel < fileChannels; ++channel) {
          float *destData = chunkBuffer.getWritePointer(channel, written);
          const float *sourceData =
              fileBuffer.getReadPointer(channel, bouncePlayPos);

          for (int i = 0; i < toCopy; ++i) {
            float sampleVal = sourceData[i];
            if (bounceFadeSamplesRemaining > 0) {
              float ratio =
                  (float)bounceFadeSamplesRemaining / (float)fadeLength;
              float gain = bounceIsFadeIn ? (1.0f - ratio) : ratio;
              sampleVal *= gain;
              if (channel == fileChannels - 1) {
                bounceFadeSamplesRemaining--;
              }
            }
            destData[i] = sampleVal;
          }
        }

        bouncePlayPos += toCopy;
        written += toCopy;
      }

      writer->writeFromAudioSampleBuffer(chunkBuffer, 0, samplesToProcess);
      samplesRendered += samplesToProcess;
    }

    writer.reset();
    return wavFile;
  }

  // ==============================================================================
  // PROCESSBLOCK AVEC COMPATIBILITÉ ET SYNCHRONISATION BPM DAW
  // ==============================================================================
  void processBlock(juce::AudioBuffer<float> &buffer,
                    juce::MidiBuffer &midiMessages) override {
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamplesToProcess = buffer.getNumSamples();

    for (auto i = 0; i < totalNumOutputChannels; ++i)
      buffer.clear(i, 0, numSamplesToProcess);

    // Récupération des informations d'hôte du DAW (BPM et Lecture)
    bool hostIsPlaying = false;
    if (auto *hostPlayHead = getPlayHead()) {
      if (auto posOpt = hostPlayHead->getPosition()) {
        hostIsPlaying = posOpt->getIsPlaying();
        if (auto bpmOpt = posOpt->getBpm()) {
          // L'hôte fournit le BPM en continu
          lastHostBpm = *bpmOpt;
        }
      }
    }

    // Si la synchronisation d'hôte est active (Sync On)
    bool isSyncActive = syncParam->load() > 0.5f;
    if (isSyncActive) {
      if (hostIsPlaying && !isPlayingSequence.load()) {
        startSequence();
      } else if (!hostIsPlaying && isPlayingSequence.load()) {
        stopSequence();
      }
    }

    if (isPlayingSequence.load() && sampleLoaded) {
      const juce::SpinLock::ScopedLockType sl(bufferLock);
      const int fileLength = fileBuffer.getNumSamples();
      const int fileChannels = fileBuffer.getNumChannels();

      int playPos = currentPlaybackSample.load();
      int endPos = currentSliceEndSample.load();
      int samplesWritten = 0;

      while (samplesWritten < numSamplesToProcess) {
        if (playPos >= endPos || playPos >= fileLength) {
          determineNextSlice();
          playPos = currentPlaybackSample.load();
          endPos = currentSliceEndSample.load();
        }

        int samplesToCopy =
            std::min(numSamplesToProcess - samplesWritten, endPos - playPos);
        if (samplesToCopy <= 0) {
          isPlayingSequence.store(false);
          break;
        }

        for (int channel = 0; channel < totalNumOutputChannels; ++channel) {
          int fileChannel = channel % fileChannels;
          float *destData = buffer.getWritePointer(channel, samplesWritten);
          const float *sourceData =
              fileBuffer.getReadPointer(fileChannel, playPos);

          for (int i = 0; i < samplesToCopy; ++i) {
            float sampleVal = sourceData[i];

            if (fadeSamplesRemaining > 0) {
              float ratio = (float)fadeSamplesRemaining / (float)fadeLength;
              float gain = isFadeIn ? (1.0f - ratio) : ratio;
              sampleVal *= gain;

              if (channel == totalNumOutputChannels - 1) {
                fadeSamplesRemaining--;
              }
            }

            destData[i] = sampleVal;
          }
        }

        playPos += samplesToCopy;
        samplesWritten += samplesToCopy;
      }

      currentPlaybackSample.store(playPos);
    }
  }

  double getLastHostBpm() const { return lastHostBpm; }

  const juce::String getName() const override { return "RythmSlicer"; }
  bool acceptsMidi() const override { return true; }
  bool producesMidi() const override { return false; }
  bool isMidiEffect() const override { return false; }
  double getTailLengthSeconds() const override { return 0.0; }

  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int /*index*/) override {}
  const juce::String getProgramName(int /*index*/) override { return {}; }
  void changeProgramName(int /*index*/,
                         const juce::String & /*newName*/) override {}

  void getStateInformation(juce::MemoryBlock &destData) override {
    auto stateXml(state.copyState().createXml());
    copyXmlToBinary(*stateXml, destData);
  }

  void setStateInformation(const void *data, int sizeInBytes) override {
    std::unique_ptr<juce::XmlElement> xmlState(
        getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(state.state.getType()))
      state.replaceState(juce::ValueTree::fromXml(*xmlState));
  }

  bool hasEditor() const override { return true; }
  juce::AudioProcessorEditor *createEditor() override;

private:
  juce::AudioProcessorValueTreeState state;

  std::atomic<float> *gridParam = nullptr;
  std::atomic<float> *beatsParam = nullptr;
  std::atomic<float> *bpmParam = nullptr;
  std::atomic<float> *randomParam = nullptr;
  std::atomic<float> *repeatParam = nullptr;
  std::atomic<float> *syncParam = nullptr;

  juce::AudioFormatManager formatManager;
  juce::AudioBuffer<float> fileBuffer;
  juce::SpinLock bufferLock;

  double sampleRateOfFile = 44100.0;
  bool sampleLoaded = false;
  double lastHostBpm = 120.0;

  bool hasErrorFlag = false;
  juce::String errorMessage = "";

  std::atomic<bool> isPlayingSequence{false};
  std::atomic<int> currentPlaybackSample{0};
  std::atomic<int> currentSliceEndSample{0};
  std::atomic<int> activeSliceIndex{0};

  int fadeLength = 220;
  int fadeSamplesRemaining = 0;
  bool isFadeIn = true;

  juce::Random randomGenerator;

  std::vector<int> slicePositions;
  juce::SpinLock slicesLock;
  int lastGridIndex = -1;
  float lastBeatsCount = -1.0f;
  int lastSlicingMode = -1;
  float lastSensitivity = -1.0f;

  std::atomic<float> *modeParam = nullptr;
  std::atomic<float> *sensitivityParam = nullptr;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlicerAudioProcessor)
};

// ==============================================================================
// 2. COMPOSANT VISUEL DE LA FORME D'ONDE AVEC RETOUR EN TEMPS RÉEL
// ==============================================================================
class WaveformVisualizer : public juce::Component {
public:
  WaveformVisualizer(SlicerAudioProcessor &p) : processor(p) {}

  void updateWaveform() {
    if (processor.isSampleLoaded())
      processor.getAudioBufferCopy(localBufferCopy);
    else
      localBufferCopy.setSize(1, 0);

    repaint();
  }

  int getSliceIndexNearX(int x, float tolerancePixels = 6.0f) {
    if (!processor.isSampleLoaded())
      return -1;
    auto slicePositions = processor.getSlicePositionsInSamples();
    int numSamples = processor.getFileBufferLength();
    int width = getWidth();

    for (int i = 1; i < (int)slicePositions.size(); ++i) {
      float ratio = (float)slicePositions[i] / (float)numSamples;
      int xPos = (int)(ratio * width);
      if (std::abs(x - xPos) <= tolerancePixels) {
        return i;
      }
    }
    return -1;
  }

  void mouseDown(const juce::MouseEvent &e) override {
    draggedSliceIndex = getSliceIndexNearX(e.x);
  }

  void mouseDrag(const juce::MouseEvent &e) override {
    if (draggedSliceIndex > 0) {
      int numSamples = processor.getFileBufferLength();
      float ratio = (float)e.x / (float)getWidth();
      int newSamplePos = (int)(ratio * numSamples);
      processor.moveSliceMarker(draggedSliceIndex, newSamplePos);
      repaint();
    }
  }

  void mouseUp(const juce::MouseEvent & /*e*/) override {
    draggedSliceIndex = -1;
  }

  void mouseDoubleClick(const juce::MouseEvent &e) override {
    int idx = getSliceIndexNearX(e.x);
    if (idx > 0) {
      processor.deleteSliceMarker(idx);
      repaint();
    }
  }

  void mouseMove(const juce::MouseEvent &e) override {
    int idx = getSliceIndexNearX(e.x);
    if (idx > 0)
      setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    else
      setMouseCursor(juce::MouseCursor::NormalCursor);
  }

  void paint(juce::Graphics &g) override {
    g.fillAll(juce::Colour(0xFF111115));

    const int width = getWidth();
    const int height = getHeight();

    if (processor.hasError()) {
      g.setColour(juce::Colour(0xFFFF4D4D));
      g.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
      g.drawFittedText(processor.getErrorMessage(),
                       getLocalBounds().reduced(15),
                       juce::Justification::centred, 4);
      return;
    }

    if (localBufferCopy.getNumSamples() == 0) {
      g.setColour(juce::Colours::grey);
      g.setFont(juce::Font(juce::FontOptions(15.0f)));
      g.drawText("Glissez-deposez un sample de batterie ici (< 5 min)\nou "
                 "utilisez le bouton Charger",
                 getLocalBounds(), juce::Justification::centred, true);
      return;
    }

    // Dessin de l'onde audio corrigée
    g.setColour(juce::Colour(0xFF00E5FF)); // Bleu cyan électrique

    const int numSamples = localBufferCopy.getNumSamples();
    const float *channelData = localBufferCopy.getReadPointer(0);
    const double samplesPerPixel = (double)numSamples / (double)width;

    for (int pixel = 0; pixel < width; ++pixel) {
      int startSample = (int)(pixel * samplesPerPixel);
      int endSample = (int)((pixel + 1) * samplesPerPixel);

      float minVal = 0.0f;
      float maxVal = 0.0f;

      for (int s = startSample; s < endSample && s < numSamples; ++s) {
        float val = channelData[s];
        if (val < minVal)
          minVal = val;
        if (val > maxVal)
          maxVal = val;
      }

      float yMin = (height / 2.0f) - (minVal * (height / 2.0f) * 0.95f);
      float yMax = (height / 2.0f) - (maxVal * (height / 2.0f) * 0.95f);

      g.drawVerticalLine(pixel, yMax, yMin);
    }

    // Surbrillance de la tranche active
    auto slicePositions = processor.getSlicePositionsInSamples();
    int activeIdx = processor.getActiveSliceIndex();

    if (processor.isCurrentlyPlaying() && activeIdx >= 0 &&
        activeIdx < (int)slicePositions.size()) {
      int startSample = slicePositions[activeIdx];
      int endSample = (activeIdx + 1 < (int)slicePositions.size())
                          ? slicePositions[activeIdx + 1]
                          : numSamples;

      float startRatio = (float)startSample / (float)numSamples;
      float endRatio = (float)endSample / (float)numSamples;

      int xStart = (int)(startRatio * width);
      int xEnd = (int)(endRatio * width);

      g.setColour(juce::Colour(0x3FFF006E));
      g.fillRect(xStart, 0, xEnd - xStart, height);
    }

    // Barres de découpe (Slices)
    g.setColour(juce::Colour(0xFFFF006E)); // Rose fluo
    for (int sliceSample : slicePositions) {
      float ratio = (float)sliceSample / (float)numSamples;
      int xPos = (int)(ratio * width);

      g.drawVerticalLine(xPos, 0.0f, (float)height);
      g.fillRect(xPos - 2, 0, 5, 8);
    }

    // Dessin du Playhead (Curseur de lecture)
    if (processor.isCurrentlyPlaying() ||
        processor.getCurrentPlaybackSample() > 0) {
      int currentSample = processor.getCurrentPlaybackSample();
      float ratio = (float)currentSample / (float)numSamples;
      int xPos = (int)(ratio * width);

      g.setColour(juce::Colours::white);
      g.drawVerticalLine(xPos, 0.0f, (float)height);

      juce::Path triangle;
      triangle.addTriangle((float)xPos - 5.0f, 0.0f, (float)xPos + 5.0f, 0.0f,
                           (float)xPos, 6.0f);
      g.fillPath(triangle);
    }
  }

private:
  SlicerAudioProcessor &processor;
  juce::AudioBuffer<float> localBufferCopy;
  int draggedSliceIndex = -1;
};

// ==============================================================================
// COMPOSANT D'AIDE UTILISATEUR INTEGREE (OVERLAY)
// ==============================================================================
class HelpOverlay : public juce::Component {
public:
  HelpOverlay() {
    closeButton.setButtonText("X");
    closeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFFFF006E)); // Neon Pink
    closeButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible(closeButton);
    closeButton.onClick = [this]() { setVisible(false); };

    addAndMakeVisible(viewport);
    viewport.setViewedComponent(&helpTextLabel, false);

    helpTextLabel.setMultiLine(true);
    helpTextLabel.setReadOnly(true);
    helpTextLabel.setCaretVisible(false);
    helpTextLabel.setScrollbarsShown(false);

    helpTextLabel.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF1E1E24));
    helpTextLabel.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    helpTextLabel.setFont(juce::Font(juce::FontOptions(13.0f)));

    juce::File helpFile("C:\\Users\\comme\\Documents\\GitHub\\RythmSlicer\\help.md");
    if (helpFile.existsAsFile()) {
      helpTextLabel.setText(helpFile.loadFileAsString());
    } else {
      helpTextLabel.setText("Guide d'Utilisation RythmSlicer v1.6.3\n\n(Fichier help.md non trouve)");
    }
  }

  void paint(juce::Graphics &g) override {
    g.fillAll(juce::Colour(0xDD111115));

    auto bounds = getLocalBounds().reduced(30).toFloat();
    g.setColour(juce::Colour(0xFF00E5FF)); // Cyan border
    g.drawRoundedRectangle(bounds, 8.0f, 2.0f);
    g.setColour(juce::Colour(0xFF1E1E24)); // Box background
    g.fillRoundedRectangle(bounds, 8.0f);
  }

  void resized() override {
    auto bounds = getLocalBounds().reduced(30);
    closeButton.setBounds(bounds.getX() + bounds.getWidth() - 35, bounds.getY() + 10, 25, 25);

    auto textBounds = bounds.reduced(15);
    textBounds.removeFromTop(25);
    viewport.setBounds(textBounds);

    int width = textBounds.getWidth() - 16;
    int height = 1200; // Large height to fit markdown text
    helpTextLabel.setSize(width, height);
  }

private:
  juce::TextButton closeButton;
  juce::Viewport viewport;
  juce::TextEditor helpTextLabel;
};

// ==============================================================================
// 3. L'INTERFACE GRAPHIQUE PRINCIPALE (GUI)
// ==============================================================================
class SlicerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   public juce::DragAndDropContainer,
                                   public juce::FileDragAndDropTarget,
                                   public juce::Button::Listener,
                                   public juce::ChangeListener,
                                   public juce::Timer {
public:
  SlicerAudioProcessorEditor(SlicerAudioProcessor &p,
                             juce::AudioProcessorValueTreeState &vts)
      : AudioProcessorEditor(&p), processor(p), valueTreeState(vts),
        visualizer(p), randomKnobLookAndFeel(juce::Colour(0xFF00E5FF)),
        repeatKnobLookAndFeel(juce::Colour(0xFFFF006E)),
        sensitivityKnobLookAndFeel(juce::Colour(0xFF00E5FF)),
        wavDragComponent([this]() { return processor.renderLoopToWavFile(); }) {
    processor.addChangeListener(this);

    // Config Bouton d'Aide "?"
    helpButton.setButtonText("?");
    helpButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF22222B));
    helpButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF00E5FF)); // Cyan
    addAndMakeVisible(helpButton);
    helpButton.onClick = [this]() {
      helpOverlay.setVisible(true);
      helpOverlay.toFront(true);
    };

    // Config Overlay d'Aide
    addChildComponent(helpOverlay);
    helpOverlay.setVisible(false);

    // Appliquer les LookAndFeels sur-mesure pour les potentiomètres
    randomSlider.setLookAndFeel(&randomKnobLookAndFeel);
    repeatSlider.setLookAndFeel(&repeatKnobLookAndFeel);

    // 1. Bouton de chargement manuel
    loadButton.setButtonText("Charger un Sample");
    loadButton.addListener(this);
    loadButton.setColour(juce::TextButton::buttonColourId,
                         juce::Colour(0xFF2A2A35));
    addAndMakeVisible(loadButton);

    // 2. Boutons d'action du séquenceur (Cyberpunk Neon Symbols)
    playButton.setButtonText(juce::String::fromUTF8("▶"));
    playButton.addListener(this);
    playButton.setColour(juce::TextButton::buttonColourId,
                         juce::Colour(0xFF00B4D8));
    addAndMakeVisible(playButton);

    playFromZeroButton.setButtonText(juce::String::fromUTF8("⏮"));
    playFromZeroButton.addListener(this);
    playFromZeroButton.setColour(juce::TextButton::buttonColourId,
                                 juce::Colour(0xFF00F0FF));
    addAndMakeVisible(playFromZeroButton);

    stopButton.setButtonText(juce::String::fromUTF8("⏸"));
    stopButton.addListener(this);
    stopButton.setColour(juce::TextButton::buttonColourId,
                         juce::Colour(0xFFF72585));
    addAndMakeVisible(stopButton);

    // 3. Composant de Glisser-Déposer de WAV
    addAndMakeVisible(wavDragComponent);

    // 4. Composant visuel
    addAndMakeVisible(visualizer);

    // 5. Bouton On/Off Sync Hôte
    syncButton.setButtonText("SYNC DAW");
    syncButton.setClickingTogglesState(true);
    syncButton.addListener(this);
    syncButton.setColour(juce::TextButton::buttonColourId,
                         juce::Colour(0xFF2A2A35));
    syncButton.setColour(juce::TextButton::buttonOnColourId,
                         juce::Colour(0xFF00E5FF));
    syncButton.setColour(juce::TextButton::textColourOnId,
                         juce::Colours::black);
    addAndMakeVisible(syncButton);
    syncAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            valueTreeState, SYNC_ID, syncButton);

    // 6. Sélecteurs de configuration rythmique (Bas Gauche)
    gridLabel.setText("Grille :", juce::dontSendNotification);
    gridLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(gridLabel);

    addAndMakeVisible(gridSelector);
    if (auto *choiceParam = dynamic_cast<juce::AudioParameterChoice *>(
            vts.getParameter(GRID_ID))) {
      gridSelector.addItemList(choiceParam->choices, 1);
    }
    gridAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        valueTreeState, GRID_ID, gridSelector);
    gridSelector.onChange = [this]() { visualizer.repaint(); };

    beatsLabel.setText("Beats :", juce::dontSendNotification);
    beatsLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(beatsLabel);

    beatsSlider.setSliderStyle(juce::Slider::IncDecButtons);
    beatsSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 50, 20);
    addAndMakeVisible(beatsSlider);
    beatsAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            valueTreeState, BEATS_ID, beatsSlider);
    beatsSlider.onValueChange = [this]() { visualizer.repaint(); };

    bpmLabel.setText("BPM :", juce::dontSendNotification);
    bpmLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(bpmLabel);

    bpmSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    bpmSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 60, 20);
    addAndMakeVisible(bpmSlider);
    bpmAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            valueTreeState, BPM_ID, bpmSlider);

    // 7. Contrôles de Randomisation
    randomLabel.setText("Randomisation", juce::dontSendNotification);
    randomLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(randomLabel);

    randomSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    randomSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    randomSlider.setTextValueSuffix(" %");
    addAndMakeVisible(randomSlider);
    randomAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            valueTreeState, RANDOM_ID, randomSlider);

    repeatLabel.setText("Repetition (Stutter)", juce::dontSendNotification);
    repeatLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(repeatLabel);

    repeatSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    repeatSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    repeatSlider.setTextValueSuffix(" %");
    addAndMakeVisible(repeatSlider);
    repeatAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            valueTreeState, REPEAT_ID, repeatSlider);

    // 8. Commutateur de Mode de Découpe (Warp-Synced / Transients)
    modeLabel.setText("Mode :", juce::dontSendNotification);
    modeLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(modeLabel);

    addAndMakeVisible(modeSelector);
    if (auto *modeChoice = dynamic_cast<juce::AudioParameterChoice *>(
            vts.getParameter(MODE_ID))) {
      modeSelector.addItemList(modeChoice->choices, 1);
    }
    modeAttachment = std::make_unique<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment>(vts, MODE_ID,
                                                                modeSelector);
    modeSelector.onChange = [this]() {
      visualizer.repaint();
      updateControlVisibilities();
    };

    // 9. Sensibilité/Granularité pour les Transients
    sensitivityLabel.setText("Sensibilite (Transients)",
                             juce::dontSendNotification);
    sensitivityLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(sensitivityLabel);

    sensitivitySlider.setLookAndFeel(&sensitivityKnobLookAndFeel);
    sensitivitySlider.setSliderStyle(
        juce::Slider::RotaryHorizontalVerticalDrag);
    sensitivitySlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60,
                                      18);
    sensitivitySlider.setTextValueSuffix(" %");
    addAndMakeVisible(sensitivitySlider);
    sensitivityAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            vts, SENSITIVITY_ID, sensitivitySlider);
    sensitivitySlider.onValueChange = [this]() { visualizer.repaint(); };

    visualizer.updateWaveform();
    updateButtonStates();
    updateControlVisibilities();

    startTimerHz(30);

    setSize(850,
            480); // Ajustement de la taille pour intégrer le bandeau d'export
  }

  ~SlicerAudioProcessorEditor() override {
    // Nettoyer proprement les LookAndFeels
    randomSlider.setLookAndFeel(nullptr);
    repeatSlider.setLookAndFeel(nullptr);
    sensitivitySlider.setLookAndFeel(nullptr);

    processor.removeChangeListener(this);
    stopTimer();
  }

  void timerCallback() override {
    // Griser dynamiquement le BPM si Host Sync est activé
    bool isSyncEnabled = syncButton.getToggleState();
    bpmSlider.setEnabled(!isSyncEnabled);

    if (isSyncEnabled) {
      bpmSlider.setValue(processor.getLastHostBpm(),
                         juce::dontSendNotification);
    }

    if (processor.isCurrentlyPlaying()) {
      visualizer.repaint();
    }
  }

  void changeListenerCallback(juce::ChangeBroadcaster * /*source*/) override {
    visualizer.updateWaveform();
    updateButtonStates();
  }

  void paint(juce::Graphics &g) override {
    g.fillAll(juce::Colour(0xFF18181F));

    // Bandeau d'en-tête
    g.setColour(juce::Colour(0xFF22222B));
    g.fillRect(0, 0, getWidth(), 65);

    // Titre Principal "RythmSlicer"
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(20.0f, juce::Font::bold)));
    g.drawText("RythmSlicer", 20, 10, 140, 24, juce::Justification::left);

    // Numéro de Version "v1.6.3" en cyan
    g.setColour(juce::Colour(0xFF00E5FF));
    g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    g.drawText("v1.6.3", 155, 18, 60, 16, juce::Justification::left);

    // Signature "ARTSEN"
    g.setColour(juce::Colour(0xFF8A8A9F));
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    g.drawText("BY ARTSEN", 20, 34, 140, 16, juce::Justification::left);

    // Séquence active info
    if (processor.isCurrentlyPlaying()) {
      g.setColour(juce::Colour(0xFFFF006E));
      g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
      g.drawText("Sequence active : tranche #" +
                     juce::String(processor.getActiveSliceIndex() + 1),
                 240, 23, 200, 20, juce::Justification::left);
    }
  }

  void resized() override {
    helpOverlay.setBounds(getLocalBounds());

    auto area = getLocalBounds();
 
    // En-tête (Boutons d'action)
    auto headerArea = area.removeFromTop(65);
    loadButton.setBounds(headerArea.removeFromRight(150).reduced(12));
    stopButton.setBounds(headerArea.removeFromRight(60).reduced(4, 12));
    playButton.setBounds(headerArea.removeFromRight(60).reduced(4, 12));
    playFromZeroButton.setBounds(headerArea.removeFromRight(60).reduced(4, 12));
    syncButton.setBounds(headerArea.removeFromRight(100).reduced(8, 12));
    helpButton.setBounds(headerArea.removeFromRight(40).reduced(4, 12));

    // Bandeau d'export (placé juste sous le visualiseur)
    auto exportArea = area.removeFromBottom(45).reduced(15, 5);
    wavDragComponent.setBounds(exportArea);

    // Panneau des contrôles du bas
    auto controlsArea = area.removeFromBottom(130).reduced(15);

    auto leftControls =
        controlsArea.removeFromLeft(controlsArea.getWidth() / 2).reduced(5);
    auto rightControls = controlsArea.reduced(5);

    int rowHeight = leftControls.getHeight() / 4;

    auto modeRow = leftControls.removeFromTop(rowHeight);
    modeLabel.setBounds(modeRow.removeFromLeft(80));
    modeSelector.setBounds(modeRow.reduced(2));

    auto gridRow = leftControls.removeFromTop(rowHeight);
    gridLabel.setBounds(gridRow.removeFromLeft(80));
    gridSelector.setBounds(gridRow.reduced(2));

    auto beatsRow = leftControls.removeFromTop(rowHeight);
    beatsLabel.setBounds(beatsRow.removeFromLeft(80));
    beatsSlider.setBounds(beatsRow.reduced(2));

    auto bpmRow = leftControls;
    bpmLabel.setBounds(bpmRow.removeFromLeft(80));
    bpmSlider.setBounds(bpmRow.reduced(2));

    int potWidth = rightControls.getWidth() / 3;

    auto sensitivityPotArea = rightControls.removeFromLeft(potWidth);
    sensitivityLabel.setBounds(sensitivityPotArea.removeFromTop(20));
    sensitivitySlider.setBounds(sensitivityPotArea.reduced(2));

    auto randomPotArea = rightControls.removeFromLeft(potWidth);
    randomLabel.setBounds(randomPotArea.removeFromTop(20));
    randomSlider.setBounds(randomPotArea.reduced(2));

    auto repeatPotArea = rightControls;
    repeatLabel.setBounds(repeatPotArea.removeFromTop(20));
    repeatSlider.setBounds(repeatPotArea.reduced(2));

    // Reste de la fenêtre (Visualiseur au centre)
    visualizer.setBounds(area.reduced(15));
  }

  void buttonClicked(juce::Button *button) override {
    if (button == &loadButton) {
      fileChooser = std::make_unique<juce::FileChooser>(
          "Selectionner un loop audio...", juce::File{}, "*.wav;*.aif;*.aiff");

      auto folderChooserFlags = juce::FileBrowserComponent::openMode |
                                juce::FileBrowserComponent::canSelectFiles;

      fileChooser->launchAsync(folderChooserFlags,
                               [this](const juce::FileChooser &fc) {
                                 auto file = fc.getResult();
                                 if (file.existsAsFile()) {
                                   processor.loadFile(file);
                                 }
                               });
    } else if (button == &playButton) {
      syncButton.setToggleState(false, juce::sendNotification);
      processor.startSequence(false);
    } else if (button == &playFromZeroButton) {
      syncButton.setToggleState(false, juce::sendNotification);
      processor.startSequence(true);
    } else if (button == &stopButton) {
      processor.stopSequence();
    } else if (button == &helpButton) {
      helpOverlay.setVisible(!helpOverlay.isVisible());
    }
  }

  void updateButtonStates() {
    bool sampleReady = processor.isSampleLoaded() && !processor.hasError();
    playButton.setEnabled(sampleReady);
    playFromZeroButton.setEnabled(sampleReady);
    stopButton.setEnabled(sampleReady);
  }

  bool isInterestedInFileDrag(const juce::StringArray &files) override {
    for (auto file : files) {
      if (file.endsWith(".wav") || file.endsWith(".aif") ||
          file.endsWith(".aiff"))
        return true;
    }
    return false;
  }

  void filesDropped(const juce::StringArray &files, int /*x*/,
                    int /*y*/) override {
    if (files.size() > 0) {
      juce::File file(files[0]);
      processor.loadFile(file);
    }
  }

private:
  SlicerAudioProcessor &processor;
  juce::AudioProcessorValueTreeState &valueTreeState;

  WaveformVisualizer visualizer;

  juce::TextButton loadButton;
  juce::TextButton playButton;
  juce::TextButton playFromZeroButton;
  juce::TextButton stopButton;
  juce::TextButton syncButton;
  juce::TextButton helpButton;
  HelpOverlay helpOverlay;

  // Notre composant interactif de Glisser-Déposer de WAV
  WavDragComponent wavDragComponent;

  // Déclaration des LookAndFeels pour dessiner de grands boutons néon cyberpunk
  ModernKnobLookAndFeel randomKnobLookAndFeel;
  ModernKnobLookAndFeel repeatKnobLookAndFeel;

  std::unique_ptr<juce::FileChooser> fileChooser;

  juce::Label gridLabel;
  juce::ComboBox gridSelector;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
      gridAttachment;

  juce::Label beatsLabel;
  juce::Slider beatsSlider;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
      beatsAttachment;

  juce::Label bpmLabel;
  juce::Slider bpmSlider;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
      bpmAttachment;

  juce::Label randomLabel;
  juce::Slider randomSlider;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
      randomAttachment;

  juce::Label repeatLabel;
  juce::Slider repeatSlider;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
      repeatAttachment;

  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
      syncAttachment;

  juce::Label modeLabel;
  juce::ComboBox modeSelector;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
      modeAttachment;

  juce::Label sensitivityLabel;
  juce::Slider sensitivitySlider;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
      sensitivityAttachment;
  ModernKnobLookAndFeel sensitivityKnobLookAndFeel;

  void updateControlVisibilities() {
    int mode = modeSelector.getSelectedItemIndex();
    bool isWarp = (mode == 0);

    gridLabel.setVisible(isWarp);
    gridSelector.setVisible(isWarp);
    beatsLabel.setVisible(isWarp);
    beatsSlider.setVisible(isWarp);

    sensitivityLabel.setVisible(!isWarp);
    sensitivitySlider.setVisible(!isWarp);
  }

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlicerAudioProcessorEditor)
};

juce::AudioProcessorEditor *SlicerAudioProcessor::createEditor() {
  return new SlicerAudioProcessorEditor(*this, state);
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new SlicerAudioProcessor();
}