#pragma once

#include "PluginProcessor.h"

//==============================================================================
namespace Pal
{
    const juce::Colour bg       { 0xff1b1815 };
    const juce::Colour bgTop    { 0xff26211c };
    const juce::Colour panel    { 0xff2a2520 };
    const juce::Colour panelEdge{ 0xff3b342c };
    const juce::Colour cream    { 0xffefe6d2 };
    const juce::Colour dim      { 0xffa39a8a };
    const juce::Colour amber    { 0xffe0a34a };
    const juce::Colour amberDim { 0xff7a5a2c };
    const juce::Colour knobBody { 0xff342e28 };
    const juce::Colour track    { 0xff15120f };
}

//==============================================================================
class VaultLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VaultLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
};

//==============================================================================
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const char* paramId, const juce::String& title);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    juce::Slider slider;
    juce::String title;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

//==============================================================================
class TypeSelector : public juce::Component
{
public:
    explicit TypeSelector (juce::RangedAudioParameter& param);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> cell (int i) const;
    int hitTest (juce::Point<float>) const;

    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    int selected = 0, hover = -1;
};

//==============================================================================
class DecayDisplay : public juce::Component, private juce::Timer
{
public:
    explicit DecayDisplay (juce::AudioProcessorValueTreeState& state);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    juce::AudioProcessorValueTreeState& state;
    std::array<float, 8> last {};
};

//==============================================================================
class EchoVaultEditor : public juce::AudioProcessorEditor
{
public:
    explicit EchoVaultEditor (EchoVaultProcessor&);
    ~EchoVaultEditor() override;
    void resized() override;

    static constexpr int baseW = 920, baseH = 560;

private:
    class Content : public juce::Component, private juce::Timer
    {
    public:
        explicit Content (EchoVaultProcessor&);
        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        void timerCallback() override;
        void refreshPresetBox();

        EchoVaultProcessor& proc;
        juce::ComboBox presetBox;
        juce::TextButton prevButton { "<" }, nextButton { ">" };
        TypeSelector typeSelector;
        DecayDisplay display;
        std::vector<std::unique_ptr<Knob>> spaceKnobs, toneKnobs;
        int shownProgram = -1;
        juce::Rectangle<int> roomPanel, displayPanel, spacePanel, tonePanel;
    };

    VaultLookAndFeel lnf;
    Content content;
};
