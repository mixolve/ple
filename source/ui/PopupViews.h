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

    struct Row
    {
        juce::String label;
        bool isSelected = false;
        bool isPathActive = false;
        bool isDirectory = false;
    };

    FileBrowserContent (std::vector<Row> items,
                        SelectionCallback selectionCallback,
                        FolderPlayCallback folderPlayCallback);

    void setRows (std::vector<Row> items);
    void resized() override;

private:
    class Surface final : public juce::Component
    {
    public:
        void setRows (const std::vector<Row>& items,
                      SelectionCallback callback,
                      FolderPlayCallback folderPlayCallback);
        void setRows (std::vector<Row> items);

        int getContentHeight() const;

        void paint (juce::Graphics& g) override;
        void mouseMove (const juce::MouseEvent& event) override;
        void mouseExit (const juce::MouseEvent&) override;
        void mouseDown (const juce::MouseEvent& event) override;
        void mouseDrag (const juce::MouseEvent& event) override;
        void mouseUp (const juce::MouseEvent& event) override;

    private:
        juce::Rectangle<int> getRowBoundsForIndex (int index) const;
        int getRowIndexForY (float y) const noexcept;
        int getPlayButtonRowIndexForPosition (juce::Point<float> position) const noexcept;
        bool isPlayButtonHit (int rowIndex, float x) const noexcept;
        void updateHoveredRow (float y);

        std::vector<Row> rows;
        SelectionCallback onSelect;
        FolderPlayCallback onPlayFolder;
        int hoveredIndex = -1;
        int hoveredPlayIndex = -1;
        int pressedIndex = -1;
        juce::Point<float> pressPosition;
        bool pressedOnPlayButton = false;
        bool tapCandidate = false;
        static constexpr int tapThresholdPixels = 8;
    };

    GreyViewport viewport;
    Surface surface;
};

class NowPlayingContent final : public juce::Component
{
public:
    using SwipeCallback = std::function<void()>;

    NowPlayingContent (SwipeCallback previousTrackAction = {},
                       SwipeCallback nextTrackAction = {});

    void setTrack (const ple::NowPlayingTrack& track);
    void paint (juce::Graphics& g) override;
    void mouseMove (const juce::MouseEvent& event) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;

private:
    static juce::String formatTimeText (double seconds);
    juce::Rectangle<int> getArtworkBounds() const;

    ple::NowPlayingTrack nowPlayingTrack;
    SwipeCallback previousTrackAction;
    SwipeCallback nextTrackAction;
    juce::Point<float> swipeStartPosition;
    bool swipeCandidate = false;
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