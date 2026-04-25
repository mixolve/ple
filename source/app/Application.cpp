#include <JuceHeader.h>
#include "App.h"

class PleApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override       { return "ple"; }
    const juce::String getApplicationVersion() override    { return "0.1"; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    void initialise (const juce::String&) override
    {
       #if JUCE_IOS
        juce::Desktop::getInstance().setOrientationsEnabled (juce::Desktop::upright);
       #endif

        mainWindow.reset (new MainWindow());
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted (const juce::String&) override {}

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow()
            : DocumentWindow (juce::String(), juce::Colours::black, 0)
        {
            setUsingNativeTitleBar (false);
            setTitleBarButtonsRequired (0, false);
            setTitleBarHeight (0);
            setContentOwned (new MainComponent(), true);
            const auto userArea = juce::Desktop::getInstance().getDisplays().getMainDisplay().userArea;
            setResizeLimits (userArea.getWidth(), 248, userArea.getWidth(), 248);
            setBounds (userArea.getX(), userArea.getY(), userArea.getWidth(), 248);
            setResizable (false, false);
            setDraggable (false);
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (PleApplication)
