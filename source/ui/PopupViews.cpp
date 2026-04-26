#include "ui/PopupViews.h"
#include "BinaryData.h"

namespace
{
const auto popupUiGrey800 = juce::Colour (0xff242424);
const auto popupUiGrey700 = juce::Colour (0xff363636);
const auto popupUiGrey500 = juce::Colour (0xff707070);
const auto popupUiWhite = juce::Colour (0xffffffff);
const auto popupUiAccentPeach = juce::Colour (0xffffcc99);

constexpr int popupUiButtonHeight = 30;
constexpr int popupUiListTopInset = 8;
constexpr int popupUiListSideInset = 8;
constexpr int popupUiListBottomInset = 8;
constexpr int popupUiListRowGap = 8;
constexpr int popupUiListRowPitch = popupUiButtonHeight + popupUiListRowGap;
constexpr float popupUiFontSize = 22.0f;

juce::Font makePopupUiFont (const bool bold = false, const float height = popupUiFontSize)
{
    jassert (juce::approximatelyEqual (height, popupUiFontSize));

#if JUCE_TARGET_HAS_BINARY_DATA
    static const auto regularTypeface = juce::Typeface::createSystemTypefaceFor (BinaryData::SometypeMonoRegular_ttf,
                                                                                  BinaryData::SometypeMonoRegular_ttfSize);
    static const auto boldTypeface = juce::Typeface::createSystemTypefaceFor (BinaryData::SometypeMonoBold_ttf,
                                                                               BinaryData::SometypeMonoBold_ttfSize);

    const auto typeface = bold ? boldTypeface : regularTypeface;

    if (typeface != nullptr)
        return juce::Font (juce::FontOptions (typeface).withHeight (height));
#endif

    return juce::Font (juce::FontOptions ("Sometype Mono", height, bold ? juce::Font::bold : juce::Font::plain));
}
}

void GreyViewport::paint (juce::Graphics& g)
{
    g.fillAll (popupUiGrey800);
}

PluginWindowFrame::PluginWindowFrame (std::unique_ptr<juce::Component> contentToOwn)
    : content (std::move (contentToOwn))
{
    jassert (content != nullptr);
    setOpaque (true);

    if (content != nullptr)
        content->setOpaque (true);

    addAndMakeVisible (*content);
}

void PluginWindowFrame::setPaintCallback (PaintCallback callback)
{
    paintCallback = std::move (callback);
    paintCallbackScheduled = false;
}

void PluginWindowFrame::paint (juce::Graphics& g)
{
    g.setColour (popupUiGrey800);
    g.fillAll();

    g.setColour (popupUiGrey500);
    g.drawRect (getLocalBounds(), 1);

    if (paintCallback != nullptr && ! paintCallbackScheduled)
    {
        paintCallbackScheduled = true;
        const auto callback = paintCallback;

        juce::MessageManager::callAsync ([callback]
        {
            if (callback != nullptr)
                callback();
        });
    }
}

void PluginWindowFrame::resized()
{
    if (content != nullptr)
        content->setBounds (getLocalBounds().reduced (1));
}

juce::Component* PluginWindowFrame::getContentComponent() const noexcept
{
    return content.get();
}

PluginMenuContent::PluginMenuContent (std::vector<juce::PluginDescription> items,
                                      SelectionCallback selectionCallback,
                                      SelectedIndexGetter selectedIndexGetter)
{
    setOpaque (true);
    addAndMakeVisible (viewport);
    viewport.setOpaque (true);
    viewport.setViewedComponent (&surface, false);
    viewport.setScrollBarsShown (false, false, false, false);
    viewport.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::all);
    viewport.setScrollBarThickness (4);

    surface.setOpaque (true);

    surface.setDescriptions (items, std::move (selectionCallback), std::move (selectedIndexGetter));
}

void PluginMenuContent::resized()
{
    viewport.setBounds (getLocalBounds());
    surface.setSize (viewport.getWidth(), surface.getContentHeight());
}

void PluginMenuContent::Surface::setDescriptions (const std::vector<juce::PluginDescription>& items,
                                                  SelectionCallback callback,
                                                  SelectedIndexGetter selectedIndexGetter)
{
    descriptions = items;
    onSelect = std::move (callback);
    getSelectedIndex = std::move (selectedIndexGetter);
    hoveredIndex = -1;
    pressedIndex = -1;
    tapCandidate = false;
    repaint();
}

int PluginMenuContent::Surface::getContentHeight() const
{
    const auto itemCount = static_cast<int> (descriptions.size());
    return popupUiListTopInset + (itemCount <= 0 ? popupUiButtonHeight
                                                 : (itemCount * popupUiListRowPitch) - popupUiListRowGap)
         + popupUiListBottomInset;
}

void PluginMenuContent::Surface::paint (juce::Graphics& g)
{
    g.setColour (popupUiGrey800);
    g.fillAll();

    for (size_t index = 0; index < descriptions.size(); ++index)
    {
        const auto rowBounds = juce::Rectangle<int> (popupUiListSideInset,
                                                     popupUiListTopInset + static_cast<int> (index) * popupUiListRowPitch,
                                                     juce::jmax (0, getWidth() - (popupUiListSideInset * 2)),
                                                     popupUiButtonHeight).reduced (1, 0);

        const auto isSelected = getSelectedIndex != nullptr && static_cast<int> (index) == getSelectedIndex();
        const auto isHighlighted = static_cast<int> (index) == hoveredIndex;
        const auto isPressed = static_cast<int> (index) == pressedIndex;

        g.setColour (isSelected || isHighlighted || isPressed ? popupUiGrey700 : popupUiGrey800);
        g.fillRect (rowBounds);
        g.setColour (isPressed ? popupUiGrey500 : isSelected ? popupUiAccentPeach : popupUiGrey500);
        g.drawRect (rowBounds, 1);

        g.setColour (popupUiWhite);
        g.setFont (makePopupUiFont());
        g.drawText (descriptions[index].name.toUpperCase(), rowBounds.reduced (10, 0), juce::Justification::centredLeft, true);
    }
}

void PluginMenuContent::Surface::mouseMove (const juce::MouseEvent& event)
{
    updateHoveredRow (event.position.y);
}

void PluginMenuContent::Surface::mouseExit (const juce::MouseEvent&)
{
    updateHoveredRow (-1.0f);
}

void PluginMenuContent::Surface::mouseDown (const juce::MouseEvent& event)
{
    pressedIndex = getRowIndexForY (event.position.y);
    pressPosition = event.position;
    tapCandidate = true;
    repaint();
}

void PluginMenuContent::Surface::mouseDrag (const juce::MouseEvent& event)
{
    if (! tapCandidate)
        return;

    if (event.position.getDistanceFrom (pressPosition) > (float) tapThresholdPixels)
    {
        pressedIndex = -1;
        tapCandidate = false;
        repaint();
    }
}

void PluginMenuContent::Surface::mouseUp (const juce::MouseEvent& event)
{
    const auto rowIndex = getRowIndexForY (event.position.y);

    if (tapCandidate && rowIndex >= 0 && rowIndex == pressedIndex && onSelect)
        onSelect (rowIndex);

    pressedIndex = -1;
    tapCandidate = false;
}

int PluginMenuContent::Surface::getRowIndexForY (float y) const noexcept
{
    if (y < 0.0f)
        return -1;

    const auto yPixels = static_cast<int> (y);
    if (yPixels < popupUiListTopInset)
        return -1;

    const auto rowIndex = (yPixels - popupUiListTopInset) / popupUiListRowPitch;

    if (! juce::isPositiveAndBelow (rowIndex, static_cast<int> (descriptions.size())))
        return -1;

    if (((yPixels - popupUiListTopInset) % popupUiListRowPitch) >= popupUiButtonHeight)
        return -1;

    return rowIndex;
}

void PluginMenuContent::Surface::updateHoveredRow (float y)
{
    const auto nextHoveredIndex = getRowIndexForY (y);

    if (hoveredIndex != nextHoveredIndex)
    {
        hoveredIndex = nextHoveredIndex;
        repaint();
    }
}

FileBrowserContent::FileBrowserContent (std::vector<Row> items,
                                        SelectionCallback selectionCallback,
                                        FolderPlayCallback folderPlayCallback)
{
    setOpaque (true);
    addAndMakeVisible (viewport);
    viewport.setOpaque (true);
    viewport.setViewedComponent (&surface, false);
    viewport.setScrollBarsShown (false, false, false, false);
    viewport.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::all);
    viewport.setScrollBarThickness (4);

    surface.setOpaque (true);

    surface.setRows (std::move (items), std::move (selectionCallback), std::move (folderPlayCallback));
}

void FileBrowserContent::setRows (std::vector<Row> items)
{
    const auto viewPosition = viewport.getViewPosition();

    surface.setRows (std::move (items));
    surface.setSize (viewport.getWidth(), surface.getContentHeight());
    viewport.setViewPosition (viewPosition);
}

void FileBrowserContent::resized()
{
    viewport.setBounds (getLocalBounds());
    surface.setSize (viewport.getWidth(), surface.getContentHeight());
}

void FileBrowserContent::Surface::setRows (const std::vector<Row>& items,
                                           SelectionCallback callback,
                                           FolderPlayCallback folderPlayCallback)
{
    rows = items;
    onSelect = std::move (callback);
    onPlayFolder = std::move (folderPlayCallback);
    hoveredIndex = -1;
    hoveredPlayIndex = -1;
    pressedIndex = -1;
    pressedOnPlayButton = false;
    tapCandidate = false;
    repaint();
}

void FileBrowserContent::Surface::setRows (std::vector<Row> items)
{
    rows = std::move (items);
    hoveredIndex = -1;
    hoveredPlayIndex = -1;
    pressedIndex = -1;
    pressedOnPlayButton = false;
    tapCandidate = false;
    repaint();
}

int FileBrowserContent::Surface::getContentHeight() const
{
    const auto itemCount = static_cast<int> (rows.size());
    return popupUiListTopInset + (itemCount <= 0 ? popupUiButtonHeight
                                                 : (itemCount * popupUiListRowPitch) - popupUiListRowGap)
         + popupUiListBottomInset;
}

void FileBrowserContent::Surface::paint (juce::Graphics& g)
{
    g.setColour (popupUiGrey800);
    g.fillAll();

    for (size_t index = 0; index < rows.size(); ++index)
    {
        const auto rowBounds = getRowBoundsForIndex (static_cast<int> (index));

        const auto isHighlighted = static_cast<int> (index) == hoveredIndex;
        const auto isSelected = rows[index].isSelected;
        const auto isPathActive = rows[index].isPathActive;
        const auto hasAccent = isSelected || isPathActive;
        const auto hasFolderPlayButton = rows[index].isDirectory;
        const auto isPressed = static_cast<int> (index) == pressedIndex;

        auto labelArea = rowBounds;
        juce::Rectangle<int> playArea;

        if (hasFolderPlayButton)
        {
            const auto playWidth = juce::jmax (1, juce::roundToInt (rowBounds.getWidth() * 0.10f));
            playArea = labelArea.removeFromRight (playWidth);
        }

        g.setColour (hasAccent || isHighlighted || isPressed ? popupUiGrey700 : popupUiGrey800);
        g.fillRect (labelArea);
        g.setColour (isPressed ? popupUiGrey500 : hasAccent ? popupUiAccentPeach : popupUiGrey500);
        g.drawRect (labelArea, 1);

        if (hasFolderPlayButton)
        {
            const auto isPlayActive = isPathActive;
            const auto isPlayPressed = isPressed && pressedOnPlayButton;
            g.setColour (isPlayActive || (isHighlighted && hoveredPlayIndex == static_cast<int> (index)) || isPlayPressed ? popupUiGrey700 : popupUiGrey800);
            g.fillRect (playArea);
            g.setColour (isPlayPressed ? popupUiGrey500 : isPlayActive ? popupUiAccentPeach : popupUiGrey500);
            g.drawRect (playArea, 1);

            juce::Path triangle;
            const auto triangleBounds = playArea.reduced (juce::jmax (2, playArea.getWidth() / 4), juce::jmax (2, playArea.getHeight() / 4));
            triangle.addTriangle ((float) triangleBounds.getX(),
                                  (float) triangleBounds.getY(),
                                  (float) triangleBounds.getX(),
                                  (float) triangleBounds.getBottom(),
                                  (float) triangleBounds.getRight(),
                                  (float) triangleBounds.getCentreY());
            g.setColour (isPlayActive ? popupUiAccentPeach : popupUiWhite);
            g.fillPath (triangle);
        }

        g.setColour (popupUiWhite);
        g.setFont (makePopupUiFont());
        g.drawText (rows[index].label.toUpperCase(), labelArea.reduced (10, 0), juce::Justification::centredLeft, true);
    }
}

void FileBrowserContent::Surface::mouseMove (const juce::MouseEvent& event)
{
    hoveredPlayIndex = getPlayButtonRowIndexForPosition (event.position);
    updateHoveredRow (event.position.y);
}

void FileBrowserContent::Surface::mouseExit (const juce::MouseEvent&)
{
    hoveredPlayIndex = -1;
    updateHoveredRow (-1.0f);
}

void FileBrowserContent::Surface::mouseDown (const juce::MouseEvent& event)
{
    pressedIndex = getRowIndexForY (event.position.y);
    pressPosition = event.position;
    pressedOnPlayButton = pressedIndex >= 0 && isPlayButtonHit (pressedIndex, event.position.x);
    tapCandidate = true;
    repaint();
}

void FileBrowserContent::Surface::mouseDrag (const juce::MouseEvent& event)
{
    if (! tapCandidate)
        return;

    if (event.position.getDistanceFrom (pressPosition) > (float) tapThresholdPixels)
    {
        pressedIndex = -1;
        pressedOnPlayButton = false;
        tapCandidate = false;
        repaint();
    }
}

void FileBrowserContent::Surface::mouseUp (const juce::MouseEvent& event)
{
    const auto rowIndex = getRowIndexForY (event.position.y);

    if (tapCandidate && rowIndex >= 0 && rowIndex == pressedIndex)
    {
        if (pressedOnPlayButton && rows[static_cast<size_t> (rowIndex)].isDirectory)
        {
            if (onPlayFolder)
                onPlayFolder (rowIndex);
        }
        else if (onSelect)
        {
            onSelect (rowIndex);
        }
    }

    pressedIndex = -1;
    pressedOnPlayButton = false;
    tapCandidate = false;
    repaint();
}

juce::Rectangle<int> FileBrowserContent::Surface::getRowBoundsForIndex (int index) const
{
    return juce::Rectangle<int> (popupUiListSideInset,
                                 popupUiListTopInset + index * popupUiListRowPitch,
                                 juce::jmax (0, getWidth() - (popupUiListSideInset * 2)),
                                 popupUiButtonHeight).reduced (1, 0);
}

int FileBrowserContent::Surface::getRowIndexForY (float y) const noexcept
{
    if (y < 0.0f)
        return -1;

    const auto yPixels = static_cast<int> (y);
    if (yPixels < popupUiListTopInset)
        return -1;

    const auto rowIndex = (yPixels - popupUiListTopInset) / popupUiListRowPitch;

    if (! juce::isPositiveAndBelow (rowIndex, static_cast<int> (rows.size())))
        return -1;

    if (((yPixels - popupUiListTopInset) % popupUiListRowPitch) >= popupUiButtonHeight)
        return -1;

    return rowIndex;
}

int FileBrowserContent::Surface::getPlayButtonRowIndexForPosition (juce::Point<float> position) const noexcept
{
    const auto rowIndex = getRowIndexForY (position.y);

    if (rowIndex < 0 || ! rows[static_cast<size_t> (rowIndex)].isDirectory)
        return -1;

    if (! isPlayButtonHit (rowIndex, position.x))
        return -1;

    return rowIndex;
}

bool FileBrowserContent::Surface::isPlayButtonHit (int rowIndex, float x) const noexcept
{
    if (! juce::isPositiveAndBelow (rowIndex, static_cast<int> (rows.size())))
        return false;

    if (! rows[static_cast<size_t> (rowIndex)].isDirectory)
        return false;

    const auto rowBounds = getRowBoundsForIndex (rowIndex);
    const auto playWidth = juce::jmax (1, juce::roundToInt (rowBounds.getWidth() * 0.10f));
    const auto playStartX = rowBounds.getRight() - playWidth;

    return x >= (float) playStartX;
}

void FileBrowserContent::Surface::updateHoveredRow (float y)
{
    const auto nextHoveredIndex = getRowIndexForY (y);

    if (hoveredIndex != nextHoveredIndex)
    {
        hoveredIndex = nextHoveredIndex;
        repaint();
    }
}

NowPlayingContent::NowPlayingContent (SwipeCallback previousTrackActionToOwn,
                                      SwipeCallback nextTrackActionToOwn)
    : previousTrackAction (std::move (previousTrackActionToOwn))
    , nextTrackAction (std::move (nextTrackActionToOwn))
{
    setOpaque (true);
    setInterceptsMouseClicks (true, false);
}

void NowPlayingContent::setTrack (const ple::NowPlayingTrack& track)
{
    nowPlayingTrack = track;
    repaint();
}

juce::String NowPlayingContent::formatTimeText (double seconds)
{
    if (seconds < 0.0)
        return "--:--";

    const auto totalSeconds = juce::jmax (0, juce::roundToInt (seconds));
    const auto hours = totalSeconds / 3600;
    const auto minutes = (totalSeconds % 3600) / 60;
    const auto secs = totalSeconds % 60;

    const auto twoDigits = [] (int value)
    {
        return value < 10 ? juce::String ("0") + juce::String (value)
                          : juce::String (value);
    };

    if (hours > 0)
        return juce::String (hours) + ":" + twoDigits (minutes) + ":" + twoDigits (secs);

    return juce::String (minutes) + ":" + twoDigits (secs);
}

void NowPlayingContent::paint (juce::Graphics& g)
{
    g.setColour (popupUiGrey800);
    g.fillAll();

    auto contentArea = getLocalBounds().reduced (4, 4);
    const auto title = nowPlayingTrack.title.isNotEmpty() ? nowPlayingTrack.title : juce::String ("NO TRACK");
    const auto subtitle = nowPlayingTrack.artist.isNotEmpty() ? nowPlayingTrack.artist : juce::String ("UNKNOWN ARTIST");

    const auto textLineHeight = juce::roundToInt (popupUiFontSize);
    const auto textGap = 6;
    const auto artworkBounds = getArtworkBounds();

    g.setColour (popupUiGrey700);
    g.fillRect (artworkBounds);
    g.setColour (popupUiGrey500);
    g.drawRect (artworkBounds, 1);

    if (nowPlayingTrack.artwork.isValid())
    {
        g.drawImageWithin (nowPlayingTrack.artwork,
                           artworkBounds.getX(),
                           artworkBounds.getY(),
                           artworkBounds.getWidth(),
                           artworkBounds.getHeight(),
                           juce::RectanglePlacement::centred,
                           false);
    }

    auto textArea = contentArea;
    textArea.removeFromTop ((artworkBounds.getY() - contentArea.getY()) + artworkBounds.getHeight() + textGap);

    auto titleArea = textArea.removeFromTop (textLineHeight);
    auto subtitleArea = textArea.removeFromTop (textLineHeight);

    g.setColour (popupUiWhite);
    g.setFont (makePopupUiFont());
    g.drawFittedText (title.toUpperCase(), titleArea, juce::Justification::centred, 1, 1.0f);
    g.drawFittedText (subtitle.toUpperCase(), subtitleArea, juce::Justification::centred, 1, 1.0f);
}

juce::Rectangle<int> NowPlayingContent::getArtworkBounds() const
{
    auto contentArea = getLocalBounds().reduced (4, 4);
    const auto textLineHeight = juce::roundToInt (popupUiFontSize);
    const auto textGap = 6;
    const auto maxArtworkWidth = juce::jmax (1, juce::roundToInt (contentArea.getWidth() * 0.9f));
    const auto maxArtworkHeight = juce::jmax (1, contentArea.getHeight() - (textLineHeight * 2) - textGap);
    const auto artworkAspectRatio = nowPlayingTrack.artwork.isValid() && nowPlayingTrack.artwork.getHeight() > 0
                                        ? (float) nowPlayingTrack.artwork.getWidth() / (float) nowPlayingTrack.artwork.getHeight()
                                        : 1.0f;

    auto artworkArea = juce::Rectangle<float> (0.0f, 0.0f, (float) maxArtworkWidth, (float) maxArtworkHeight);

    if (artworkArea.getWidth() / artworkArea.getHeight() > artworkAspectRatio)
        artworkArea.setWidth (artworkArea.getHeight() * artworkAspectRatio);
    else
        artworkArea.setHeight (artworkArea.getWidth() / artworkAspectRatio);

    const auto blockHeight = juce::roundToInt (artworkArea.getHeight()) + textGap + (textLineHeight * 2);
    const auto blockTop = contentArea.getY() + juce::jmax (0, (contentArea.getHeight() - blockHeight) / 2);

    artworkArea.setX ((float) contentArea.getCentreX() - (artworkArea.getWidth() / 2.0f));
    artworkArea.setY ((float) blockTop);

    return artworkArea.toNearestInt();
}

void NowPlayingContent::mouseMove (const juce::MouseEvent& event)
{
    setMouseCursor (getArtworkBounds().contains (event.getPosition())
                        ? juce::MouseCursor::PointingHandCursor
                        : juce::MouseCursor::NormalCursor);
}

void NowPlayingContent::mouseExit (const juce::MouseEvent&)
{
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void NowPlayingContent::mouseDown (const juce::MouseEvent& event)
{
    swipeCandidate = getArtworkBounds().contains (event.getPosition());
    swipeStartPosition = event.position;
}

void NowPlayingContent::mouseUp (const juce::MouseEvent& event)
{
    if (! swipeCandidate)
        return;

    swipeCandidate = false;

    const auto deltaX = event.position.x - swipeStartPosition.x;
    const auto deltaY = event.position.y - swipeStartPosition.y;

    if (std::abs (deltaX) < swipeThresholdPixels || std::abs (deltaX) < std::abs (deltaY))
        return;

    if (deltaX < 0.0f)
    {
        if (nextTrackAction)
            nextTrackAction();
    }
    else
    {
        if (previousTrackAction)
            previousTrackAction();
    }
}

AboutContent::AboutContent()
{
    setOpaque (true);
    setInterceptsMouseClicks (true, false);

    auto markdownText = juce::String::fromUTF8 (BinaryData::about_md, BinaryData::about_mdSize);
    markdownLines.addLines (markdownText);

    if (markdownLines.size() > 0)
    {
        const auto firstLine = markdownLines[0].trim();

        if (firstLine.startsWithChar ('['))
        {
            const auto closeLabel = firstLine.indexOfChar (']');
            const auto openUrl = firstLine.indexOfChar ('(');
            const auto closeUrl = firstLine.lastIndexOfChar (')');

            if (closeLabel > 1 && openUrl == closeLabel + 1 && closeUrl > openUrl + 1)
            {
                linkText = firstLine.substring (1, closeLabel);
                linkUrl = juce::URL (firstLine.substring (openUrl + 1, closeUrl));
            }
        }

        if (linkText.isEmpty())
            linkText = firstLine;
    }

    if (linkText.isEmpty())
        linkText = "all-in-web";
}

void AboutContent::paint (juce::Graphics& g)
{
    g.setColour (popupUiGrey800);
    g.fillAll();

    g.setFont (makePopupUiFont());

    for (int i = 0; i < markdownLines.size(); ++i)
    {
        const auto lineBoundsForRow = lineBounds[static_cast<size_t> (i)];
        const auto isLinkLine = i == 0;

        g.setColour (isLinkLine ? popupUiAccentPeach : popupUiWhite);

        if (isLinkLine)
            g.drawFittedText (linkText, lineBoundsForRow, juce::Justification::centred, 1, 1.0f);
        else
            g.drawFittedText (markdownLines[i], lineBoundsForRow, juce::Justification::centred, 1, 1.0f);
    }
}

void AboutContent::resized()
{
    auto contentArea = getLocalBounds().reduced (12);
    const auto textHeight = juce::roundToInt (makePopupUiFont().getHeight()) + 6;
    const auto lineGap = 8;
    const auto lineHeight = textHeight;
    const auto blockHeight = (markdownLines.size() * lineHeight) + juce::jmax (0, (markdownLines.size() - 1) * lineGap);
    const auto top = contentArea.getY() + juce::jmax (0, (contentArea.getHeight() - blockHeight) / 2);

    lineBounds.clear();
    lineBounds.reserve (static_cast<size_t> (markdownLines.size()));

    auto currentY = top;

    for (int i = 0; i < markdownLines.size(); ++i)
    {
        const auto rowBounds = juce::Rectangle<int> (contentArea.getX(), currentY, contentArea.getWidth(), lineHeight);
        lineBounds.push_back (rowBounds);

        if (i == 0)
            linkBounds = rowBounds;

        currentY += lineHeight + lineGap;
    }
}

void AboutContent::mouseMove (const juce::MouseEvent& event)
{
    setMouseCursor (linkBounds.contains (event.getPosition())
                        ? juce::MouseCursor::PointingHandCursor
                        : juce::MouseCursor::NormalCursor);
}

void AboutContent::mouseExit (const juce::MouseEvent&)
{
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void AboutContent::mouseUp (const juce::MouseEvent& event)
{
    if (linkUrl.isWellFormed() && linkBounds.contains (event.getPosition()))
        linkUrl.launchInDefaultBrowser();
}