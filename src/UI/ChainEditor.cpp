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

    // Create visual
    NodeVisual visual;
    visual.id = nodeId;
    visual.type = moduleType;
    visual.displayName = nodeId;  // TODO: Get from node getName()
    visual.position = position;
    visual.bounds = juce::Rectangle<float>(position.x, position.y, nodeWidth, nodeHeight);
    visual.color = juce::Colours::lightblue;  // TODO: Get from node
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
    const int gridSize = 20;
    for (int x = canvasArea.getX(); x < canvasArea.getRight(); x += gridSize) {
        g.drawVerticalLine(x, canvasArea.getY(), canvasArea.getBottom());
    }
    for (int y = canvasArea.getY(); y < canvasArea.getBottom(); y += gridSize) {
        g.drawHorizontalLine(y, canvasArea.getX(), canvasArea.getRight());
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

void ChainEditor::resized()
{
    auto bounds = getLocalBounds();

    // Close button at top-left (30x30 with 10px padding)
    closeButtonBounds = bounds.removeFromTop(40).removeFromLeft(40).reduced(5);

    // Layout: [Palette] | [Canvas] | [Properties]
    int paletteW = showPalette ? paletteWidth : 0;
    int propsW = showProperties ? propertiesWidth : 0;

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

    // Create visuals for nodes
    int xPos = 100;
    int yPos = 100;
    const int spacing = 150;

    currentGraph->forEachNode([&](const std::string& id, const SignalNode* node) {
        if (!node) return;

        NodeVisual visual;
        visual.id = id;
        visual.displayName = node->getName();
        visual.type = "unknown";  // TODO: Store type in graph
        visual.position = juce::Point<float>(xPos, yPos);
        visual.bounds = juce::Rectangle<float>(xPos, yPos, nodeWidth, nodeHeight);
        visual.color = node->getProbeColor();

        nodeVisuals.push_back(visual);

        xPos += spacing;
        if (xPos > canvasArea.getWidth() - 100) {
            xPos = 100;
            yPos += spacing;
        }
    });

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
    const float hitThreshold = 10.0f;  // Pixels - how close must click be to curve

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

        if (minDistance < hitThreshold) {
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

    // Node body
    g.setColour(node.color.darker(node.isSelected ? 0.0f : 0.3f));
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

    // Input port
    auto inputPort = getNodeInputPort(node).translated(offset.x, offset.y);
    g.setColour(juce::Colours::green);
    g.fillEllipse(inputPort.x - portRadius, inputPort.y - portRadius,
                  portRadius * 2, portRadius * 2);

    // Output port
    auto outputPort = getNodeOutputPort(node).translated(offset.x, offset.y);
    g.setColour(juce::Colours::red);
    g.fillEllipse(outputPort.x - portRadius, outputPort.y - portRadius,
                  portRadius * 2, portRadius * 2);

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

    // Highlight hovered connections
    juce::Colour color = conn.color;
    if (conn.isHovered) {
        color = color.brighter(0.5f);
    }

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
    // Layout parameter controls
    auto bounds = getLocalBounds().reduced(10);
    bounds.removeFromTop(30);  // Skip title area

    for (auto* control : parameterControls) {
        control->setBounds(bounds.removeFromTop(40));
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
    repaint();
}

void ChainEditor::PropertiesPanel::rebuildControls()
{
    parameterControls.clear();

    if (selectedNodeId.empty() || !owner.currentGraph) {
        return;
    }

    // TODO: Query node for parameters and create controls dynamically
    // For now, just show a placeholder label

    auto* label = new juce::Label("nodeIdLabel", "Node: " + selectedNodeId);
    label->setColour(juce::Label::textColourId, juce::Colours::white);
    parameterControls.add(label);
    addAndMakeVisible(label);

    resized();
}

} // namespace vizasynth
