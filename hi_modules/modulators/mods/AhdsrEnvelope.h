/*  ===========================================================================
*
*   This file is part of HISE.
*   Copyright 2016 Christoph Hart
*
*   HISE is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   HISE is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with HISE.  If not, see <http://www.gnu.org/licenses/>.
*
*   Commercial licenses for using HISE in an closed source project are
*   available on request. Please visit the project's website to get more
*   information about commercial licensing:
*
*   http://www.hise.audio/
*
*   HISE is based on the JUCE library,
*   which must be separately licensed for closed source applications:
*
*   http://www.juce.com
*
*   ===========================================================================
*/

#ifndef HI_AHDSRENVELOPE_H_INCLUDED
#define HI_AHDSRENVELOPE_H_INCLUDED

namespace hise { using namespace juce;



/** @brief A pretty common envelope type with 5 states. @ingroup modulatorTypes

### AHDSR Envelope

A pretty common envelope type with 5 states. (Go to http://en.wikiaudio.org/ADSR_envelope for a general description on how an envelope works)

The code is based on the example envelope code from http://earlevel.com.

The Modulator has five states: Attack, Hold, Decay, Sustain and Release and allows modulation of 
the attack time and level, the decay time and the release time with VoiceStartModulators.

Unlike the [SimpleEnvelope](#SimpleEnvelope), this envelope has a exponential curve, so it sounds nicer (but is a little bit more resource-hungry).

ID | Parameter | Description
-- | --------- | -----------
0 | Attack | the attack time in milliseconds
1 | AttackLevel | the attack level in decibel
2 | Hold | the hold time in milliseconds
3 | Decay | the decay time in milliseconds
4 | Sustain | the sustain level in decibel
5 | Release | the release time in milliseconds
*/
class AhdsrEnvelope: public EnvelopeModulator	
{
public:

	SET_PROCESSOR_NAME("AHDSR", "AHDSR Envelope", "A envelope modulator with five states")

	/// @brief special parameters for AhdsrEnvelope
	enum SpecialParameters
	{
		Attack = EnvelopeModulator::Parameters::numParameters,		 ///< the attack time in milliseconds
		AttackLevel, ///< the attack level in decibel
		Hold,		 ///< the hold time in milliseconds
		Decay,		 ///< the decay time in milliseconds
		Sustain,	 ///< the sustain level in decibel
		Release,	 ///< the release time in milliseconds
		AttackCurve, ///< the attack curve (0.0 = concave, 1.0 = convex)
		DecayCurve,  ///< the decay curve (0.0 = concave, 1.0 = linear)
		ReleaseCurve,///< the release curve (0.0 = concave, 1.0 = linear)
		EcoMode,	 ///< uses 16x downsampling and linear interpolation for calculating the envelope curve
		numTotalParameters
	};

	enum EditorStates
	{
		AttackTimeChainShown = Processor::numEditorStates,
		AttackLevelChainShown,
		DecayTimeChainShown,
		SustainLevelChainShown,
		ReleaseTimeChainShown,
		numEditorStates
	};

	enum InternalChains
	{
		AttackTimeChain = 0,
		AttackLevelChain,
		DecayTimeChain,
		SustainLevelChain,
		ReleaseTimeChain,
		numInternalChains
	};

	AhdsrEnvelope(MainController *mc, const String &id, int voiceAmount, Modulation::Mode m);
	~AhdsrEnvelope() {};

	void restoreFromValueTree(const ValueTree &v) override;;
	ValueTree exportAsValueTree() const override;

	Processor *getChildProcessor(int processorIndex) override;;
	const Processor *getChildProcessor(int processorIndex) const override;;
	int getNumChildProcessors() const override { return numInternalChains; };
	int getNumInternalChains() const override {return numInternalChains;};

	float startVoice(int voiceIndex) override;	
	void stopVoice(int voiceIndex) override;
	void reset(int voiceIndex) override;;

	void calculateBlock(int startSample, int numSamples);;

	void handleHiseEvent(const HiseEvent &e) override;
	

	ProcessorEditorBody *createEditor(ProcessorEditor* parentEditor) override;

	

	float getDefaultValue(int parameterIndex) const override;
	void setInternalAttribute (int parameter_index, float newValue) override;;

	

	float getAttribute(int parameter_index) const override;;

	void prepareToPlay(double sampleRate, int samplesPerBlock) override;

	/** @brief returns \c true, if the envelope is not IDLE and not bypassed. */
	bool isPlaying(int voiceIndex) const override;;
    
    /** @internal The container for the envelope state. */
    struct AhdsrEnvelopeState: public EnvelopeModulator::ModulatorState
	{
	public:

		AhdsrEnvelopeState(int voiceIndex, const AhdsrEnvelope *ownerEnvelope):
			ModulatorState(voiceIndex),
			current_state(IDLE),
			holdCounter(0),
			envelope(ownerEnvelope),
			current_value(0.0f),
			lastSustainValue(1.0f)
		{
			for(int i = 0; i < numInternalChains; i++) modValues[i] = 1.0f;
		};

		/** The internal states that this envelope has */
		enum EnvelopeState
		{
			ATTACK, 	///< attack phase (isPlaying() returns \c true)
			HOLD, 		///< hold phase
			DECAY, 		///< decay phase
			SUSTAIN, 	///< sustain phase (isPlaying() returns \c true)
			RETRIGGER, 	///< retrigger phase (monophonic only)
			RELEASE, 	///< attack phase (isPlaying() returns \c true)
			IDLE 		///< idle state (isPlaying() returns \c false.
		};

		/** Calculate the attack rate for the state. If the modulation value is 1.0f, they are simply copied from the envelope. */
		void setAttackRate(float rate);;
		
		/** Calculate the decay rate for the state. If the modulation value is 1.0f, they are simply copied from the envelope. */
		void setDecayRate(float rate);;
		
		/** Calculate the release rate for the state. If the modulation value is 1.0f, they are simply copied from the envelope. */
		void setReleaseRate(float rate);;

		const AhdsrEnvelope *envelope;

        /// the uptime
		int holdCounter;
		float current_value;

		int leftOverSamplesFromLastBuffer = 0;

		/** The ratio in which the attack time is altered. This is calculated by the internal ModulatorChain attackChain*/
		float modValues[numInternalChains];

		float attackLevel;
		float attackCoef;
		float attackBase;

		float decayCoef;
		float decayBase;

		float releaseCoef;
		float releaseBase;
		float release_delta;

		float lastSustainValue;

		EnvelopeState current_state;
	};

	ModulatorState *createSubclassedState(int voiceIndex) const override {return new AhdsrEnvelopeState(voiceIndex, this); };

	void calculateCoefficients(float timeInMilliSeconds, float base, float maximum, float &stateBase, float &stateCoeff) const;

	struct StateInfo
	{
		bool operator ==(const StateInfo& other) const
		{
			return other.state == state;
		};

		AhdsrEnvelopeState::EnvelopeState state = AhdsrEnvelopeState::IDLE;
		double changeTime = 0.0;
	};

	StateInfo getStateInfo() const
	{
		return stateInfo;
	};

private:

	StateInfo stateInfo;

	float getSampleRateForCurrentMode() const;

	void setAttackRate(float rate);
	void setDecayRate(float rate);
	void setReleaseRate(float rate);
	void setSustainLevel(float level);
	void setHoldTime(float holdTimeMs);
	void setTargetRatioA(float targetRatio);
	void setTargetRatioDR(float targetRatio);
	void setTargetRatioRR(float targetRatio);

	float calcCoef(float rate, float targetRatio) const;

	float calculateNewValue(int voiceIndex);
	
	void setAttackCurve(float newValue);
	void setDecayCurve(float newValue);
	void setReleaseCurve(float newValue);
	
	float inputValue;

	float attack;
	
	float attackLevel;

	float attackCurve;
	float decayCurve;
	float releaseCurve;

	float hold;
	float holdTimeSamples;

	float attackBase;

	float decay;
	float decayCoef;
	float decayBase;
	float targetRatioDR;

	float sustain;

	float release;
	float releaseCoef;
	float releaseBase;
	float targetRatioRR;

	AhdsrEnvelopeState *state;

	float release_delta;

	ModulatorChain::Collection internalChains;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AhdsrEnvelope)
	JUCE_DECLARE_WEAK_REFERENCEABLE(AhdsrEnvelope);
};

class AhdsrGraph : public Component,
				   public Timer
{
public:

	enum ColourIds
	{
		bgColour,
		fillColour,
		lineColour,
		outlineColour,
		numColourIds
	};

	AhdsrGraph(Processor *p);

	~AhdsrGraph();

	void paint(Graphics &g);
	void setUseFlatDesign(bool shouldUseFlatDesign);

	void timerCallback() override;

	void rebuildGraph();

	class Panel : public PanelWithProcessorConnection
	{
	public:

		SET_PANEL_NAME("AHDSRGraph");

		Panel(FloatingTile* parent);

		Identifier getProcessorTypeId() const override { return AhdsrEnvelope::getClassType(); }
		Component* createContentComponent(int /*index*/) override;
		void fillModuleList(StringArray& moduleList) override;
	};

private:

	AhdsrEnvelope::StateInfo lastState;

	bool flatDesign = false;

	WeakReference<Processor> processor;

	float attack = 0.0f;
	float attackLevel = 0.0f;
	float hold = 0.0f;
	float decay = 0.0f;
	float sustain = 0.0f;
	float release = 0.0f;
	float attackCurve = 0.0f;
	float decayCurve = 0.0f;
	float releaseCurve = 0.0f;

	Path envelopePath;
	Path attackPath;
	Path holdPath;
	Path decayPath;
	Path releasePath;

	JUCE_DECLARE_WEAK_REFERENCEABLE(AhdsrEnvelope);
};

} // namespace hise

#endif