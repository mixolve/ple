#pragma once

#include <JuceHeader.h>
#include "audio/PlaybackController.h"
#include <functional>
#include <memory>
#include <vector>

class PluginWindowFrame final : public juce::Component
{
public:
    using PaintCallback = std::function<void()>;

    explicit PluginWindowFrame (std::unique_ptr<juce::Component> contentToOwn);

    void setPaintCallback (PaintCallback callback);

    void paint (juce::Graphics& g) override;
    void resized() override;

    juce::Component* getContentComponent() const noexcept;

private:
    std::unique_ptr<juce::Component> content;
    PaintCallback paintCallback;
    bool paintCallbackScheduled = false;
};

class GreyViewport final : public juce::Viewport
{
public:
    void paint (juce::Graphics& g) override;
};

class PluginMenuContent final : public juce::Component
{
public:
    using SelectionCallback = std::function<void(int)>;
    using SelectedIndexGetter = std::function<int()>;

    PluginMenuContent (std::vector<juce::PluginDescription> items,
                       SelectionCallback selectionCallback,
                       SelectedIndexGetter selectedIndexGetter);

    void resized() override;

private:
    class Surface final : public juce::Component
    {
    public:
        void setDescriptions (const std::vector<juce::PluginDescription>& items,
                              SelectionCallback callback,
                              SelectedIndexGetter selectedIndexGetter);

        int getContentHeight() const;

        void paint (juce::Graphics& g) override;
        void mouseMove (const juce::MouseEvent& event) override;
        void mouseExit (const juce::MouseEvent&) override;
        void mouseDown (const juce::MouseEvent& event) override;
        void mouseDrag (const juce::MouseEvent& event) override;
        void mouseUp (const juce::MouseEvent& event) override;

    private:
        int getRowIndexForY (float y) const noexcept;
        void updateHoveredRow (float y);

        std::vector<juce::PluginDescription> descriptions;
        SelectionCallback onSelect;
        SelectedIndexGetter getSelectedIndex;
        int hoveredIndex = -1;
        int pressedIndex = -1;
        juce::Point<float> pressPosition;
        bool tapCandidate = false;
        static constexpr int tapThresholdPixels = 8;
    };

    GreyViewport viewport;
    Surface surface;
};

class FileBrowserContent final : public juce::Component
{
public:
    using SelectionCallback = std::function<void(int)>;
    using FolderPlayCallback = std::function<void(int)>;
    using LongPressCallback = std::function<void(int)>;

    struct Row
    {
        juce::String label;
        bool isSelected = false;
        bool isPathActive = false;
        bool isDirectory = false;
        bool isMarked = false;
    };

    FileBrowserContent (std::vector<Row> items,
                        SelectionCallback selectionCallback,
                        FolderPlayCallback folderPlayCallback,
                        LongPressCallback longPressCallback = {});

    void setRows (std::vector<Row> items);
    void resized() override;

private:
    class Surface final : public juce::Component
                         , private juce::Timer
    {
    public:
        void setRows (const std::vector<Row>& items,
                      SelectionCallback callback,
                      FolderPlayCallback folderPlayCallback,
                      LongPressCallback longPressCallback);
        void setRows (std::vector<Row> items);

        int getContentHeight() const;

        void paint (juce::Graphics& g) override;
        void mouseMove (const juce::MouseEvent& event) override;
        void mouseExit (const juce::MouseEvent&) override;
        void mouseDown (const juce::MouseEvent& event) override;
        void mouseDrag (const juce::MouseEvent& event) override;
        void mouseUp (const juce::MouseEvent& event) override;

        void timerCallback() override;

    private:
        juce::Rectangle<int> getRowBoundsForIndex (int index) const;
        int getRowIndexForY (float y) const noexcept;
        int getPlayButtonRowIndexForPosition (juce::Point<float> position) const noexcept;
        bool isPlayButtonHit (int rowIndex, float x) const noexcept;
        void repaintRow (int rowIndex);
        void updateHoveredRow (float y);
        void resetPressState();

        std::vector<Row> rows;
        SelectionCallback onSelect;
        FolderPlayCallback onPlayFolder;
        LongPressCallback onLongPress;
        int hoveredIndex = -1;
        int hoveredPlayIndex = -1;
        int pressedIndex = -1;
        juce::Point<float> pressPosition;
        bool pressedOnPlayButton = false;
        bool tapCandidate = false;
        bool longPressCandidate = false;
        bool longPressTriggered = false;
        bool pressedRowPointerInside = false;
        static constexpr int tapThresholdPixels = 8;
        static constexpr int longPressThresholdMs = 1000;
    };

    GreyViewport viewport;
    Surface surface;
};

class BrowserActionContent final : public juce::Component
                               , private juce::Timer
{
public:
    using ActionCallback = std::function<void()>;

    BrowserActionContent (juce::String fileLabel,
                          bool isMarked,
                          bool canRemove,
                          ActionCallback removeCallback,
                          ActionCallback toggleMarkCallback,
                          ActionCallback dismissCallback = {});

    void resized() override;
    void paint (juce::Graphics& g) override;
    void mouseMove (const juce::MouseEvent& event) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void timerCallback() override;

private:
    bool isInsideRemoveButton (juce::Point<float> position) const noexcept;
    bool isInsideMarkButton (juce::Point<float> position) const noexcept;
    void resetPressState();

    juce::String fileLabel;
    bool isMarked = false;
    bool canRemove = false;
    ActionCallback removeCallback;
    ActionCallback toggleMarkCallback;
    ActionCallback dismissCallback;
    juce::Rectangle<int> titleBounds;
    juce::Rectangle<int> removeButtonBounds;
    juce::Rectangle<int> markButtonBounds;
    juce::Point<float> pressPosition;
    bool removePressed = false;
    bool removeLongPressReady = false;
    bool removePointerInside = false;
    bool markPressed = false;
    bool markPointerInside = false;
};

class NowPlayingContent final : public juce::Component
{
public:
    using SwipeCallback = std::function<void()>;
    using SeekCallback = std::function<void()>;

    NowPlayingContent (SwipeCallback previousTrackAction = {},
                       SwipeCallback nextTrackAction = {},
                       SeekCallback seekBackwardAction = {},
                       SeekCallback seekForwardAction = {});

    void setTrack (const ple::NowPlayingTrack& track);
    void paint (juce::Graphics& g) override;
    void mouseMove (const juce::MouseEvent& event) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;

private:
    static juce::String formatTimeText (double seconds);
    juce::Rectangle<int> getArtworkBounds() const;
    juce::Rectangle<int> getSeekBackwardBounds() const;
    juce::Rectangle<int> getSeekForwardBounds() const;

    ple::NowPlayingTrack nowPlayingTrack;
    SwipeCallback previousTrackAction;
    SwipeCallback nextTrackAction;
    SeekCallback seekBackwardAction;
    SeekCallback seekForwardAction;
    juce::Point<float> swipeStartPosition;
    bool swipeCandidate = false;
    bool seekBackwardPressed = false;
    bool seekForwardPressed = false;
    static constexpr int swipeThresholdPixels = 12;
};

class AboutContent final : public juce::Component
{
public:
    AboutContent();

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent& event) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent& event) override;

private:
    juce::String linkText;
    juce::URL linkUrl;
    juce::StringArray markdownLines;
    std::vector<juce::Rectangle<int>> lineBounds;
    juce::Rectangle<int> linkBounds;
};
