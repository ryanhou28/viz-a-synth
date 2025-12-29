#include "ChainEditor.h"
#include "../Core/Configuration.h"
#include <algorithm>
#include <limits>

namespace vizasynth {

//=============================================================================
// ChainEditor Implementation
//=============================================================================

ChainEditor::ChainEditor()
{
    palette = std::make_unique<ModulePalette>(*this);
    propertiesPanel = std::make_unique<PropertiesPanel>(*this);

    addAndMakeVisible(palette.get());
    addAndMakeVisible(propertiesPanel.get());

    setWantsKeyboardFocus(true);
}

void ChainEditor::setGraph(SignalGraph* graph)
{
    currentGraph = graph;
    rebuildVisualsFromGraph();
    repaint();
}

std::string ChainEditor::addNode(const std::string& moduleType, juce::Point<float> position)
{
    if (!currentGraph || isReadOnly) {
        return "";
    }

    // Create the node via factory
    auto node = SignalNodeFactory::create(moduleType);
    if (!node) {
        return "";
    }

    // Generate unique ID
    std::string nodeId = generateNodeId(moduleType);

    // Determine number of inputs based on type
    int numInputs = 1;
    if (moduleType == "mixer") {
        numInputs = 2;  // Mixers have multiple inputs
    }

    // Add to graph
    currentGraph->addNode(std::move(node), nodeId, numInputs);

    // Create visual - get display name and color from node
    NodeVisual visual;
    visual.id = nodeId;
    visual.type = moduleType;

    // Get display name from node, or generate friendly name from ID
    auto* createdNode = currentGraph->getNode(nodeId);
    if (createdNode) {
        // Use node's name (e.g., "PolyBLEP Oscillator") combined with the ID for uniqueness
        std::string baseName = createdNode->getName();
        // Create a short display name like "Osc 1", "Filter 1", etc.
        if (moduleType == "oscillator") {
            visual.displayName = "Osc " + std::to_string(nextNodeIndex - 1);
        } else if (moduleType == "filter") {
            visual.displayName = "Filter " + std::to_string(nextNodeIndex - 1);
        } else if (moduleType == "mixer") {
            visual.displayName = "Mix " + std::to_string(nextNodeIndex - 1);
        } else {
            visual.displayName = baseName;
        }
        visual.color = createdNode->getProbeColor();
    } else {
        visual.displayName = nodeId;
        visual.color = juce::Colours::lightblue;
    }

    visual.position = position;
    visual.bounds = juce::Rectangle<float>(position.x, position.y, nodeWidth, nodeHeight);
    visual.numInputs = numInputs;

    nodeVisuals.push_back(visual);

    // Register probe for this node
    if (probeRegistry && currentGraph) {
        auto* graphNode = currentGraph->getNode(nodeId);
        if (graphNode) {
            auto* probeBuffer = graphNode->getProbeBuffer();
            if (probeBuffer) {
                std::string probeId = nodeId + ".output";
                std::string displayName = nodeId;
                std::string processingType = getProcessingType(moduleType);
                juce::Colour color = visual.color;
                int orderIndex = static_cast<int>(nodeVisuals.size()) * 10;

                probeRegistry->registerProbe(probeId, displayName, processingType,
                                            probeBuffer, color, orderIndex);
            }
        }
    }

    notifyGraphModified();
    repaint();

    return nodeId;
}

bool ChainEditor::removeNode(const std::string& nodeId)
{
    if (!currentGraph || isReadOnly) {
        return false;
    }

    // Unregister probe BEFORE removing from graph
    if (probeRegistry) {
        std::string probeId = nodeId + ".output";
        probeRegistry->unregisterProbe(probeId);
    }

    // Remove from graph
    auto removed = currentGraph->removeNode(nodeId);
    if (!removed) {
        return false;
    }

    // Remove visual
    nodeVisuals.erase(
        std::remove_if(nodeVisuals.begin(), nodeVisuals.end(),
                       [&nodeId](const NodeVisual& v) { return v.id == nodeId; }),
        nodeVisuals.end()
    );

    // Remove associated connections
    connectionVisuals.erase(
        std::remove_if(connectionVisuals.begin(), connectionVisuals.end(),
                       [&nodeId](const ConnectionVisual& c) {
                           return c.sourceNodeId == nodeId || c.destNodeId == nodeId;
                       }),
        connectionVisuals.end()
    );

    // Clear selection if this was selected
    if (selectedNodeId == nodeId) {
        clearSelection();
    }

    notifyGraphModified();
    repaint();

    return true;
}

bool ChainEditor::connectNodes(const std::string& sourceId, const std::string& destId)
{
    if (!currentGraph || isReadOnly) {
        return false;
    }

    // Connect in graph
    if (!currentGraph->connect(sourceId, destId)) {
        return false;
    }

    // Create connection visual
    ConnectionVisual connVisual;
    connVisual.sourceNodeId = sourceId;
    connVisual.destNodeId = destId;
    connVisual.destInputIndex = 0;
    connVisual.color = juce::Colours::white.withAlpha(0.8f);

    connectionVisuals.push_back(connVisual);

    notifyGraphModified();
    repaint();

    return true;
}

bool ChainEditor::disconnectNodes(const std::string& sourceId, const std::string& destId)
{
    if (!currentGraph || isReadOnly) {
        return false;
    }

    // Disconnect in graph
    if (!currentGraph->disconnect(sourceId, destId)) {
        return false;
    }

    // Remove connection visual
    connectionVisuals.erase(
        std::remove_if(connectionVisuals.begin(), connectionVisuals.end(),
                       [&](const ConnectionVisual& c) {
                           return c.sourceNodeId == sourceId && c.destNodeId == destId;
                       }),
        connectionVisuals.end()
    );

    notifyGraphModified();
    repaint();

    return true;
}

void ChainEditor::selectNode(const std::string& nodeId)
{
    selectedNodeId = nodeId;

    // Update visual state
    for (auto& visual : nodeVisuals) {
        visual.isSelected = (visual.id == nodeId);
    }

    // Update properties panel
    if (propertiesPanel) {
        propertiesPanel->setSelectedNode(nodeId);
    }

    repaint();
}

void ChainEditor::clearSelection()
{
    selectedNodeId.clear();

    for (auto& visual : nodeVisuals) {
        visual.isSelected = false;
    }

    if (propertiesPanel) {
        propertiesPanel->clearSelection();
    }

    repaint();
}

void ChainEditor::setShowPalette(bool show)
{
    showPalette = show;
    palette->setVisible(show);
    resized();
}

void ChainEditor::setShowProperties(bool show)
{
    showProperties = show;
    propertiesPanel->setVisible(show);
    resized();
}

//=============================================================================
// Component Overrides
//=============================================================================

void ChainEditor::paint(juce::Graphics& g)
{
    auto& config = vizasynth::ConfigurationManager::getInstance();

    // Background
    g.fillAll(config.getBackgroundColour().darker(0.2f));

    // Draw close button at top-left
    g.setColour(isCloseButtonHovered ? juce::Colours::red.brighter(0.3f) : juce::Colours::darkgrey);
    g.fillRoundedRectangle(closeButtonBounds.toFloat(), 4.0f);
    g.setColour(config.getTextColour());
    g.setFont(16.0f);
    g.drawText("X", closeButtonBounds, juce::Justification::centred);

    // Canvas background
    g.setColour(config.getBackgroundColour());
    g.fillRect(canvasArea);

    // Draw grid (optional)
    g.setColour(config.getBackgroundColour().brighter(0.1f));
    const int gridSz = getGridSize();
    for (int x = canvasArea.getX(); x < canvasArea.getRight(); x += gridSz) {
        g.drawVerticalLine(x, static_cast<float>(canvasArea.getY()), static_cast<float>(canvasArea.getBottom()));
    }
    for (int y = canvasArea.getY(); y < canvasArea.getBottom(); y += gridSz) {
        g.drawHorizontalLine(y, static_cast<float>(canvasArea.getX()), static_cast<float>(canvasArea.getRight()));
    }

    // Draw validation warning if graph is invalid
    if (currentGraph && !currentGraph->validate()) {
        auto errorMsg = currentGraph->getValidationError();
        if (!errorMsg.empty()) {
            auto warningArea = juce::Rectangle<int>(
                canvasArea.getX() + 20,
                canvasArea.getY() + 20,
                canvasArea.getWidth() - 40,
                60
            );

            // Warning background using theme colors
            auto warningBg = juce::Colour(0xffff9800);  // Orange
            g.setColour(warningBg.withAlpha(0.9f));
            g.fillRoundedRectangle(warningArea.toFloat(), 8.0f);

            // Warning border
            g.setColour(warningBg.darker(0.3f));
            g.drawRoundedRectangle(warningArea.toFloat(), 8.0f, 2.0f);

            // Warning icon and text
            g.setColour(juce::Colours::black);
            g.setFont(14.0f);
            g.drawText("⚠ WARNING", warningArea.removeFromTop(20).reduced(10, 0),
                      juce::Justification::centredLeft);

            g.setFont(12.0f);
            g.drawText(errorMsg, warningArea.reduced(10, 0),
                      juce::Justification::centredLeft, true);
        }
    }

    // Draw connections first (behind nodes)
    for (const auto& conn : connectionVisuals) {
        drawConnection(g, conn);
    }

    // Draw connection being dragged
    if (dragState.type == DragState::Type::Connection) {
        auto sourceNode = std::find_if(nodeVisuals.begin(), nodeVisuals.end(),
                                        [this](const NodeVisual& n) {
                                            return n.id == dragState.nodeId;
                                        });
        if (sourceNode != nodeVisuals.end()) {
            auto offset = canvasArea.getTopLeft().toFloat();
            // Get port in canvas coordinates, then offset to screen coordinates
            juce::Point<float> start = getNodeOutputPort(*sourceNode).translated(offset.x, offset.y);
            // dragState.currentPos is in canvas coordinates, so offset to screen
            juce::Point<float> end = dragState.currentPos.translated(offset.x, offset.y);
            drawConnectionCurve(g, start, end,
                                juce::Colours::yellow.withAlpha(0.5f), true);
        }
    }

    // Draw nodes
    for (const auto& node : nodeVisuals) {
        drawNode(g, node);
    }

    // Draw read-only overlay if applicable
    if (isReadOnly) {
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.fillRect(canvasArea);
        g.setColour(juce::Colours::white);
        g.setFont(20.0f);
        g.drawText("Read Only", canvasArea, juce::Justification::centred);
    }
}

void ChainEditor::visibilityChanged()
{
    // Rebuild visuals when component becomes visible
    // This ensures proper layout after canvasArea is sized
    if (isVisible() && currentGraph) {
        rebuildVisualsFromGraph();
    }
}

void ChainEditor::resized()
{
    auto bounds = getLocalBounds();

    // Close button at top-left (30x30 with 10px padding)
    closeButtonBounds = bounds.removeFromTop(40).removeFromLeft(40).reduced(5);

    // Layout: [Palette] | [Canvas] | [Properties]
    int paletteW = showPalette ? getPaletteWidth() : 0;
    int propsW = showProperties ? getPropertiesWidth() : 0;

    paletteArea = bounds.removeFromLeft(paletteW);
    propertiesArea = bounds.removeFromRight(propsW);
    canvasArea = bounds;

    if (palette) {
        palette->setBounds(paletteArea);
    }
    if (propertiesPanel) {
        propertiesPanel->setBounds(propertiesArea);
    }
}

void ChainEditor::mouseDown(const juce::MouseEvent& event)
{
    // Check if clicking close button
    if (closeButtonBounds.contains(event.getPosition().toInt())) {
        if (onClose) {
            onClose();
        }
        return;
    }

    if (isReadOnly) {
        return;
    }

    auto canvasPos = event.position - canvasArea.getTopLeft().toFloat();

    // Check for right-click on connection
    if (event.mods.isPopupMenu()) {
        auto* conn = findConnectionAt(canvasPos);
        if (conn) {
            showConnectionContextMenu(*conn, event.getMouseDownScreenPosition());
            return;
        }
    }

    // Check if clicking on a node
    auto* node = findNodeAt(canvasPos);
    if (node) {
        // Check if clicking on output port (start connection drag)
        auto outputPort = getNodeOutputPort(*node);
        if (canvasPos.getDistanceFrom(outputPort) < portRadius * 2) {
            dragState.type = DragState::Type::Connection;
            dragState.nodeId = node->id;
            dragState.startPos = outputPort;
            dragState.currentPos = canvasPos;
            return;
        }

        // Regular node selection and drag
        selectNode(node->id);
        dragState.type = DragState::Type::Node;
        dragState.nodeId = node->id;
        dragState.startPos = canvasPos - node->position;
        node->isDragging = true;
        return;
    }

    // Clicking on empty canvas
    clearSelection();
    dragState.type = DragState::Type::Canvas;
    dragState.startPos = canvasPos;
}

void ChainEditor::mouseDrag(const juce::MouseEvent& event)
{
    if (isReadOnly) {
        return;
    }

    auto canvasPos = event.position - canvasArea.getTopLeft().toFloat();

    if (dragState.type == DragState::Type::Node) {
        // Move node
        auto node = std::find_if(nodeVisuals.begin(), nodeVisuals.end(),
                                  [this](const NodeVisual& n) {
                                      return n.id == dragState.nodeId;
                                  });
        if (node != nodeVisuals.end()) {
            node->position = canvasPos - dragState.startPos;
            node->bounds = juce::Rectangle<float>(node->position.x, node->position.y,
                                                   nodeWidth, nodeHeight);
            repaint();
        }
    } else if (dragState.type == DragState::Type::Connection) {
        // Update connection preview
        dragState.currentPos = canvasPos;
        repaint();
    }
}

void ChainEditor::mouseUp(const juce::MouseEvent& event)
{
    if (isReadOnly) {
        return;
    }

    auto canvasPos = event.position - canvasArea.getTopLeft().toFloat();

    if (dragState.type == DragState::Type::Connection) {
        // Check if dropped on a node's input port
        auto* targetNode = findNodeAt(canvasPos);
        if (targetNode && targetNode->id != dragState.nodeId) {
            // Connect the nodes
            connectNodes(dragState.nodeId, targetNode->id);
        }
    } else if (dragState.type == DragState::Type::Node) {
        // Stop dragging
        auto node = std::find_if(nodeVisuals.begin(), nodeVisuals.end(),
                                  [this](const NodeVisual& n) {
                                      return n.id == dragState.nodeId;
                                  });
        if (node != nodeVisuals.end()) {
            node->isDragging = false;
        }
    }

    // Reset drag state
    dragState.type = DragState::Type::None;
    dragState.nodeId.clear();
    repaint();
}

void ChainEditor::mouseMove(const juce::MouseEvent& event)
{
    // Update close button hover state
    bool wasHovered = isCloseButtonHovered;
    isCloseButtonHovered = closeButtonBounds.contains(event.getPosition().toInt());

    auto canvasPos = event.position - canvasArea.getTopLeft().toFloat();

    // Update hover state for nodes
    std::string newHoveredId;
    auto* node = findNodeAt(canvasPos);
    if (node) {
        newHoveredId = node->id;
    }

    // Update hover state for connections
    ConnectionVisual* previousHoveredConn = hoveredConnection;
    hoveredConnection = findConnectionAt(canvasPos);

    // Update connection hover states
    for (auto& conn : connectionVisuals) {
        conn.isHovered = (&conn == hoveredConnection);
    }

    bool needsRepaint = false;

    if (newHoveredId != hoveredNodeId) {
        // Clear previous hover
        for (auto& n : nodeVisuals) {
            n.isHovered = (n.id == newHoveredId);
        }
        hoveredNodeId = newHoveredId;
        needsRepaint = true;
    }

    if (previousHoveredConn != hoveredConnection) {
        needsRepaint = true;
    }

    if (wasHovered != isCloseButtonHovered) {
        needsRepaint = true;
    }

    if (needsRepaint) {
        repaint();
    }
}

bool ChainEditor::keyPressed(const juce::KeyPress& key)
{
    if (isReadOnly) {
        return false;
    }

    // Delete key removes selected node
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) {
        if (!selectedNodeId.empty()) {
            removeNode(selectedNodeId);
            return true;
        }
    }

    return false;
}

void ChainEditor::showConnectionContextMenu(const ConnectionVisual& conn, juce::Point<int> position)
{
    juce::PopupMenu menu;
    menu.addItem(1, "Delete Connection");

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(
        juce::Rectangle<int>(position.x, position.y, 1, 1)),
        [this, sourceId = conn.sourceNodeId, destId = conn.destNodeId](int result) {
            if (result == 1) {
                disconnectNodes(sourceId, destId);
            }
        });
}

std::string ChainEditor::getProcessingType(const std::string& moduleType) const
{
    if (moduleType == "oscillator") return "Signal Generator";
    if (moduleType == "filter") return "LTI System";
    if (moduleType == "mixer") return "Signal Combiner";
    return "DSP Module";
}

//=============================================================================
// Helper Methods - Visual Rebuilding
//=============================================================================

void ChainEditor::rebuildVisualsFromGraph()
{
    nodeVisuals.clear();
    connectionVisuals.clear();

    if (!currentGraph) {
        return;
    }

    // Use topological order to ensure nodes are displayed in signal flow order
    // (oscillator before filter, etc.) rather than alphabetical map order
    auto processingOrder = currentGraph->computeProcessingOrder();

    // Use configurable values with fallbacks
    auto& config = ConfigurationManager::getInstance();
    int defaultCanvasWidth = config.getLayoutInt("components.chainEditor.defaultCanvasWidth", 800);
    int canvasWidth = canvasArea.getWidth() > 0 ? canvasArea.getWidth() : defaultCanvasWidth;
    int xPos = 100;
    int yPos = 100;
    const int spacing = getNodeSpacing();

    // Create visuals for nodes in topological order
    for (const auto& id : processingOrder) {
        auto* node = currentGraph->getNode(id);
        if (!node) continue;

        NodeVisual visual;
        visual.id = id;
        visual.displayName = node->getName();
        visual.type = "unknown";
        visual.position = juce::Point<float>(static_cast<float>(xPos), static_cast<float>(yPos));
        visual.bounds = juce::Rectangle<float>(static_cast<float>(xPos), static_cast<float>(yPos), nodeWidth, nodeHeight);
        visual.color = node->getProbeColor();

        nodeVisuals.push_back(visual);

        xPos += spacing;
        if (xPos > canvasWidth - 100) {
            xPos = 100;
            yPos += spacing;
        }
    }

    // Mark the output node
    auto outputId = currentGraph->getOutputNode();
    for (auto& visual : nodeVisuals) {
        visual.isOutputNode = (visual.id == outputId);
    }

    // Create visuals for connections
    auto connections = currentGraph->getConnections();
    for (const auto& conn : connections) {
        ConnectionVisual connVisual;
        connVisual.sourceNodeId = conn.sourceNode;
        connVisual.destNodeId = conn.destNode;
        connVisual.destInputIndex = conn.destInputIndex;
        connVisual.color = juce::Colours::white.withAlpha(0.8f);

        connectionVisuals.push_back(connVisual);
    }
}

ChainEditor::NodeVisual* ChainEditor::findNodeAt(juce::Point<float> position)
{
    for (auto& node : nodeVisuals) {
        if (node.bounds.contains(position)) {
            return &node;
        }
    }
    return nullptr;
}

ChainEditor::ConnectionVisual* ChainEditor::findConnectionAt(juce::Point<float> position)
{
    const float hitThresh = getHitThreshold();  // Pixels - how close must click be to curve

    for (auto& conn : connectionVisuals) {
        // Find source and dest nodes
        auto source = std::find_if(nodeVisuals.begin(), nodeVisuals.end(),
                                     [&conn](const NodeVisual& n) {
                                         return n.id == conn.sourceNodeId;
                                     });
        auto dest = std::find_if(nodeVisuals.begin(), nodeVisuals.end(),
                                   [&conn](const NodeVisual& n) {
                                       return n.id == conn.destNodeId;
                                   });

        if (source == nodeVisuals.end() || dest == nodeVisuals.end()) {
            continue;
        }

        // Get connection endpoints in canvas coordinates
        juce::Point<float> start = getNodeOutputPort(*source);
        juce::Point<float> end = getNodeInputPort(*dest);

        // Build the bezier curve path (same as drawConnectionCurve)
        juce::Path path;
        path.startNewSubPath(start);

        float midX = (start.x + end.x) / 2;
        juce::Point<float> cp1(midX, start.y);
        juce::Point<float> cp2(midX, end.y);

        path.cubicTo(cp1, cp2, end);

        // Check if position is near the curve
        // Sample along the path and find minimum distance
        float minDistance = std::numeric_limits<float>::max();
        const int samples = 20;  // Number of points to test along curve

        for (int i = 0; i <= samples; ++i) {
            float t = i / static_cast<float>(samples);
            auto point = path.getPointAlongPath(path.getLength() * t);
            float distance = position.getDistanceFrom(point);

            if (distance < minDistance) {
                minDistance = distance;
            }
        }

        if (minDistance < hitThresh) {
            return &conn;
        }
    }

    return nullptr;
}

juce::Point<float> ChainEditor::getNodeInputPort(const NodeVisual& node) const
{
    // Return port position in canvas coordinates (not screen coordinates)
    // Left edge, vertically centered
    return juce::Point<float>(node.bounds.getX(), node.bounds.getCentreY());
}

juce::Point<float> ChainEditor::getNodeOutputPort(const NodeVisual& node) const
{
    // Return port position in canvas coordinates (not screen coordinates)
    // Right edge, vertically centered
    return juce::Point<float>(node.bounds.getRight(), node.bounds.getCentreY());
}

void ChainEditor::drawNode(juce::Graphics& g, const NodeVisual& node)
{
    auto& config = vizasynth::ConfigurationManager::getInstance();
    auto offset = canvasArea.getTopLeft().toFloat();
    auto bounds = node.bounds.translated(offset.x, offset.y);

    // Check if probe is visible
    bool probeVisible = true;
    if (currentGraph) {
        probeVisible = currentGraph->isNodeProbeVisible(node.id);
    }

    // Node body - slightly desaturated if probe is hidden
    juce::Colour bodyColor = node.color.darker(node.isSelected ? 0.0f : 0.3f);
    if (!probeVisible) {
        bodyColor = bodyColor.withSaturation(bodyColor.getSaturation() * 0.5f);
    }
    g.setColour(bodyColor);
    g.fillRoundedRectangle(bounds, 8.0f);

    // Border
    if (node.isSelected) {
        g.setColour(config.getAccentColour());
        g.drawRoundedRectangle(bounds.reduced(1), 8.0f, 3.0f);
    } else if (node.isHovered) {
        g.setColour(config.getTextColour());
        g.drawRoundedRectangle(bounds, 8.0f, 2.0f);
    } else {
        g.setColour(config.getTextColour().withAlpha(0.3f));
        g.drawRoundedRectangle(bounds, 8.0f, 1.0f);
    }

    // Node name
    g.setColour(config.getTextColour());
    g.setFont(14.0f);
    g.drawText(node.displayName, bounds, juce::Justification::centred);

    // Draw probe visibility indicator (small eye icon in corner)
    if (!probeVisible) {
        // Draw a crossed-out eye indicator in top-right corner
        auto eyeBounds = juce::Rectangle<float>(bounds.getRight() - 20, bounds.getY() + 2, 16, 12);
        g.setColour(juce::Colours::grey);
        g.setFont(10.0f);
        g.drawText("⊘", eyeBounds, juce::Justification::centred);  // Crossed circle as "hidden" indicator
    }

    // Input port
    auto inputPort = getNodeInputPort(node).translated(offset.x, offset.y);
    float pRadius = getPortRadius();
    g.setColour(getInputPortColor());
    g.fillEllipse(inputPort.x - pRadius, inputPort.y - pRadius,
                  pRadius * 2, pRadius * 2);

    // Output port
    auto outputPort = getNodeOutputPort(node).translated(offset.x, offset.y);
    g.setColour(getOutputPortColor());
    g.fillEllipse(outputPort.x - pRadius, outputPort.y - pRadius,
                  pRadius * 2, pRadius * 2);

    // Draw output node indicator
    if (node.isOutputNode) {
        // Gold border to indicate output node
        g.setColour(juce::Colours::gold);
        g.drawRoundedRectangle(bounds.expanded(3), 10.0f, 3.0f);

        // "OUTPUT" label below the node
        g.setFont(10.0f);
        auto labelBounds = juce::Rectangle<float>(
            bounds.getX(),
            bounds.getBottom() + 2,
            bounds.getWidth(),
            12
        );
        g.drawText("OUTPUT", labelBounds, juce::Justification::centred);
    }
}

void ChainEditor::drawConnection(juce::Graphics& g, const ConnectionVisual& conn)
{
    // Find source and dest nodes
    auto source = std::find_if(nodeVisuals.begin(), nodeVisuals.end(),
                                 [&conn](const NodeVisual& n) {
                                     return n.id == conn.sourceNodeId;
                                 });
    auto dest = std::find_if(nodeVisuals.begin(), nodeVisuals.end(),
                               [&conn](const NodeVisual& n) {
                                   return n.id == conn.destNodeId;
                               });

    if (source == nodeVisuals.end() || dest == nodeVisuals.end()) {
        return;
    }

    auto offset = canvasArea.getTopLeft().toFloat();
    juce::Point<float> start = getNodeOutputPort(*source).translated(offset.x, offset.y);
    juce::Point<float> end = getNodeInputPort(*dest).translated(offset.x, offset.y);

    // Use configurable connection colors
    juce::Colour color = conn.isHovered ? getConnectionHoverColor() : getConnectionColor();

    drawConnectionCurve(g, start, end, color);
}

void ChainEditor::drawConnectionCurve(juce::Graphics& g,
                                       juce::Point<float> start,
                                       juce::Point<float> end,
                                       juce::Colour color,
                                       bool dashed)
{
    g.setColour(color);

    juce::Path path;
    path.startNewSubPath(start);

    // Bezier curve for smooth connections
    float midX = (start.x + end.x) / 2;
    juce::Point<float> cp1(midX, start.y);
    juce::Point<float> cp2(midX, end.y);

    path.cubicTo(cp1, cp2, end);

    if (dashed) {
        // JUCE doesn't support dashed paths directly, so use reduced opacity instead
        g.setOpacity(0.5f);
        g.strokePath(path, juce::PathStrokeType(connectionThickness));
        g.setOpacity(1.0f);
    } else {
        g.strokePath(path, juce::PathStrokeType(connectionThickness));
    }
}

void ChainEditor::notifyGraphModified()
{
    if (onGraphModified) {
        onGraphModified(currentGraph);
    }
}

//=============================================================================
// Configuration Accessors
//=============================================================================

float ChainEditor::getNodeWidth() const
{
    auto& config = ConfigurationManager::getInstance();
    return config.getLayoutFloat("components.chainEditor.nodeWidth", 120.0f);
}

float ChainEditor::getNodeHeight() const
{
    auto& config = ConfigurationManager::getInstance();
    return config.getLayoutFloat("components.chainEditor.nodeHeight", 60.0f);
}

float ChainEditor::getPortRadius() const
{
    auto& config = ConfigurationManager::getInstance();
    return config.getLayoutFloat("components.chainEditor.portRadius", 8.0f);
}

float ChainEditor::getConnectionThickness() const
{
    auto& config = ConfigurationManager::getInstance();
    return config.getLayoutFloat("components.chainEditor.connectionThickness", 3.0f);
}

int ChainEditor::getPaletteWidth() const
{
    auto& config = ConfigurationManager::getInstance();
    return config.getLayoutInt("components.chainEditor.paletteWidth", 150);
}

int ChainEditor::getPropertiesWidth() const
{
    auto& config = ConfigurationManager::getInstance();
    return config.getLayoutInt("components.chainEditor.propertiesWidth", 200);
}

float ChainEditor::getHitThreshold() const
{
    auto& config = ConfigurationManager::getInstance();
    return config.getLayoutFloat("components.chainEditor.hitThreshold", 10.0f);
}

int ChainEditor::getGridSize() const
{
    auto& config = ConfigurationManager::getInstance();
    return config.getLayoutInt("components.chainEditor.gridSize", 20);
}

int ChainEditor::getNodeSpacing() const
{
    auto& config = ConfigurationManager::getInstance();
    return config.getLayoutInt("components.chainEditor.nodeSpacing", 150);
}

juce::Colour ChainEditor::getInputPortColor() const
{
    auto& config = ConfigurationManager::getInstance();
    return config.getThemeColour("chainEditor.colors.inputPort", juce::Colours::green);
}

juce::Colour ChainEditor::getOutputPortColor() const
{
    auto& config = ConfigurationManager::getInstance();
    return config.getThemeColour("chainEditor.colors.outputPort", juce::Colours::red);
}

juce::Colour ChainEditor::getConnectionColor() const
{
    auto& config = ConfigurationManager::getInstance();
    return config.getThemeColour("chainEditor.colors.connection", juce::Colours::white.withAlpha(0.8f));
}

juce::Colour ChainEditor::getConnectionHoverColor() const
{
    auto& config = ConfigurationManager::getInstance();
    return config.getThemeColour("chainEditor.colors.connectionHover", juce::Colours::white);
}

std::string ChainEditor::generateNodeId(const std::string& type)
{
    return type + std::to_string(nextNodeIndex++);
}

//=============================================================================
// ModulePalette Implementation
//=============================================================================

ChainEditor::ModulePalette::ModulePalette(ChainEditor& editor)
    : owner(editor)
{
    refreshItems();
}

void ChainEditor::ModulePalette::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(juce::Colour(0xff252525));

    // Title
    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    g.drawText("Modules", getLocalBounds().removeFromTop(30), juce::Justification::centred);

    // Draw items
    for (const auto& item : items) {
        // Item background
        g.setColour(item.color.darker(0.5f));
        g.fillRoundedRectangle(item.bounds.toFloat(), 4.0f);

        // Border
        g.setColour(juce::Colours::grey);
        g.drawRoundedRectangle(item.bounds.toFloat(), 4.0f, 1.0f);

        // Name
        g.setColour(juce::Colours::white);
        g.setFont(12.0f);
        g.drawText(item.displayName, item.bounds, juce::Justification::centred);
    }
}

void ChainEditor::ModulePalette::resized()
{
    refreshItems();
}

void ChainEditor::ModulePalette::mouseDown(const juce::MouseEvent& event)
{
    // Check which item was clicked
    for (const auto& item : items) {
        if (item.bounds.contains(event.getPosition())) {
            // Store which item is being dragged
            draggedType = item.type;
            return;
        }
    }
    draggedType.clear();
}

void ChainEditor::ModulePalette::mouseDrag(const juce::MouseEvent& event)
{
    if (draggedType.empty()) {
        return;
    }

    // If we've dragged far enough, start creating a node on the canvas
    if (!event.mouseWasDraggedSinceMouseDown() || event.getDistanceFromDragStart() < 10) {
        return;
    }

    // Convert to owner's (ChainEditor) coordinates
    auto canvasPos = owner.getLocalPoint(this, event.getPosition());

    // Check if we're over the canvas area
    if (owner.canvasArea.contains(canvasPos.toInt())) {
        // Convert to canvas coordinates (relative to canvas area)
        auto nodePos = (canvasPos - owner.canvasArea.getTopLeft()).toFloat();

        // Create the node at this position
        owner.addNode(draggedType, nodePos);

        // Clear dragged type so we don't create multiple nodes
        draggedType.clear();
    }
}

void ChainEditor::ModulePalette::refreshItems()
{
    items.clear();

    // Define available modules
    struct ModuleDef {
        std::string type;
        std::string name;
        juce::Colour color;
    };

    std::vector<ModuleDef> moduleDefs = {
        {"oscillator", "Oscillator", juce::Colour(0xffFF9500)},
        {"filter", "Filter", juce::Colour(0xffBB86FC)},
        {"mixer", "Mixer", juce::Colour(0xff4CAF50)}
    };

    int y = 40;
    int itemHeight = 50;
    int margin = 10;
    int width = getWidth() - margin * 2;

    for (const auto& def : moduleDefs) {
        PaletteItem item;
        item.type = def.type;
        item.displayName = def.name;
        item.color = def.color;
        item.bounds = juce::Rectangle<int>(margin, y, width, itemHeight);

        items.push_back(item);

        y += itemHeight + margin;
    }
}

//=============================================================================
// PropertiesPanel Implementation
//=============================================================================

ChainEditor::PropertiesPanel::PropertiesPanel(ChainEditor& editor)
    : owner(editor)
{
    nodeNameLabel = std::make_unique<juce::Label>("nodeNameLabel", "");
    nodeNameLabel->setFont(juce::Font(14.0f, juce::Font::bold));
    nodeNameLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(nodeNameLabel.get());

    nodeTypeLabel = std::make_unique<juce::Label>("nodeTypeLabel", "");
    nodeTypeLabel->setFont(juce::Font(11.0f));
    nodeTypeLabel->setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(nodeTypeLabel.get());
}

void ChainEditor::PropertiesPanel::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(juce::Colour(0xff252525));

    // Title
    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    g.drawText("Properties", getLocalBounds().removeFromTop(30), juce::Justification::centred);

    if (selectedNodeId.empty()) {
        g.setFont(12.0f);
        g.setColour(juce::Colours::grey);
        g.drawText("No node selected", getLocalBounds(), juce::Justification::centred);
    }
}

void ChainEditor::PropertiesPanel::resized()
{
    auto bounds = getLocalBounds().reduced(10);
    bounds.removeFromTop(30);  // Skip title area

    // Node name and type labels
    if (nodeNameLabel && nodeNameLabel->isVisible()) {
        nodeNameLabel->setBounds(bounds.removeFromTop(20));
    }
    if (nodeTypeLabel && nodeTypeLabel->isVisible()) {
        nodeTypeLabel->setBounds(bounds.removeFromTop(16));
        bounds.removeFromTop(8);  // Spacing
    }

    // Layout parameter controls with their labels
    const int controlHeight = 28;
    const int labelHeight = 16;
    const int spacing = 6;

    for (int i = 0; i < parameterControls.size(); ++i) {
        if (i < parameterLabels.size() && parameterLabels[i] != nullptr) {
            parameterLabels[i]->setBounds(bounds.removeFromTop(labelHeight));
        }
        parameterControls[i]->setBounds(bounds.removeFromTop(controlHeight));
        bounds.removeFromTop(spacing);
    }
}

void ChainEditor::PropertiesPanel::setSelectedNode(const std::string& nodeId)
{
    selectedNodeId = nodeId;
    rebuildControls();
    repaint();
}

void ChainEditor::PropertiesPanel::clearSelection()
{
    selectedNodeId.clear();
    parameterControls.clear();
    parameterLabels.clear();

    // Reset pointers
    waveformCombo = nullptr;
    detuneSlider = nullptr;
    octaveSlider = nullptr;
    bandLimitedToggle = nullptr;
    filterTypeCombo = nullptr;
    cutoffSlider = nullptr;
    resonanceSlider = nullptr;
    probeVisibleToggle = nullptr;

    if (nodeNameLabel) nodeNameLabel->setVisible(false);
    if (nodeTypeLabel) nodeTypeLabel->setVisible(false);

    repaint();
}

void ChainEditor::PropertiesPanel::rebuildControls()
{
    // Clear existing controls
    parameterControls.clear();
    parameterLabels.clear();

    // Reset pointers
    waveformCombo = nullptr;
    detuneSlider = nullptr;
    octaveSlider = nullptr;
    bandLimitedToggle = nullptr;
    filterTypeCombo = nullptr;
    cutoffSlider = nullptr;
    resonanceSlider = nullptr;
    probeVisibleToggle = nullptr;

    if (selectedNodeId.empty() || !owner.currentGraph) {
        if (nodeNameLabel) nodeNameLabel->setVisible(false);
        if (nodeTypeLabel) nodeTypeLabel->setVisible(false);
        return;
    }

    auto* node = owner.currentGraph->getNode(selectedNodeId);
    if (!node) {
        if (nodeNameLabel) nodeNameLabel->setVisible(false);
        if (nodeTypeLabel) nodeTypeLabel->setVisible(false);
        return;
    }

    // Update node info labels
    nodeNameLabel->setText(node->getName(), juce::dontSendNotification);
    nodeNameLabel->setVisible(true);

    nodeTypeLabel->setText("ID: " + selectedNodeId, juce::dontSendNotification);
    nodeTypeLabel->setVisible(true);

    // Check node type and create appropriate controls
    if (auto* osc = dynamic_cast<OscillatorSource*>(node)) {
        createOscillatorControls(osc);
    } else if (auto* filter = dynamic_cast<FilterNode*>(node)) {
        createFilterControls(filter);
    } else if (node->getName() == "Mixer") {
        createMixerControls(node);
    }

    // Always add probe visibility control
    createProbeVisibilityControl();

    resized();
}

void ChainEditor::PropertiesPanel::createOscillatorControls(OscillatorSource* osc)
{
    // Waveform selector
    auto* waveformLabel = new juce::Label("waveformLabel", "Waveform");
    waveformLabel->setFont(juce::Font(11.0f));
    waveformLabel->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    parameterLabels.add(waveformLabel);
    addAndMakeVisible(waveformLabel);

    auto* waveform = new juce::ComboBox("waveformCombo");
    waveform->addItem("Sine", 1);
    waveform->addItem("Saw", 2);
    waveform->addItem("Square", 3);
    waveform->addItem("Triangle", 4);
    waveform->setSelectedId(static_cast<int>(osc->getWaveform()) + 1, juce::dontSendNotification);
    waveform->addListener(this);
    waveformCombo = waveform;
    parameterControls.add(waveform);
    addAndMakeVisible(waveform);

    // Detune slider
    auto* detuneLabel = new juce::Label("detuneLabel", "Detune (cents)");
    detuneLabel->setFont(juce::Font(11.0f));
    detuneLabel->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    parameterLabels.add(detuneLabel);
    addAndMakeVisible(detuneLabel);

    auto* detune = new juce::Slider("detuneSlider");
    detune->setSliderStyle(juce::Slider::LinearHorizontal);
    detune->setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    detune->setRange(-100.0, 100.0, 1.0);
    detune->setValue(osc->getDetuneCents(), juce::dontSendNotification);
    detune->addListener(this);
    detuneSlider = detune;
    parameterControls.add(detune);
    addAndMakeVisible(detune);

    // Octave slider
    auto* octaveLabel = new juce::Label("octaveLabel", "Octave");
    octaveLabel->setFont(juce::Font(11.0f));
    octaveLabel->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    parameterLabels.add(octaveLabel);
    addAndMakeVisible(octaveLabel);

    auto* octave = new juce::Slider("octaveSlider");
    octave->setSliderStyle(juce::Slider::LinearHorizontal);
    octave->setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    octave->setRange(-2.0, 2.0, 1.0);
    octave->setValue(osc->getOctaveOffset(), juce::dontSendNotification);
    octave->addListener(this);
    octaveSlider = octave;
    parameterControls.add(octave);
    addAndMakeVisible(octave);

    // Band-limited toggle
    auto* bandLimitedLabel = new juce::Label("bandLimitedLabel", "Band-Limited");
    bandLimitedLabel->setFont(juce::Font(11.0f));
    bandLimitedLabel->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    parameterLabels.add(bandLimitedLabel);
    addAndMakeVisible(bandLimitedLabel);

    auto* bandLimited = new juce::ToggleButton("Enable");
    bandLimited->setToggleState(osc->isBandLimited(), juce::dontSendNotification);
    bandLimited->addListener(this);
    bandLimitedToggle = bandLimited;
    parameterControls.add(bandLimited);
    addAndMakeVisible(bandLimited);
}

void ChainEditor::PropertiesPanel::createFilterControls(FilterNode* filter)
{
    // Filter type selector
    auto* typeLabel = new juce::Label("typeLabel", "Filter Type");
    typeLabel->setFont(juce::Font(11.0f));
    typeLabel->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    parameterLabels.add(typeLabel);
    addAndMakeVisible(typeLabel);

    auto* filterType = new juce::ComboBox("filterTypeCombo");
    filterType->addItem("Lowpass", 1);
    filterType->addItem("Highpass", 2);
    filterType->addItem("Bandpass", 3);
    filterType->addItem("Notch", 4);
    filterType->setSelectedId(static_cast<int>(filter->getType()) + 1, juce::dontSendNotification);
    filterType->addListener(this);
    filterTypeCombo = filterType;
    parameterControls.add(filterType);
    addAndMakeVisible(filterType);

    // Cutoff slider
    auto* cutoffLabel = new juce::Label("cutoffLabel", "Cutoff (Hz)");
    cutoffLabel->setFont(juce::Font(11.0f));
    cutoffLabel->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    parameterLabels.add(cutoffLabel);
    addAndMakeVisible(cutoffLabel);

    auto* cutoff = new juce::Slider("cutoffSlider");
    cutoff->setSliderStyle(juce::Slider::LinearHorizontal);
    cutoff->setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    cutoff->setRange(20.0, 20000.0, 1.0);
    cutoff->setSkewFactorFromMidPoint(1000.0);  // Log-like response
    cutoff->setValue(filter->getCutoff(), juce::dontSendNotification);
    cutoff->addListener(this);
    cutoffSlider = cutoff;
    parameterControls.add(cutoff);
    addAndMakeVisible(cutoff);

    // Resonance slider
    auto* resonanceLabel = new juce::Label("resonanceLabel", "Resonance (Q)");
    resonanceLabel->setFont(juce::Font(11.0f));
    resonanceLabel->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    parameterLabels.add(resonanceLabel);
    addAndMakeVisible(resonanceLabel);

    auto* resonance = new juce::Slider("resonanceSlider");
    resonance->setSliderStyle(juce::Slider::LinearHorizontal);
    resonance->setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    resonance->setRange(0.5, 20.0, 0.1);
    resonance->setValue(filter->getResonance(), juce::dontSendNotification);
    resonance->addListener(this);
    resonanceSlider = resonance;
    parameterControls.add(resonance);
    addAndMakeVisible(resonance);
}

void ChainEditor::PropertiesPanel::createMixerControls(SignalNode* mixer)
{
    // Mixer has limited controls - just show info
    auto* infoLabel = new juce::Label("infoLabel", mixer->getDescription());
    infoLabel->setFont(juce::Font(11.0f));
    infoLabel->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    parameterLabels.add(infoLabel);
    addAndMakeVisible(infoLabel);

    // Placeholder for mixer controls
    auto* placeholder = new juce::Label("placeholder", "");
    parameterControls.add(placeholder);
    addAndMakeVisible(placeholder);
}

void ChainEditor::PropertiesPanel::createProbeVisibilityControl()
{
    // Separator
    auto* probeLabel = new juce::Label("probeLabel", "Probe Visibility");
    probeLabel->setFont(juce::Font(11.0f));
    probeLabel->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    parameterLabels.add(probeLabel);
    addAndMakeVisible(probeLabel);

    auto* probeVisible = new juce::ToggleButton("Show in Probe Dropdown");
    bool isVisible = owner.currentGraph->isNodeProbeVisible(selectedNodeId);
    probeVisible->setToggleState(isVisible, juce::dontSendNotification);
    probeVisible->addListener(this);
    probeVisibleToggle = probeVisible;
    parameterControls.add(probeVisible);
    addAndMakeVisible(probeVisible);
}

void ChainEditor::PropertiesPanel::sliderValueChanged(juce::Slider* slider)
{
    if (selectedNodeId.empty() || !owner.currentGraph) return;

    auto* node = owner.currentGraph->getNode(selectedNodeId);
    if (!node) return;

    if (slider == detuneSlider) {
        if (auto* osc = dynamic_cast<OscillatorSource*>(node)) {
            osc->setDetuneCents(static_cast<float>(slider->getValue()));
            owner.notifyGraphModified();
        }
    } else if (slider == octaveSlider) {
        if (auto* osc = dynamic_cast<OscillatorSource*>(node)) {
            osc->setOctaveOffset(static_cast<int>(slider->getValue()));
            owner.notifyGraphModified();
        }
    } else if (slider == cutoffSlider) {
        if (auto* filter = dynamic_cast<FilterNode*>(node)) {
            filter->setCutoff(static_cast<float>(slider->getValue()));
            owner.notifyGraphModified();
        }
    } else if (slider == resonanceSlider) {
        if (auto* filter = dynamic_cast<FilterNode*>(node)) {
            filter->setResonance(static_cast<float>(slider->getValue()));
            owner.notifyGraphModified();
        }
    }
}

void ChainEditor::PropertiesPanel::comboBoxChanged(juce::ComboBox* comboBox)
{
    if (selectedNodeId.empty() || !owner.currentGraph) return;

    auto* node = owner.currentGraph->getNode(selectedNodeId);
    if (!node) return;

    if (comboBox == waveformCombo) {
        if (auto* osc = dynamic_cast<OscillatorSource*>(node)) {
            int selection = comboBox->getSelectedId() - 1;  // Convert to 0-based
            osc->setWaveform(static_cast<OscillatorSource::Waveform>(selection));
            owner.notifyGraphModified();
        }
    } else if (comboBox == filterTypeCombo) {
        if (auto* filter = dynamic_cast<FilterNode*>(node)) {
            int selection = comboBox->getSelectedId() - 1;  // Convert to 0-based
            filter->setType(static_cast<FilterNode::Type>(selection));
            owner.notifyGraphModified();
        }
    }
}

void ChainEditor::PropertiesPanel::buttonClicked(juce::Button* button)
{
    if (selectedNodeId.empty() || !owner.currentGraph) return;

    if (button == bandLimitedToggle) {
        auto* node = owner.currentGraph->getNode(selectedNodeId);
        if (auto* osc = dynamic_cast<OscillatorSource*>(node)) {
            osc->setBandLimited(button->getToggleState());
            owner.notifyGraphModified();
        }
    } else if (button == probeVisibleToggle) {
        owner.currentGraph->setNodeProbeVisible(selectedNodeId, button->getToggleState());
        owner.notifyGraphModified();
        owner.repaint();  // Update node visual indicator
    }
}

} // namespace vizasynth
