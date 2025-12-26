#include "SignalFlowView.h"

namespace vizasynth {

SignalFlowView::SignalFlowView(ProbeManager& pm)
    : probeManager(pm)
{
    // Initialize the signal blocks
    // Note: ENV is not selectable because PostEnvelope probe point is not implemented
    // (envelope just multiplies signal - effectively same as Output)
    blocks = {
        {"OSC", "Signal Generator", ProbePoint::Oscillator, {}, false, true},
        {"FILTER", "LTI System", ProbePoint::PostFilter, {}, false, true},
        {"ENV", "Time-varying Gain", ProbePoint::PostEnvelope, {}, false, false},  // Not selectable
        {"OUT", "Output", ProbePoint::Output, {}, false, true}
    };

    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

SignalFlowView::~SignalFlowView() = default;

void SignalFlowView::setProbeSelectionCallback(ProbeSelectionCallback callback)
{
    selectionCallback = std::move(callback);
}

void SignalFlowView::updateProbeSelection()
{
    repaint();
}

juce::Colour SignalFlowView::getProbeColour(ProbePoint probe)
{
    switch (probe) {
        case ProbePoint::Oscillator:    return juce::Colour(0xff4ecdc4);  // Cyan
        case ProbePoint::PostFilter:    return juce::Colour(0xffff6b6b);  // Coral/Red
        case ProbePoint::PostEnvelope:  return juce::Colour(0xffffd93d);  // Yellow
        case ProbePoint::Output:        return juce::Colour(0xff6bcb77);  // Green
        case ProbePoint::Mix:           return juce::Colour(0xffb088f9);  // Purple
        default:                        return juce::Colour(0xffe0e0e0);  // Light gray
    }
}

void SignalFlowView::paint(juce::Graphics& g)
{
    auto& config = ConfigurationManager::getInstance();

    // Background
    g.setColour(config.getPanelBackgroundColour());
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 10.0f);

    // Get current probe selection
    auto activeProbe = probeManager.getActiveProbe();

    // Draw arrows first (so blocks appear on top)
    g.setColour(juce::Colour(0xff606080));
    for (size_t i = 0; i < blocks.size() - 1; ++i) {
        auto& current = blocks[i];
        auto& next = blocks[i + 1];

        juce::Point<float> from(current.bounds.getRight(), current.bounds.getCentreY());
        juce::Point<float> to(next.bounds.getX(), next.bounds.getCentreY());

        drawArrow(g, from, to);
    }

    // Draw blocks
    for (size_t i = 0; i < blocks.size(); ++i) {
        bool isSelected = (blocks[i].probePoint == activeProbe);
        drawBlock(g, blocks[i], isSelected);
    }

    // Draw title
    g.setColour(config.getTextColour().withAlpha(0.6f));
    g.setFont(11.0f);
    g.drawText("SIGNAL FLOW", getLocalBounds().reduced(8, 4).removeFromTop(14),
               juce::Justification::centredLeft);
}

void SignalFlowView::drawBlock(juce::Graphics& g, const SignalBlock& block, bool isSelected)
{
    auto& config = ConfigurationManager::getInstance();
    auto bounds = block.bounds;

    // Non-selectable blocks are dimmed
    float alpha = block.isSelectable ? 1.0f : 0.5f;

    // Block background
    juce::Colour bgColour = config.getBackgroundColour().brighter(0.1f);
    if (block.isHovered && block.isSelectable) {
        bgColour = bgColour.brighter(0.15f);
    }

    g.setColour(bgColour.withAlpha(alpha));
    g.fillRoundedRectangle(bounds, blockCornerRadius);

    // Border - colored if selected, dim otherwise
    juce::Colour borderColour;
    if (!block.isSelectable) {
        // Dashed/dim border for non-selectable
        borderColour = juce::Colour(0xff303050);
    } else if (isSelected) {
        borderColour = getProbeColour(block.probePoint);
    } else {
        borderColour = juce::Colour(0xff404060);
    }

    float borderWidth = isSelected ? 2.5f : 1.0f;

    g.setColour(borderColour);
    g.drawRoundedRectangle(bounds, blockCornerRadius, borderWidth);

    // Glow effect if selected (only for selectable blocks)
    if (isSelected && block.isSelectable) {
        auto glowColour = getProbeColour(block.probePoint).withAlpha(0.3f);
        g.setColour(glowColour);
        g.drawRoundedRectangle(bounds.expanded(2.0f), blockCornerRadius + 2.0f, 2.0f);
    }

    // Block name
    juce::Colour textColour;
    if (!block.isSelectable) {
        textColour = config.getTextColour().withAlpha(0.5f);
    } else if (isSelected) {
        textColour = getProbeColour(block.probePoint);
    } else {
        textColour = config.getTextColour();
    }
    g.setColour(textColour);
    g.setFont(juce::Font(13.0f).boldened());
    g.drawText(block.name, bounds, juce::Justification::centred);

    // Processing type (small text below, only if there's enough height)
    if (bounds.getHeight() > 35.0f) {
        g.setColour(config.getTextColour().withAlpha(block.isSelectable ? 0.5f : 0.3f));
        g.setFont(8.0f);
        auto typeArea = bounds.removeFromBottom(12.0f);
        g.drawText(block.processingType, typeArea, juce::Justification::centred);
    }
}

void SignalFlowView::drawArrow(juce::Graphics& g, juce::Point<float> from, juce::Point<float> to)
{
    // Shorten the line a bit for visual clarity
    from.x += arrowGap * 0.3f;
    to.x -= arrowGap * 0.3f;

    // Draw line
    g.drawLine(from.x, from.y, to.x - arrowHeadSize, to.y, 1.5f);

    // Draw arrowhead
    juce::Path arrow;
    arrow.startNewSubPath(to.x, to.y);
    arrow.lineTo(to.x - arrowHeadSize, to.y - arrowHeadSize * 0.5f);
    arrow.lineTo(to.x - arrowHeadSize, to.y + arrowHeadSize * 0.5f);
    arrow.closeSubPath();

    g.fillPath(arrow);
}

void SignalFlowView::resized()
{
    auto bounds = getLocalBounds().toFloat();

    // Reserve space for title
    bounds.removeFromTop(16.0f);
    bounds = bounds.reduced(blockPadding, 4.0f);

    // Calculate block dimensions
    int numBlocks = static_cast<int>(blocks.size());
    float totalArrowSpace = arrowGap * (numBlocks - 1);
    float availableWidth = bounds.getWidth() - totalArrowSpace;
    float blockWidth = availableWidth / numBlocks;
    float blockHeight = bounds.getHeight();

    // Position each block
    float x = bounds.getX();
    for (auto& block : blocks) {
        block.bounds = juce::Rectangle<float>(x, bounds.getY(), blockWidth, blockHeight);
        x += blockWidth + arrowGap;
    }
}

void SignalFlowView::mouseDown(const juce::MouseEvent& event)
{
    int clickedIndex = findBlockAtPoint(event.position);
    if (clickedIndex >= 0 && clickedIndex < static_cast<int>(blocks.size())) {
        // Only allow selection of selectable blocks
        if (!blocks[clickedIndex].isSelectable) {
            return;
        }

        auto newProbe = blocks[clickedIndex].probePoint;

        // Update probe manager
        probeManager.setActiveProbe(newProbe);

        // Notify callback
        if (selectionCallback) {
            selectionCallback(newProbe);
        }

        repaint();
    }
}

void SignalFlowView::mouseMove(const juce::MouseEvent& event)
{
    int newHoveredIndex = findBlockAtPoint(event.position);

    // Don't show hover on non-selectable blocks
    if (newHoveredIndex >= 0 && newHoveredIndex < static_cast<int>(blocks.size())) {
        if (!blocks[newHoveredIndex].isSelectable) {
            newHoveredIndex = -1;
        }
    }

    if (newHoveredIndex != hoveredBlockIndex) {
        // Clear old hover
        if (hoveredBlockIndex >= 0 && hoveredBlockIndex < static_cast<int>(blocks.size())) {
            blocks[hoveredBlockIndex].isHovered = false;
        }

        // Set new hover
        hoveredBlockIndex = newHoveredIndex;
        if (hoveredBlockIndex >= 0 && hoveredBlockIndex < static_cast<int>(blocks.size())) {
            blocks[hoveredBlockIndex].isHovered = true;
        }

        repaint();
    }
}

void SignalFlowView::mouseExit(const juce::MouseEvent&)
{
    if (hoveredBlockIndex >= 0 && hoveredBlockIndex < static_cast<int>(blocks.size())) {
        blocks[hoveredBlockIndex].isHovered = false;
        hoveredBlockIndex = -1;
        repaint();
    }
}

int SignalFlowView::findBlockAtPoint(juce::Point<float> point) const
{
    for (size_t i = 0; i < blocks.size(); ++i) {
        if (blocks[i].bounds.contains(point)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

} // namespace vizasynth
