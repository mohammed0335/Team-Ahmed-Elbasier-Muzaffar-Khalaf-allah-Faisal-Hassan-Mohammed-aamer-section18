#include "MainComponent.h"

MainComponent::MainComponent()
{
    audio.addChangeListener(this);

    for (auto* b : { &loadButton, &playPauseButton, &restartButton, &stopButton,
                     &startButton, &endButton, &muteButton, &loopButton,
                     &jumpBack, &jumpForward, &addMarkerButton })
    {
        addAndMakeVisible(b);
        b->addListener(this);
    }

    // Slider setup
    volumeSlider.setRange(0.0, 1.0, 0.01);
    volumeSlider.setValue(0.5);
    volumeSlider.addListener(this);
    addAndMakeVisible(volumeSlider);

    positionSlider.setRange(0.0, 1.0, 0.001);
    positionSlider.addListener(this);
    addAndMakeVisible(positionSlider);

    speedSlider.setRange(0.5, 2.0, 0.01);
    speedSlider.setValue(1.0);
    speedSlider.addListener(this);
    addAndMakeVisible(speedSlider);

    // Labels
    addAndMakeVisible(fileLabel);
    addAndMakeVisible(timeLabel);
    addAndMakeVisible(markersLabel);

    // Playlist Box 
    playlistBox.setModel(nullptr);
    playlistBox.setColour(juce::ListBox::backgroundColourId, juce::Colours::darkgrey);
    playlistBox.setColour(juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    playlistBox.setRowHeight(22);
    addAndMakeVisible(playlistBox);

    // استعادة الجلسة السابقة إن وُجدت
    sessionFile = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                    .getChildFile("audioplayer_session.props");

    audio.restoreSession(sessionFile);
    refreshPlaylistItems();

    setSize(900, 360);
    startTimerHz(10);
}

MainComponent::~MainComponent()
{
    stopTimer();
    audio.saveSession(sessionFile);
    audio.removeChangeListener(this);
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(25, 28, 33)); // خلفية داكنة أنيقة
}

void MainComponent::resized()
{
    int margin = 10;
    int buttonW = 85;
    int buttonH = 30;
    int x = margin, y = margin;

    // الصف الأول من الأزرار
    for (auto* b : { &loadButton, &playPauseButton, &restartButton, &stopButton,
                     &startButton, &endButton })
    {
        b->setBounds(x, y, buttonW, buttonH);
        x += buttonW + margin;
    }

    // الصف الثاني
    y += buttonH + margin;
    x = margin;
    for (auto* b : { &muteButton, &loopButton, &jumpBack, &jumpForward, &addMarkerButton })
    {
        b->setBounds(x, y, buttonW, buttonH);
        x += buttonW + margin;
    }

    // السلايدرات
    y += buttonH + margin;
    volumeSlider.setBounds(margin, y, getWidth() - 270, 25);
    y += 35;
    positionSlider.setBounds(margin, y, getWidth() - 270, 25);
    y += 35;
    speedSlider.setBounds(margin, y, getWidth() - 270, 25);

    // اللابلز
    y += 40;
    fileLabel.setBounds(margin, y, getWidth() - 270, 25);
    y += 25;
    timeLabel.setBounds(margin, y, getWidth() - 270, 25);
    y += 25;
    markersLabel.setBounds(margin, y, getWidth() - 270, 25);

    // قائمة التشغيل
    playlistBox.setBounds(getWidth() - 250, margin, 230, getHeight() - 2 * margin);
}

void MainComponent::buttonClicked(juce::Button* button)
{
    if (button == &loadButton)
    {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Select an audio file...", juce::File{}, "*.mp3;*.wav");

        fileChooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f.existsAsFile())
                {
                    audio.addToPlaylist(f);
                    refreshPlaylistItems();
                    audio.playIndex(audio.getPlaylistSize() - 1);
                }
            });
    }
    else if (button == &playPauseButton)
        audio.isPlaying() ? audio.pause() : audio.play();

    else if (button == &restartButton)
        audio.restart();

    else if (button == &stopButton)
        audio.stop();

    else if (button == &startButton)
        audio.setPosition(0.0);

    else if (button == &endButton)
        audio.setPosition(audio.getLength());

    else if (button == &muteButton)
    {
        static float lastVol = 0.5f;
        if (audio.getGain() > 0.001f)
        {
            lastVol = audio.getGain();
            audio.setGain(0.0f);
            muteButton.setButtonText("Unmute");
        }
        else
        {
            audio.setGain(lastVol);
            muteButton.setButtonText("Mute");
        }
    }
    else if (button == &loopButton)
    {
        audio.setLooping(!audio.isLooping());
        loopButton.setButtonText(audio.isLooping() ? "Unloop" : "Loop");
    }
    else if (button == &jumpBack)
        audio.jumpSeconds(-10.0);
    else if (button == &jumpForward)
        audio.jumpSeconds(10.0);
    else if (button == &addMarkerButton)
        audio.addMarker(audio.getPosition());
}

void MainComponent::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &volumeSlider)
        audio.setGain((float)volumeSlider.getValue());
    else if (slider == &positionSlider)
        audio.setPosition(positionSlider.getValue() * audio.getLength());
    else if (slider == &speedSlider)
        audio.setSpeed(speedSlider.getValue());
}

void MainComponent::timerCallback() { updateUI(); }

void MainComponent::updateUI()
{
    fileLabel.setText(audio.getCurrentFileName().isEmpty()
        ? "No file loaded" : audio.getCurrentFileName(), juce::dontSendNotification);

    double pos = audio.getPosition();
    double len = audio.getLength();
    positionSlider.setValue(len > 0 ? pos / len : 0.0, juce::dontSendNotification);

    auto toTime = [](double sec)
    {
        int s = (int)sec % 60;
        int m = (int)(sec / 60);
        return juce::String::formatted("%02d:%02d", m, s);
    };

    timeLabel.setText(toTime(pos) + " / " + toTime(len), juce::dontSendNotification);

    volumeSlider.setValue(audio.getGain(), juce::dontSendNotification);

    // markers
    juce::String mkStr;
    for (auto m : audio.getMarkers())
        mkStr += toTime(m) + "  ";
    markersLabel.setText("Markers: " + mkStr, juce::dontSendNotification);
}

void MainComponent::refreshPlaylistItems()
{
    playlistItems.clear();
    for (int i = 0; i < audio.getPlaylistSize(); ++i)
        playlistItems.add(audio.getPlaylistFile(i).getFileName());
    playlistBox.updateContent();
}

void MainComponent::changeListenerCallback(juce::ChangeBroadcaster*) { updateUI(); }
