#include "ui/PopupViews.h"
#include "BinaryData.h"

namespace
{
const auto popupUiGrey800 = juce::Colour (0xff242424);
const auto popupUiGrey700 = juce::Colour (0xff363636);
const auto popupUiGrey500 = juce::Colour (0xff707070);
const auto popupUiWhite = juce::Colour (0xffffffff);
const auto popupUiAccentPeach = juce::Colour (0xffffcc99);
const auto popupUiMarkedGrey = juce::Colour (0xff9a9a9a);

constexpr int popupUiButtonHeight = 30;
constexpr int popupUiListTopInset = 8;
constexpr int popupUiListSideInset = 8;
constexpr int popupUiListBottomInset = 8;
constexpr int popupUiListRowGap = 8;
constexpr int popupUiListRowPitch = popupUiButtonHeight + popupUiListRowGap;
constexpr float popupUiFontSize = 22.0f;
constexpr int popupUiActionWidth = 96;
constexpr int popupUiActionHeight = 28;
constexpr int popupUiActionGap = 8;

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

juce_wchar toUppercaseCyrillicChar (const juce_wchar character) noexcept
{
    if (character >= 0x0430 && character <= 0x044f)
        return static_cast<juce_wchar> (character - 0x20);

    if (character >= 0x0450 && character <= 0x045f)
        return static_cast<juce_wchar> (character - 0x50);

    return character;
}

juce::String toPopupUiUppercase (const juce::String& text)
{
    const auto upperText = text.toUpperCase();
    juce::String result;
    auto source = upperText.getCharPointer();

    while (! source.isEmpty())
        result << juce::String::charToString (toUppercaseCyrillicChar (source.getAndAdvance()));

    return result;
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

void PluginWindowFrame::paintOverChildren (juce::Graphics& g)
{
    g.setColour (popupUiGrey500);
    g.drawRect (getLocalBounds(), 1);
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
        g.drawText (toPopupUiUppercase (descriptions[index].name), rowBounds.reduced (10, 0), juce::Justification::centredLeft, true);
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
                                        FolderPlayCallback folderPlayCallback,
                                        LongPressCallback longPressCallback)
{
    setOpaque (true);
    addAndMakeVisible (viewport);
    viewport.setOpaque (true);
    viewport.setViewedComponent (&surface, false);
    viewport.setScrollBarsShown (false, false, false, false);
    viewport.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::all);
    viewport.setScrollBarThickness (4);

    surface.setOpaque (true);

    surface.setRows (std::move (items), std::move (selectionCallback), std::move (folderPlayCallback), std::move (longPressCallback));
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
                                           FolderPlayCallback folderPlayCallback,
                                           LongPressCallback longPressCallback)
{
    rows = items;
    onSelect = std::move (callback);
    onPlayFolder = std::move (folderPlayCallback);
    onLongPress = std::move (longPressCallback);
    hoveredIndex = -1;
    hoveredPlayIndex = -1;
    pressedIndex = -1;
    pressedOnPlayButton = false;
    tapCandidate = false;
    longPressCandidate = false;
    longPressTriggered = false;
    pressedRowPointerInside = false;
    stopTimer();
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
    longPressCandidate = false;
    longPressTriggered = false;
    pressedRowPointerInside = false;
    stopTimer();
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

    const auto rowCount = static_cast<int> (rows.size());

    if (rowCount <= 0)
        return;

    const auto clipBounds = g.getClipBounds();
    const auto firstVisibleRow = juce::jlimit (0,
                                               rowCount - 1,
                                               juce::jmax (0, ((clipBounds.getY() - popupUiListTopInset) / popupUiListRowPitch) - 1));
    const auto lastVisibleRow = juce::jlimit (0,
                                              rowCount - 1,
                                              juce::jmax (0, ((clipBounds.getBottom() - popupUiListTopInset) / popupUiListRowPitch) + 1));

    g.setFont (makePopupUiFont());

    for (int index = firstVisibleRow; index <= lastVisibleRow; ++index)
    {
        const auto rowBounds = getRowBoundsForIndex (index);

        if (! clipBounds.intersects (rowBounds))
            continue;

        const auto& row = rows[static_cast<size_t> (index)];
        const auto isHighlighted = index == hoveredIndex;
        const auto isSelected = row.isSelected;
        const auto isPathActive = row.isPathActive;
        const auto isMarked = row.isMarked;
        const auto hasAccent = isSelected || isPathActive;
        const auto hasFolderPlayButton = row.isDirectory;
        const auto isPressed = index == pressedIndex;

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
            g.setColour (isPlayActive || (isHighlighted && hoveredPlayIndex == index) || isPlayPressed ? popupUiGrey700 : popupUiGrey800);
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

        g.setColour (isMarked ? popupUiMarkedGrey : popupUiWhite);
        g.drawText (toPopupUiUppercase (row.label), labelArea.reduced (10, 0), juce::Justification::centredLeft, true);
    }
}

void FileBrowserContent::Surface::mouseMove (const juce::MouseEvent& event)
{
    const auto previousHoveredPlayIndex = hoveredPlayIndex;
    hoveredPlayIndex = getPlayButtonRowIndexForPosition (event.position);

    if (hoveredPlayIndex != previousHoveredPlayIndex)
    {
        repaintRow (previousHoveredPlayIndex);
        repaintRow (hoveredPlayIndex);
    }

    updateHoveredRow (event.position.y);
}

void FileBrowserContent::Surface::mouseExit (const juce::MouseEvent&)
{
    const auto previousHoveredPlayIndex = hoveredPlayIndex;
    hoveredPlayIndex = -1;
    repaintRow (previousHoveredPlayIndex);
    updateHoveredRow (-1.0f);
}

void FileBrowserContent::Surface::mouseDown (const juce::MouseEvent& event)
{
    pressedIndex = getRowIndexForY (event.position.y);
    pressPosition = event.position;
    pressedOnPlayButton = pressedIndex >= 0 && isPlayButtonHit (pressedIndex, event.position.x);
    tapCandidate = pressedIndex >= 0;
    longPressCandidate = tapCandidate && ! pressedOnPlayButton;
    longPressTriggered = false;
    pressedRowPointerInside = tapCandidate;

    if (longPressCandidate)
        startTimer (longPressThresholdMs);

    repaintRow (pressedIndex);
}

void FileBrowserContent::Surface::mouseDrag (const juce::MouseEvent& event)
{
    if (! tapCandidate)
        return;

    if (event.position.getDistanceFrom (pressPosition) > (float) tapThresholdPixels)
    {
        const auto rowIndex = getRowIndexForY (event.position.y);

        if (longPressCandidate && rowIndex == pressedIndex)
        {
            if (! pressedRowPointerInside)
            {
                pressedRowPointerInside = true;
                repaintRow (pressedIndex);
            }

            return;
        }

        resetPressState();
    }
}

void FileBrowserContent::Surface::mouseUp (const juce::MouseEvent& event)
{
    const auto rowIndex = getRowIndexForY (event.position.y);

    stopTimer();

    if (longPressTriggered)
    {
        resetPressState();
        return;
    }

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

    resetPressState();
}

void FileBrowserContent::Surface::timerCallback()
{
    stopTimer();

    if (! longPressCandidate || pressedIndex < 0 || pressedOnPlayButton || ! pressedRowPointerInside)
        return;

    longPressTriggered = true;
    longPressCandidate = false;
    tapCandidate = false;

    if (onLongPress)
        onLongPress (pressedIndex);
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

void FileBrowserContent::Surface::repaintRow (int rowIndex)
{
    if (! juce::isPositiveAndBelow (rowIndex, static_cast<int> (rows.size())))
        return;

    repaint (getRowBoundsForIndex (rowIndex).expanded (1, 0));
}

void FileBrowserContent::Surface::updateHoveredRow (float y)
{
    const auto nextHoveredIndex = getRowIndexForY (y);

    if (hoveredIndex != nextHoveredIndex)
    {
        const auto previousHoveredIndex = hoveredIndex;
        hoveredIndex = nextHoveredIndex;
        repaintRow (previousHoveredIndex);
        repaintRow (hoveredIndex);
    }
}

void FileBrowserContent::Surface::resetPressState()
{
    const auto previousPressedIndex = pressedIndex;
    stopTimer();
    pressedIndex = -1;
    pressedOnPlayButton = false;
    tapCandidate = false;
    longPressCandidate = false;
    longPressTriggered = false;
    pressedRowPointerInside = false;
    repaintRow (previousPressedIndex);
}

BrowserActionContent::BrowserActionContent (juce::String fileLabelToOwn,
                                            bool isMarkedToOwn,
                                            bool canRemoveToOwn,
                                            ActionCallback removeCallbackToOwn,
                                            ActionCallback toggleMarkCallbackToOwn,
                                            ActionCallback dismissCallbackToOwn)
    : fileLabel (std::move (fileLabelToOwn))
    , isMarked (isMarkedToOwn)
    , canRemove (canRemoveToOwn)
    , removeCallback (std::move (removeCallbackToOwn))
    , toggleMarkCallback (std::move (toggleMarkCallbackToOwn))
    , dismissCallback (std::move (dismissCallbackToOwn))
{
    setOpaque (true);
    setInterceptsMouseClicks (true, true);
}

void BrowserActionContent::resized()
{
    auto area = getLocalBounds().reduced (10);
    titleBounds = area.removeFromTop (36);
    area.removeFromTop (8);

    auto buttonsArea = area.removeFromTop (popupUiActionHeight);
    const auto buttonWidth = juce::jmax (1, juce::jmin (popupUiActionWidth,
                                                        (buttonsArea.getWidth() - (popupUiActionGap * 2)) / 3));
    removeButtonBounds = buttonsArea.removeFromLeft (buttonWidth);
    buttonsArea.removeFromLeft (popupUiActionGap);
    markButtonBounds = buttonsArea.removeFromLeft (buttonWidth);
    buttonsArea.removeFromLeft (popupUiActionGap);
    closeButtonBounds = buttonsArea.removeFromLeft (buttonWidth);
}

void BrowserActionContent::paint (juce::Graphics& g)
{
    g.setColour (popupUiGrey800);
    g.fillAll();

    g.setFont (makePopupUiFont());
    g.setColour (popupUiWhite);
    g.drawFittedText (toPopupUiUppercase (fileLabel), titleBounds, juce::Justification::centred, 1, 1.0f);

    g.setColour (popupUiGrey700);
    g.fillRect (removeButtonBounds);
    g.fillRect (markButtonBounds);
    g.fillRect (closeButtonBounds);

    g.setColour (popupUiGrey500);
    g.drawRect (removeButtonBounds, 1);
    g.drawRect (markButtonBounds, 1);
    g.drawRect (closeButtonBounds, 1);

    g.setColour (popupUiWhite);
    g.drawFittedText (canRemove ? (removeLongPressReady ? "SURE?" : "REMOVE") : "LOCKED",
                       removeButtonBounds,
                       juce::Justification::centred,
                       1,
                       1.0f);
    g.drawFittedText (isMarked ? "UNMARK" : "MARK", markButtonBounds, juce::Justification::centred, 1, 1.0f);
    g.drawFittedText ("CLOSE", closeButtonBounds, juce::Justification::centred, 1, 1.0f);
}

void BrowserActionContent::mouseMove (const juce::MouseEvent& event)
{
    removePointerInside = isInsideRemoveButton (event.position);
    markPointerInside = isInsideMarkButton (event.position);
    closePointerInside = isInsideCloseButton (event.position);

    const auto cursor = isInsideRemoveButton (event.position) || isInsideMarkButton (event.position) || isInsideCloseButton (event.position)
                        ? juce::MouseCursor::PointingHandCursor
                        : juce::MouseCursor::NormalCursor;

    setMouseCursor (cursor);
}

void BrowserActionContent::mouseExit (const juce::MouseEvent&)
{
    setMouseCursor (juce::MouseCursor::NormalCursor);
    removePointerInside = false;
    markPointerInside = false;
    closePointerInside = false;
}

void BrowserActionContent::mouseDown (const juce::MouseEvent& event)
{
    pressPosition = event.position;
    removePressed = isInsideRemoveButton (event.position);
    markPressed = isInsideMarkButton (event.position);
    closePressed = isInsideCloseButton (event.position);
    removePointerInside = removePressed;
    markPointerInside = markPressed;
    closePointerInside = closePressed;

    if (removePressed && canRemove)
        startTimer (1000);
}

void BrowserActionContent::mouseDrag (const juce::MouseEvent& event)
{
    removePointerInside = isInsideRemoveButton (event.position);
    markPointerInside = isInsideMarkButton (event.position);
    closePointerInside = isInsideCloseButton (event.position);
}

void BrowserActionContent::mouseUp (const juce::MouseEvent& event)
{
    stopTimer();

    const auto insideRemove = isInsideRemoveButton (event.position);
    const auto insideMark = isInsideMarkButton (event.position);
    const auto insideClose = isInsideCloseButton (event.position);
    auto shouldDismiss = false;

    if (removeLongPressReady && insideRemove && removeCallback != nullptr)
    {
        removeCallback();
        shouldDismiss = true;
    }
    else if (markPressed && insideMark && toggleMarkCallback != nullptr)
    {
        toggleMarkCallback();
        shouldDismiss = true;
    }
    else if (closePressed && insideClose)
    {
        shouldDismiss = true;
    }

    if (shouldDismiss && dismissCallback != nullptr)
        dismissCallback();

    resetPressState();
    repaint();
}

void BrowserActionContent::timerCallback()
{
    stopTimer();

    if (! removePressed || ! canRemove)
        return;

    removeLongPressReady = true;
    repaint();
}
bool BrowserActionContent::isInsideRemoveButton (juce::Point<float> position) const noexcept
{
    return removeButtonBounds.contains (position.toInt());
}

bool BrowserActionContent::isInsideMarkButton (juce::Point<float> position) const noexcept
{
    return markButtonBounds.contains (position.toInt());
}

bool BrowserActionContent::isInsideCloseButton (juce::Point<float> position) const noexcept
{
    return closeButtonBounds.contains (position.toInt());
}

void BrowserActionContent::resetPressState()
{
    stopTimer();
    removePressed = false;
    removeLongPressReady = false;
    removePointerInside = false;
    markPressed = false;
    markPointerInside = false;
    closePressed = false;
    closePointerInside = false;
}

NowPlayingContent::NowPlayingContent (SwipeCallback previousTrackActionToOwn,
                                      SwipeCallback nextTrackActionToOwn,
                                      SeekCallback seekBackwardActionToOwn,
                                      SeekCallback seekForwardActionToOwn)
    : previousTrackAction (std::move (previousTrackActionToOwn))
    , nextTrackAction (std::move (nextTrackActionToOwn))
    , seekBackwardAction (std::move (seekBackwardActionToOwn))
    , seekForwardAction (std::move (seekForwardActionToOwn))
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

    const auto seekBackwardBounds = getSeekBackwardBounds();
    const auto seekForwardBounds = getSeekForwardBounds();

    g.setColour (seekBackwardPressed ? popupUiGrey700 : popupUiGrey800);
    g.fillRect (seekBackwardBounds);
    g.setColour (popupUiGrey500);
    g.drawRect (seekBackwardBounds, 1);

    g.setColour (seekForwardPressed ? popupUiGrey700 : popupUiGrey800);
    g.fillRect (seekForwardBounds);
    g.setColour (popupUiGrey500);
    g.drawRect (seekForwardBounds, 1);

    g.setColour (popupUiWhite);
    g.setFont (makePopupUiFont());
    g.drawFittedText ("-15", seekBackwardBounds, juce::Justification::centred, 1, 1.0f);
    g.drawFittedText ("+15", seekForwardBounds, juce::Justification::centred, 1, 1.0f);
    g.drawFittedText (toPopupUiUppercase (title), titleArea, juce::Justification::centred, 1, 1.0f);
    g.drawFittedText (toPopupUiUppercase (subtitle), subtitleArea, juce::Justification::centred, 1, 1.0f);
}

juce::Rectangle<int> NowPlayingContent::getArtworkBounds() const
{
    auto contentArea = getLocalBounds().reduced (4, 4);
    const auto textLineHeight = juce::roundToInt (popupUiFontSize);
    const auto textGap = 6;
    const auto actionAreaHeight = popupUiActionHeight + popupUiListBottomInset + textGap;
    const auto maxArtworkWidth = juce::jmax (1, juce::roundToInt (contentArea.getWidth() * 0.9f));
    const auto maxArtworkHeight = juce::jmax (1, contentArea.getHeight() - (textLineHeight * 2) - textGap - actionAreaHeight);
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

juce::Rectangle<int> NowPlayingContent::getSeekBackwardBounds() const
{
    auto contentArea = getLocalBounds().reduced (4, 4);
    const auto artworkBounds = getArtworkBounds();
    return juce::Rectangle<int> (artworkBounds.getX(),
                                 contentArea.getBottom() - popupUiListBottomInset - popupUiActionHeight,
                                 52,
                                 popupUiActionHeight);
}

juce::Rectangle<int> NowPlayingContent::getSeekForwardBounds() const
{
    auto contentArea = getLocalBounds().reduced (4, 4);
    const auto artworkBounds = getArtworkBounds();
    return juce::Rectangle<int> (artworkBounds.getRight() - 52,
                                 contentArea.getBottom() - popupUiListBottomInset - popupUiActionHeight,
                                 52,
                                 popupUiActionHeight);
}

void NowPlayingContent::mouseMove (const juce::MouseEvent& event)
{
    setMouseCursor (getArtworkBounds().contains (event.getPosition())
                        || getSeekBackwardBounds().contains (event.getPosition())
                        || getSeekForwardBounds().contains (event.getPosition())
                        ? juce::MouseCursor::PointingHandCursor
                        : juce::MouseCursor::NormalCursor);
}

void NowPlayingContent::mouseExit (const juce::MouseEvent&)
{
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void NowPlayingContent::mouseDown (const juce::MouseEvent& event)
{
    seekBackwardPressed = getSeekBackwardBounds().contains (event.getPosition());
    seekForwardPressed = getSeekForwardBounds().contains (event.getPosition());

    if (seekBackwardPressed || seekForwardPressed)
    {
        swipeCandidate = false;
        repaint();
        return;
    }

    swipeCandidate = getArtworkBounds().contains (event.getPosition());
    swipeStartPosition = event.position;
}

void NowPlayingContent::mouseUp (const juce::MouseEvent& event)
{
    const auto triggerSeekBackward = seekBackwardPressed && getSeekBackwardBounds().contains (event.getPosition());
    const auto triggerSeekForward = seekForwardPressed && getSeekForwardBounds().contains (event.getPosition());

    seekBackwardPressed = false;
    seekForwardPressed = false;

    if (triggerSeekBackward)
    {
        repaint();

        if (seekBackwardAction)
            seekBackwardAction();

        return;
    }

    if (triggerSeekForward)
    {
        repaint();

        if (seekForwardAction)
            seekForwardAction();

        return;
    }

    if (! swipeCandidate)
    {
        repaint();
        return;
    }

    swipeCandidate = false;
    repaint();

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
