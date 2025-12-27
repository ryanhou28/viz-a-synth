#include "SignalFlowView.h"

namespace vizasynth {

namespace {
    std::string getEquationForProcessingType(const std::string& processingType, const std::string& name)
    {
        // Map processing types to equations
        if (processingType == "Signal Generator" || name.find("OSC") != std::string::npos) {
            return "x[n] = A·sin(2πf·n/fs + φ)";
        } else if (processingType == "LTI System" || name.find("FILTER") != std::string::npos || name.find("Filter") != std::string::npos) {
            return "H(z) = (b₀ + b₁z⁻¹ + b₂z⁻²) / (1 + a₁z⁻¹ + a₂z⁻²)";
        } else if (processingType == "Time-varying Gain" || name.find("ENV") != std::string::npos) {
            return "y[n] = x[n]·g(t)";
        } else if (processingType == "Output" || name.find("OUT") != std::string::npos || name.find("Mix") != std::string::npos) {
            return "y[n] = x[n]";
        } else {
            // Generic passthrough
            return "y[n] = f(x[n])";
        }
    }
}

SignalFlowView::SignalFlowView(ProbeManager& pm)
    : probeManager(pm)
{
    // Initialize the signal blocks (legacy hardcoded blocks)
    // Note: ENV is not selectable because PostEnvelope probe point is not implemented
    // (envelope just multiplies signal - effectively same as Output)
    blocks = {
        {"OSC", "Signal Generator", "x[n] = A·sin(2πf·n/fs + φ)", ProbePoint::Oscillator, "", getProbeColour(ProbePoint::Oscillator), {}, false, true, false},
        {"FILTER", "LTI System", "H(z) = (b₀ + b₁z⁻¹ + b₂z⁻²) / (1 + a₁z⁻¹ + a₂z⁻²)", ProbePoint::PostFilter, "", getProbeColour(ProbePoint::PostFilter), {}, false, true, false},
        {"ENV", "Time-varying Gain", "y[n] = x[n]·g(t)", ProbePoint::PostEnvelope, "", getProbeColour(ProbePoint::PostEnvelope), {}, false, false, false},  // Not selectable
        {"OUT", "Output", "y[n] = x[n]", ProbePoint::Output, "", getProbeColour(ProbePoint::Output), {}, false, true, false}
    };

    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void SignalFlowView::setProbeRegistry(ProbeRegistry* registry)
{
    probeRegistry = registry;
    if (probeRegistry) {
        updateFromProbeRegistry();
    }
}

void SignalFlowView::updateFromProbeRegistry()
{
    if (!probeRegistry) {
        useDynamicProbes = false;
        return;
    }

    // Get all available probes from the registry
    auto probes = probeRegistry->getAvailableProbes();

    if (probes.empty()) {
        // No probes registered yet, keep using hardcoded blocks
        useDynamicProbes = false;
        return;
    }

    useDynamicProbes = true;

    // Rebuild blocks from registry
    blocks.clear();

    for (const auto& probe : probes) {
        SignalBlock block;
        block.name = probe.displayName;
        block.processingType = probe.processingType;
        block.equation = getEquationForProcessingType(probe.processingType, probe.displayName);
        block.probeId = probe.id;
        block.color = probe.color;
        block.probePoint = ProbePoint::Output;  // Default (not used in dynamic mode)
        block.isSelectable = true;
        block.isHovered = false;
        block.showEquation = false;
        blocks.push_back(block);
    }

    // Add envelope block (always present, non-selectable)
    // Envelope is separate from the signal chain, so it's not registered as a probe
    SignalBlock envBlock;
    envBlock.name = "ENV";
    envBlock.processingType = "Time-varying Gain";
    envBlock.equation = "y[n] = x[n]·g(t)";
    envBlock.probeId = "envelope";
    envBlock.color = juce::Colour(0xffffd93d);  // Yellow
    envBlock.probePoint = ProbePoint::PostEnvelope;
    envBlock.isSelectable = false;
    envBlock.isHovered = false;
    envBlock.showEquation = false;
    blocks.push_back(envBlock);

    resized();
    repaint();
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
    std::string activeProbeId;
    ProbePoint activeProbe = ProbePoint::Output;

    if (useDynamicProbes && probeRegistry) {
        activeProbeId = probeRegistry->getActiveProbe();
    } else {
        activeProbe = probeManager.getActiveProbe();
    }

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
        bool isSelected = useDynamicProbes
            ? (blocks[i].probeId == activeProbeId)
            : (blocks[i].probePoint == activeProbe);
        drawBlock(g, blocks[i], isSelected);
    }

    // Draw title
    g.setColour(config.getTextColour().withAlpha(0.6f));
    g.setFont(11.0f);
    g.drawText("SIGNAL FLOW", getLocalBounds().reduced(8, 4).removeFromTop(14),
               juce::Justification::centredLeft);

    // Draw equation tooltips for blocks with showEquation = true
    for (const auto& block : blocks) {
        if (block.showEquation && !block.equation.empty()) {
            drawEquationTooltip(g, block);
        }
    }
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
    juce::Colour blockColour = block.color.isTransparent() ? getProbeColour(block.probePoint) : block.color;

    if (!block.isSelectable) {
        // Dashed/dim border for non-selectable
        borderColour = juce::Colour(0xff303050);
    } else if (isSelected) {
        borderColour = blockColour;
    } else {
        borderColour = juce::Colour(0xff404060);
    }

    float borderWidth = isSelected ? 2.5f : 1.0f;

    g.setColour(borderColour);
    g.drawRoundedRectangle(bounds, blockCornerRadius, borderWidth);

    // Glow effect if selected (only for selectable blocks)
    if (isSelected && block.isSelectable) {
        auto glowColour = blockColour.withAlpha(0.3f);
        g.setColour(glowColour);
        g.drawRoundedRectangle(bounds.expanded(2.0f), blockCornerRadius + 2.0f, 2.0f);
    }

    // Block name (smaller now to make room for processing type)
    juce::Colour textColour;
    if (!block.isSelectable) {
        textColour = config.getTextColour().withAlpha(0.5f);
    } else if (isSelected) {
        textColour = blockColour;
    } else {
        textColour = config.getTextColour();
    }

    // Block name
    g.setColour(textColour);
    g.setFont(juce::Font(13.0f).boldened());
    g.drawText(block.name, bounds, juce::Justification::centred);

    // Processing type (small text below, only if there's enough height)
    if (bounds.getHeight() > 35.0f) {
        juce::Colour typeColour;
        if (!block.isSelectable) {
            typeColour = config.getTextColour().withAlpha(0.4f);
        } else if (isSelected) {
            typeColour = blockColour.withAlpha(0.6f);
        } else {
            typeColour = config.getTextColour().withAlpha(0.5f);
        }
        g.setColour(typeColour);
        g.setFont(juce::Font(8.0f));
        auto typeArea = bounds.removeFromBottom(12.0f);
        g.drawText(block.processingType, typeArea, juce::Justification::centred);
    }
}

void SignalFlowView::drawEquationTooltip(juce::Graphics& g, const SignalBlock& block)
{
    auto& config = ConfigurationManager::getInstance();

    // Measure equation text to size the tooltip box
    g.setFont(juce::Font(10.0f));
    auto textWidth = g.getCurrentFont().getStringWidth(block.equation);
    auto textHeight = g.getCurrentFont().getHeight();

    // Create tooltip box
    float padding = 6.0f;
    float tooltipWidth = textWidth + padding * 2;
    float tooltipHeight = textHeight + padding * 2;

    // Position tooltip below the specific block, within component bounds
    auto componentBounds = getLocalBounds().toFloat().reduced(4.0f);
    auto tooltipX = block.bounds.getCentreX();
    auto tooltipY = block.bounds.getBottom() + 6.0f;

    juce::Rectangle<float> tooltipBox(
        tooltipX - tooltipWidth * 0.5f,
        tooltipY,
        tooltipWidth,
        tooltipHeight
    );

    // Keep tooltip within horizontal bounds
    if (tooltipBox.getRight() > componentBounds.getRight()) {
        tooltipBox.setX(componentBounds.getRight() - tooltipBox.getWidth());
    }
    if (tooltipBox.getX() < componentBounds.getX()) {
        tooltipBox.setX(componentBounds.getX());
    }

    // If tooltip would go below component, position it above the block instead
    bool showAbove = tooltipBox.getBottom() > componentBounds.getBottom();
    if (showAbove) {
        tooltipY = block.bounds.getY() - tooltipHeight - 6.0f;
        tooltipBox.setY(tooltipY);
    }

    // Draw tooltip background with shadow
    g.setColour(juce::Colours::black.withAlpha(0.15f));
    g.fillRoundedRectangle(tooltipBox.translated(1.0f, 2.0f), 6.0f);

    // Draw tooltip background
    juce::Colour bgColor = config.getBackgroundColour().brighter(0.2f);
    g.setColour(bgColor);
    g.fillRoundedRectangle(tooltipBox, 6.0f);

    // Draw tooltip border (use block color)
    juce::Colour blockColour = block.color.isTransparent() ? getProbeColour(block.probePoint) : block.color;
    g.setColour(blockColour.withAlpha(0.6f));
    g.drawRoundedRectangle(tooltipBox, 6.0f, 1.5f);

    // Draw equation text
    g.setColour(config.getTextColour());
    g.setFont(juce::Font(10.0f));
    g.drawText(block.equation, tooltipBox, juce::Justification::centred);

    // Draw small triangle pointer from tooltip to block
    juce::Path pointer;
    float pointerSize = 5.0f;

    if (showAbove) {
        // Tooltip is above block - pointer points down from bottom of tooltip
        juce::Point<float> tipPoint(tooltipX, tooltipBox.getBottom());
        pointer.startNewSubPath(tipPoint.x, tipPoint.y);
        pointer.lineTo(tipPoint.x - pointerSize, tipPoint.y - pointerSize);
        pointer.lineTo(tipPoint.x + pointerSize, tipPoint.y - pointerSize);
    } else {
        // Tooltip is below block - pointer points up from top of tooltip
        juce::Point<float> tipPoint(tooltipX, tooltipBox.getY());
        pointer.startNewSubPath(tipPoint.x, tipPoint.y);
        pointer.lineTo(tipPoint.x - pointerSize, tipPoint.y + pointerSize);
        pointer.lineTo(tipPoint.x + pointerSize, tipPoint.y + pointerSize);
    }
    pointer.closeSubPath();

    g.setColour(bgColor);
    g.fillPath(pointer);
    g.setColour(blockColour.withAlpha(0.4f));
    g.strokePath(pointer, juce::PathStrokeType(1.0f));
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
        auto& clickedBlock = blocks[clickedIndex];

        // Check if this is a double-click or if shift/alt key is held - show equation
        bool showEquationRequest = event.mods.isShiftDown() || event.mods.isAltDown() ||
                                   event.getNumberOfClicks() >= 2;

        if (showEquationRequest) {
            // Toggle equation display for this block
            clickedBlock.showEquation = !clickedBlock.showEquation;
            repaint();
            return;
        }

        // Only allow selection of selectable blocks
        if (!clickedBlock.isSelectable) {
            return;
        }

        if (useDynamicProbes && probeRegistry) {
            // Use string-based probe ID
            const auto& probeId = clickedBlock.probeId;
            if (probeRegistry->setActiveProbe(probeId)) {
                // Also update legacy probe manager for backwards compatibility
                auto newProbe = clickedBlock.probePoint;
                probeManager.setActiveProbe(newProbe);

                // Notify callback
                if (selectionCallback) {
                    selectionCallback(newProbe);
                }
            }
        } else {
            // Use legacy enum-based probe
            auto newProbe = clickedBlock.probePoint;

            // Update probe manager
            probeManager.setActiveProbe(newProbe);

            // Notify callback
            if (selectionCallback) {
                selectionCallback(newProbe);
            }
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
