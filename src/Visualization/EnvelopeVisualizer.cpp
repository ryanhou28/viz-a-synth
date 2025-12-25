#include "EnvelopeVisualizer.h"

namespace vizasynth {

//==============================================================================
EnvelopeVisualizer::EnvelopeVisualizer(juce::AudioProcessorValueTreeState& apvtsRef)
    : apvts(apvtsRef)
{
    // Register as listener for ADSR parameter changes
    apvts.addParameterListener("attack", this);
    apvts.addParameterListener("decay", this);
    apvts.addParameterListener("sustain", this);
    apvts.addParameterListener("release", this);

    // Initialize cached values
    lastAttack = getAttack();
    lastDecay = getDecay();
    lastSustain = getSustain();
    lastRelease = getRelease();

    startTimerHz(RefreshRateHz);
}

EnvelopeVisualizer::~EnvelopeVisualizer()
{
    stopTimer();

    // Remove parameter listeners
    apvts.removeParameterListener("attack", this);
    apvts.removeParameterListener("decay", this);
    apvts.removeParameterListener("sustain", this);
    apvts.removeParameterListener("release", this);
}

//==============================================================================
void EnvelopeVisualizer::parameterChanged(const juce::String& /*parameterID*/, float /*newValue*/)
{
    // Immediately repaint when any ADSR parameter changes
    repaint();
}

//==============================================================================
void EnvelopeVisualizer::paint(juce::Graphics& g)
{
    auto& config = ConfigurationManager::getInstance();
    auto bounds = getLocalBounds().toFloat().reduced(2);

    // Background
    g.setColour(config.getThemeColour("colors.envelope.background", juce::Colour(0xff1e1e1e)));
    g.fillRoundedRectangle(bounds, 6.0f);

    // Draw area - minimal padding to maximize graph area
    auto drawBounds = bounds.reduced(8, 8);
    drawBounds.removeFromLeft(25);    // Space for Y-axis labels
    drawBounds.removeFromBottom(18);  // Space for X-axis labels

    drawGrid(g, drawBounds);
    drawEnvelopeCurve(g, drawBounds);
    drawPlayhead(g, drawBounds);
    drawLabels(g, drawBounds);
}

void EnvelopeVisualizer::resized()
{
}

//==============================================================================
void EnvelopeVisualizer::timerCallback()
{
    if (currentState == EnvelopeState::Idle)
        return;

    float deltaTime = 1.0f / static_cast<float>(RefreshRateHz);
    playheadTime += deltaTime;

    // State machine for envelope playback
    switch (currentState)
    {
        case EnvelopeState::Attack:
            if (playheadTime >= getAttack())
            {
                playheadTime = 0.0f;
                currentState = EnvelopeState::Decay;
            }
            break;

        case EnvelopeState::Decay:
            if (playheadTime >= getDecay())
            {
                playheadTime = 0.0f;
                currentState = EnvelopeState::Sustain;
            }
            break;

        case EnvelopeState::Sustain:
            // Stay in sustain until release is triggered
            break;

        case EnvelopeState::Release:
            if (playheadTime >= getRelease())
            {
                currentState = EnvelopeState::Idle;
                playheadTime = 0.0f;
            }
            break;

        default:
            break;
    }

    repaint();
}

//==============================================================================
void EnvelopeVisualizer::triggerEnvelope()
{
    currentState = EnvelopeState::Attack;
    playheadTime = 0.0f;
    releaseStartLevel = 1.0f;  // Will be calculated properly if release from sustain
    repaint();
}

void EnvelopeVisualizer::releaseEnvelope()
{
    if (currentState == EnvelopeState::Idle)
        return;

    // Calculate current level for release start
    switch (currentState)
    {
        case EnvelopeState::Attack:
            releaseStartLevel = (getAttack() > 0) ? (playheadTime / getAttack()) : 1.0f;
            break;
        case EnvelopeState::Decay:
        {
            float decayProgress = (getDecay() > 0) ? (playheadTime / getDecay()) : 1.0f;
            releaseStartLevel = 1.0f - (1.0f - getSustain()) * decayProgress;
            break;
        }
        case EnvelopeState::Sustain:
            releaseStartLevel = getSustain();
            break;
        default:
            releaseStartLevel = getSustain();
            break;
    }

    currentState = EnvelopeState::Release;
    playheadTime = 0.0f;
    repaint();
}

void EnvelopeVisualizer::reset()
{
    currentState = EnvelopeState::Idle;
    playheadTime = 0.0f;
    repaint();
}

//==============================================================================
void EnvelopeVisualizer::drawGrid(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    auto& config = ConfigurationManager::getInstance();
    g.setColour(config.getThemeColour("colors.envelope.grid", juce::Colour(0xff333333)));

    // Horizontal grid lines (amplitude)
    for (int i = 0; i <= 4; ++i)
    {
        float y = bounds.getY() + bounds.getHeight() * (i / 4.0f);
        g.drawHorizontalLine(static_cast<int>(y), bounds.getX(), bounds.getRight());
    }

    // Vertical grid lines at segment boundaries
    float attack = getAttack();
    float decay = getDecay();
    float totalTime = getTotalDisplayTime();

    // Attack end / Decay start
    float x1 = bounds.getX() + timeToX(attack, totalTime, bounds.getWidth());
    g.drawVerticalLine(static_cast<int>(x1), bounds.getY(), bounds.getBottom());

    // Decay end / Sustain start
    float x2 = bounds.getX() + timeToX(attack + decay, totalTime, bounds.getWidth());
    g.drawVerticalLine(static_cast<int>(x2), bounds.getY(), bounds.getBottom());

    // Sustain end / Release start
    float x3 = bounds.getX() + timeToX(attack + decay + SustainDisplayTime, totalTime, bounds.getWidth());
    g.drawVerticalLine(static_cast<int>(x3), bounds.getY(), bounds.getBottom());
}

void EnvelopeVisualizer::drawEnvelopeCurve(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    float attack = getAttack();
    float decay = getDecay();
    float sustain = getSustain();
    float totalTime = getTotalDisplayTime();

    juce::Path envelopePath;

    // Start at bottom-left
    float startX = bounds.getX();
    float startY = bounds.getBottom();
    envelopePath.startNewSubPath(startX, startY);

    // Attack segment (0 to peak)
    float attackEndX = bounds.getX() + timeToX(attack, totalTime, bounds.getWidth());
    float peakY = bounds.getY();

    // Draw Attack segment
    g.setColour(getAttackColour());
    juce::Path attackPath;
    attackPath.startNewSubPath(startX, startY);
    attackPath.lineTo(attackEndX, peakY);
    g.strokePath(attackPath, juce::PathStrokeType(2.5f));
    envelopePath.lineTo(attackEndX, peakY);

    // Decay segment (peak to sustain)
    float decayEndX = bounds.getX() + timeToX(attack + decay, totalTime, bounds.getWidth());
    float sustainY = bounds.getBottom() - (sustain * bounds.getHeight());

    g.setColour(getDecayColour());
    juce::Path decayPath;
    decayPath.startNewSubPath(attackEndX, peakY);
    decayPath.lineTo(decayEndX, sustainY);
    g.strokePath(decayPath, juce::PathStrokeType(2.5f));
    envelopePath.lineTo(decayEndX, sustainY);

    // Sustain segment (horizontal line)
    float sustainEndX = bounds.getX() + timeToX(attack + decay + SustainDisplayTime, totalTime, bounds.getWidth());

    g.setColour(getSustainColour());
    juce::Path sustainPath;
    sustainPath.startNewSubPath(decayEndX, sustainY);
    sustainPath.lineTo(sustainEndX, sustainY);
    g.strokePath(sustainPath, juce::PathStrokeType(2.5f));
    envelopePath.lineTo(sustainEndX, sustainY);

    // Release segment (sustain to 0)
    float releaseEndX = bounds.getRight();

    g.setColour(getReleaseColour());
    juce::Path releasePath;
    releasePath.startNewSubPath(sustainEndX, sustainY);
    releasePath.lineTo(releaseEndX, bounds.getBottom());
    g.strokePath(releasePath, juce::PathStrokeType(2.5f));
    envelopePath.lineTo(releaseEndX, bounds.getBottom());

    // No fill under curve - just the line segments
}

void EnvelopeVisualizer::drawPlayhead(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    if (currentState == EnvelopeState::Idle)
        return;

    float attack = getAttack();
    float decay = getDecay();
    float totalTime = getTotalDisplayTime();

    float playheadX = 0.0f;
    float playheadY = bounds.getBottom();
    juce::Colour playheadColour;

    switch (currentState)
    {
        case EnvelopeState::Attack:
        {
            float progress = (attack > 0) ? (playheadTime / attack) : 1.0f;
            progress = juce::jlimit(0.0f, 1.0f, progress);
            playheadX = bounds.getX() + timeToX(playheadTime, totalTime, bounds.getWidth());
            playheadY = bounds.getBottom() - (progress * bounds.getHeight());
            playheadColour = getAttackColour();
            break;
        }
        case EnvelopeState::Decay:
        {
            float progress = (decay > 0) ? (playheadTime / decay) : 1.0f;
            progress = juce::jlimit(0.0f, 1.0f, progress);
            float level = 1.0f - (1.0f - getSustain()) * progress;
            playheadX = bounds.getX() + timeToX(attack + playheadTime, totalTime, bounds.getWidth());
            playheadY = bounds.getBottom() - (level * bounds.getHeight());
            playheadColour = getDecayColour();
            break;
        }
        case EnvelopeState::Sustain:
        {
            playheadX = bounds.getX() + timeToX(attack + decay + std::fmod(playheadTime, SustainDisplayTime), totalTime, bounds.getWidth());
            playheadY = bounds.getBottom() - (getSustain() * bounds.getHeight());
            playheadColour = getSustainColour();
            break;
        }
        case EnvelopeState::Release:
        {
            float progress = (getRelease() > 0) ? (playheadTime / getRelease()) : 1.0f;
            progress = juce::jlimit(0.0f, 1.0f, progress);
            float level = releaseStartLevel * (1.0f - progress);
            playheadX = bounds.getX() + timeToX(attack + decay + SustainDisplayTime + playheadTime, totalTime, bounds.getWidth());
            playheadY = bounds.getBottom() - (level * bounds.getHeight());
            playheadColour = getReleaseColour();
            break;
        }
        default:
            return;
    }

    // Draw vertical line
    g.setColour(playheadColour.withAlpha(0.5f));
    g.drawVerticalLine(static_cast<int>(playheadX), bounds.getY(), bounds.getBottom());

    // Draw playhead dot
    g.setColour(playheadColour);
    g.fillEllipse(playheadX - 5, playheadY - 5, 10, 10);

    // Draw glow effect
    g.setColour(playheadColour.withAlpha(0.3f));
    g.fillEllipse(playheadX - 8, playheadY - 8, 16, 16);
}

void EnvelopeVisualizer::drawLabels(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    float attack = getAttack();
    float decay = getDecay();
    float totalTime = getTotalDisplayTime();

    g.setFont(10.0f);

    // Calculate segment centers for labels
    float attackCenter = bounds.getX() + timeToX(attack / 2, totalTime, bounds.getWidth());
    float decayCenter = bounds.getX() + timeToX(attack + decay / 2, totalTime, bounds.getWidth());
    float sustainCenter = bounds.getX() + timeToX(attack + decay + SustainDisplayTime / 2, totalTime, bounds.getWidth());
    float releaseCenter = bounds.getX() + timeToX(attack + decay + SustainDisplayTime + getRelease() / 2, totalTime, bounds.getWidth());

    float labelY = bounds.getBottom() + 2;
    float labelWidth = 30;
    float labelHeight = 14;

    // Attack label
    g.setColour(getAttackColour());
    g.drawText("A", attackCenter - labelWidth / 2, labelY, labelWidth, labelHeight, juce::Justification::centred);

    // Decay label
    g.setColour(getDecayColour());
    g.drawText("D", decayCenter - labelWidth / 2, labelY, labelWidth, labelHeight, juce::Justification::centred);

    // Sustain label
    g.setColour(getSustainColour());
    g.drawText("S", sustainCenter - labelWidth / 2, labelY, labelWidth, labelHeight, juce::Justification::centred);

    // Release label
    g.setColour(getReleaseColour());
    g.drawText("R", releaseCenter - labelWidth / 2, labelY, labelWidth, labelHeight, juce::Justification::centred);

    // Draw amplitude axis labels
    g.setColour(juce::Colours::grey);
    g.setFont(9.0f);
    g.drawText("1", bounds.getX() - 22, bounds.getY() - 6, 18, 12, juce::Justification::centredRight);
    g.drawText("0", bounds.getX() - 22, bounds.getBottom() - 6, 18, 12, juce::Justification::centredRight);
}

//==============================================================================
float EnvelopeVisualizer::getAttack() const
{
    return apvts.getRawParameterValue("attack")->load();
}

float EnvelopeVisualizer::getDecay() const
{
    return apvts.getRawParameterValue("decay")->load();
}

float EnvelopeVisualizer::getSustain() const
{
    return apvts.getRawParameterValue("sustain")->load();
}

float EnvelopeVisualizer::getRelease() const
{
    return apvts.getRawParameterValue("release")->load();
}

float EnvelopeVisualizer::timeToX(float time, float totalTime, float width) const
{
    if (totalTime <= 0)
        return 0;
    return (time / totalTime) * width;
}

float EnvelopeVisualizer::getTotalDisplayTime() const
{
    return getAttack() + getDecay() + SustainDisplayTime + getRelease();
}

//==============================================================================
// Color helpers - get ADSR colors from configuration
juce::Colour EnvelopeVisualizer::getAttackColour() const
{
    return ConfigurationManager::getInstance().getThemeColour("colors.envelope.attack", juce::Colour(0xff4caf50));
}

juce::Colour EnvelopeVisualizer::getDecayColour() const
{
    return ConfigurationManager::getInstance().getThemeColour("colors.envelope.decay", juce::Colour(0xffffc107));
}

juce::Colour EnvelopeVisualizer::getSustainColour() const
{
    return ConfigurationManager::getInstance().getThemeColour("colors.envelope.sustain", juce::Colour(0xff2196f3));
}

juce::Colour EnvelopeVisualizer::getReleaseColour() const
{
    return ConfigurationManager::getInstance().getThemeColour("colors.envelope.release", juce::Colour(0xfff44336));
}

} // namespace vizasynth
