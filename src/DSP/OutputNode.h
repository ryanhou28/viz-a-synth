#pragma once

#include "../Core/SignalNode.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace vizasynth {

/**
 * OutputNode - Represents the final audio output in a SignalGraph
 *
 * This is a dedicated output node that:
 *   - Is always present in the graph (cannot be deleted)
 *   - Cannot be added from palette (only one exists per graph)
 *   - Has a single input connection (the final audio signal)
 *   - Is visually distinct (cyan/teal color)
 *   - Simply passes through its input to the graph output
 *
 * The OutputNode solves the UX confusion where users didn't understand
 * where audio goes. Previously the output was implicitly tied to whatever
 * node was set as outputNode (typically the filter).
 *
 * Usage:
 *   // OutputNode is automatically created with SignalGraph
 *   graph.connect("filter1", "output");  // Connect filter to output
 *   // Output from graph.process() comes from the output node
 */
class OutputNode : public SignalNode {
public:
    OutputNode() = default;
    ~OutputNode() override = default;

    //=========================================================================
    // Processing (pass-through)
    //=========================================================================

    /**
     * Simply pass through the input signal.
     * The OutputNode doesn't process audio - it just routes it.
     */
    float process(float input) override {
        lastOutput = input;
        return lastOutput;
    }

    void reset() override {
        lastOutput = 0.0f;
    }

    void prepare(double sampleRate, int samplesPerBlock) override {
        currentSampleRate = sampleRate;
        currentBlockSize = samplesPerBlock;
    }

    float getLastOutput() const override { return lastOutput; }
    double getSampleRate() const override { return currentSampleRate; }

    //=========================================================================
    // Identification
    //=========================================================================

    std::string getName() const override { return "Output"; }
    std::string getDescription() const override {
        return "Final audio output - connect your signal chain here";
    }
    std::string getProcessingType() const override { return "Audio Output"; }

    //=========================================================================
    // Connection Capability
    //=========================================================================

    /**
     * Output node accepts exactly one input (the final signal).
     */
    bool canAcceptInput() const override { return true; }

    /**
     * Output node does not produce output to other nodes.
     * It's the terminal node - signal goes to audio output, not other nodes.
     */
    bool canProduceOutput() const override { return false; }

    /**
     * Only one input connection allowed.
     */
    int getMaxInputs() const override { return 1; }

    std::string getInputRestrictionMessage() const override {
        return "Output node accepts one input connection.";
    }

    //=========================================================================
    // Visual Properties
    //=========================================================================

    /**
     * Cyan/teal color to stand out as the output destination.
     */
    juce::Colour getProbeColor() const override {
        return juce::Colour(0xff00BCD4);  // Cyan/teal
    }

    //=========================================================================
    // Special Node Flags
    //=========================================================================

    /**
     * Check if this is the output node type.
     * Used by ChainEditor to prevent deletion and palette addition.
     */
    static bool isOutputNodeType(const SignalNode* node) {
        return dynamic_cast<const OutputNode*>(node) != nullptr;
    }

    /**
     * Returns true - this node cannot be deleted by the user.
     */
    bool isProtected() const { return true; }
};

} // namespace vizasynth
