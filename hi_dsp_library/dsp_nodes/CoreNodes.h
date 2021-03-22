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
 *   which also must be licensed for commercial applications:
 *
 *   http://www.juce.com
 *
 *   ===========================================================================
 */

#pragma once

namespace scriptnode {
using namespace juce;
using namespace hise;
using namespace snex;
using namespace snex::Types;


#if INCLUDE_BIG_SCRIPTNODE_OBJECT_COMPILATION
namespace container
{

template <class P, typename... Ts> using frame1_block = wrap::frame<1, container::chain<P, Ts...>>;
template <class P, typename... Ts> using frame2_block = wrap::frame<2, container::chain<P, Ts...>>;
template <class P, typename... Ts> using frame4_block = wrap::frame<4, container::chain<P, Ts...>>;
template <class P, typename... Ts> using framex_block = wrap::frame_x< container::chain<P, Ts...>>;
template <class P, typename... Ts> using oversample2x = wrap::oversample<2,   container::chain<P, Ts...>, init::oversample>;
template <class P, typename... Ts> using oversample4x = wrap::oversample<4,   container::chain<P, Ts...>, init::oversample>;
template <class P, typename... Ts> using oversample8x = wrap::oversample<8,   container::chain<P, Ts...>, init::oversample>;
template <class P, typename... Ts> using oversample16x = wrap::oversample<16, container::chain<P, Ts...>, init::oversample>;
template <class P, typename... Ts> using modchain = wrap::control_rate<chain<P, Ts...>>;

}
#endif



namespace core
{



struct table: public scriptnode::data::base
{
	SET_HISE_NODE_ID("table");
	SN_GET_SELF_AS_OBJECT(table);

	HISE_EMPTY_HANDLE_EVENT;
	HISE_EMPTY_SET_PARAMETER;
	HISE_EMPTY_INITIALISE;
	HISE_EMPTY_CREATE_PARAM;

	using TableSpanType = span<float, SAMPLE_LOOKUP_TABLE_SIZE>;

	bool isPolyphonic() const { return false; };
	static constexpr bool isNormalisedModulation() { return true; }

	bool handleModulation(double& value)
	{
		return currentValue.getChangedValue(value);
	}

	void prepare(PrepareSpecs ps)
	{
		smoothedValue.prepare(ps.sampleRate, 20.0);
	}

	void reset() noexcept
	{
		currentValue.setModValue(0.0);
		smoothedValue.reset();
	}

	TableSpanType* getTableData()
	{
		return reinterpret_cast<TableSpanType*>(externalData.data);
	}

	template <typename ProcessDataType> void process(ProcessDataType& data) noexcept
	{
		DataReadLock l(this);

		if (!tableData.isEmpty())
		{
			float v = 0.0f;

			for (auto& ch : data)
			{
				for (auto& s : data.toChannelData(ch))
				{
					processFloat(s);
				}
			}

			externalData.setDisplayedValue(v);
		}
	}

	void processFloat(float& s)
	{
		InterpolatorType ip(hmath::abs(s));
		s *= tableData.interpolate(ip);
	}

	void setExternalData(const ExternalData& d, int) override
	{
		base::setExternalData(d, 0);
		d.referBlockTo(tableData, 0);
	}

	template <typename FrameDataType> void processFrame(FrameDataType& data) noexcept
	{
		DataReadLock l(this);

		if (!tableData.isEmpty())
		{
			for(auto& s: data)
				processFloat(s);
		}
	}

	sfloat smoothedValue;
	ModValue currentValue;
	block tableData;

	using TableClampType = index::clamped<SAMPLE_LOOKUP_TABLE_SIZE, false>;
	using InterpolatorType = index::lerp<index::normalised<float, TableClampType>>;

	JUCE_DECLARE_WEAK_REFERENCEABLE(table);
};

class peak
{
public:

	SET_HISE_NODE_ID("peak");
	SN_GET_SELF_AS_OBJECT(peak);

	HISE_EMPTY_PREPARE;
	HISE_EMPTY_CREATE_PARAM;
	HISE_EMPTY_HANDLE_EVENT;
	HISE_EMPTY_INITIALISE;

	bool isPolyphonic() const { return false; }

	bool handleModulation(double& value)
	{
		value = max;
		return true;
	}

	void reset() noexcept;;

	static constexpr bool isNormalisedModulation() { return true; }

	template <typename ProcessDataType> void process(ProcessDataType& data)
	{
		snex::hmath Math;

		max = 0.0f;

		for (auto& ch : data)
		{
			auto range = FloatVectorOperations::findMinAndMax(data.toChannelData(ch).begin(), data.getNumSamples());
			max = jmax<float>(max, Math.abs(range.getStart()), Math.abs(range.getEnd()));
		}
	}

	template <typename FrameDataType> void processFrame(FrameDataType& data)
	{
		snex::hmath Math;

		max = 0.0;

		for (auto& s : data)
			max = Math.max(max, Math.abs((double)s));
	}

	// This is no state variable, so we don't need it to be polyphonic...
	double max = 0.0;
};

class recorder: public data::base
{
public:

	enum class RecordingState
	{
		Idle,
		Recording,
		WaitingForStop,
		numStates
	};

	enum class Parameters
	{
		State,
		RecordingLength
	};

	DEFINE_PARAMETERS
	{
		DEF_PARAMETER(State, recorder);
		DEF_PARAMETER(RecordingLength, recorder);
	}
	

	SET_HISE_NODE_ID("recorder");
	SN_GET_SELF_AS_OBJECT(recorder);

	static constexpr bool isPolyphonic() { return false; }

	HISE_EMPTY_MOD;
	HISE_EMPTY_HANDLE_EVENT;
	HISE_EMPTY_INITIALISE;

	void reset()
	{
		recordingIndex = 0;
	}

	void setExternalData(const snex::ExternalData& d, int index) override
	{
		if (updater == nullptr)
		{
			if (auto gu = d.obj->getUpdater().getGlobalUIUpdater())
				updater = new InternalUpdater(*this, gu);
		}

		base::setExternalData(d, index);
	}

	void prepare(PrepareSpecs ps)
	{
		lastSpecs = ps;

		if (updater != nullptr)
			updater->resizeFlag.store(true);
	}

	void setState(double state)
	{
		auto thisState = (state > 0.5) ? RecordingState::Recording : RecordingState::Idle;

		if (currentState != thisState)
		{
			currentState = thisState;
			recordingIndex = 0;
		}
	}

	void setRecordingLength(double lengthInMilliseconds)
	{
		bufferSize = lengthInMilliseconds / 1000.0 * lastSpecs.sampleRate;

		if (updater != nullptr)
			updater->resizeFlag.store(true);
	}

	void createParameters(ParameterDataList& data)
	{
		{
			DEFINE_PARAMETERDATA(recorder, State);
			p.setParameterValueNames({ "On", "Off" });
			data.add(std::move(p));
		}

		{
			DEFINE_PARAMETERDATA(recorder, RecordingLength);
			p.setRange({ 0.0, 2000.0, 0.1 });
			data.add(std::move(p));
		}
	}

	void flush()
	{
		SimpleReadWriteLock::ScopedReadLock l(bufferLock);

		if (auto af = dynamic_cast<MultiChannelAudioBuffer*>(externalData.obj))
		{
			af->loadBuffer(recordingBuffer, lastSpecs.sampleRate);
		}

		currentState = RecordingState::Idle;
	}

	void rebuildBuffer()
	{
		AudioSampleBuffer newBuffer(lastSpecs.numChannels, bufferSize);
		newBuffer.clear();

		{
			SimpleReadWriteLock::ScopedWriteLock l(bufferLock);
			std::swap(newBuffer, recordingBuffer);
			recordingIndex = 0;
		}
	}

	template <typename ProcessDataType> void process(ProcessDataType& d)
	{
		if (currentState == RecordingState::Recording)
		{
			SimpleReadWriteLock::ScopedReadLock l(bufferLock);

			if (d.getNumChannels() == 2)
				FrameConverters::forwardToFrameStereo(this, d);
			if (d.getNumChannels() == 1)
				FrameConverters::forwardToFrameMono(this, d);
		}
	}

	template <typename FrameDataType> void processFrame(FrameDataType& data)
	{
		int numSamplesInBuffer = recordingBuffer.getNumSamples();

		if (currentState == RecordingState::Recording && isPositiveAndBelow(recordingIndex, numSamplesInBuffer))
		{
			for (int i = 0; i < data.size(); i++)
				recordingBuffer.setSample(i, recordingIndex, data[i]);

			recordingIndex++;
		}


		if (recordingIndex++ >= numSamplesInBuffer)
		{
			recordingIndex = 0;
			currentState = RecordingState::WaitingForStop;

			if (updater != nullptr)
				updater->flushFlag.store(true);
		}
	}

private:

	struct InternalUpdater : public PooledUIUpdater::SimpleTimer
	{
		InternalUpdater(recorder& p, PooledUIUpdater* u) :
			SimpleTimer(u),
			parent(p)
		{};

		void timerCallback() override
		{
			if (resizeFlag)
				refreshBufferSize();

			if (flushFlag)
				flushBuffer();
		}

		void flushBuffer()
		{
			parent.flush();
			flushFlag.store(false);
		}

		void refreshBufferSize()
		{
			parent.rebuildBuffer();
			resizeFlag.store(false);
		}

		std::atomic<bool> resizeFlag = false;
		std::atomic<bool> flushFlag = false;

		recorder& parent;
	};

	ScopedPointer<InternalUpdater> updater;

	int recordingIndex = 0;
	int bufferSize = 0;
	RecordingState currentState = RecordingState::Idle;
	PrepareSpecs lastSpecs;
	SimpleReadWriteLock bufferLock;
	AudioSampleBuffer recordingBuffer;
};

class mono2stereo : public HiseDspBase
{
public:

	SET_HISE_NODE_ID("mono2stereo");
	SN_GET_SELF_AS_OBJECT(mono2stereo);

	HISE_EMPTY_PREPARE;
	HISE_EMPTY_CREATE_PARAM;
	HISE_EMPTY_RESET;
	HISE_EMPTY_HANDLE_EVENT;
	HISE_EMPTY_SET_PARAMETER;
	
	template <typename ProcessDataType> void process(ProcessDataType& data)
	{
		if (data.getNumChannels() >= 2)
        {
            auto dst = data[1];
            Math.vcopy(dst, data[0]);
        }
			
	}

	template <typename FrameDataType> void processFrame(FrameDataType& data)
	{
		if (data.size() >= 2)
			data[1] = data[0];
	}

	hmath Math;
};

class empty : public HiseDspBase
{
public:

	SET_HISE_NODE_ID("empty");
	SN_GET_SELF_AS_OBJECT(empty);

	HISE_EMPTY_PREPARE;
	HISE_EMPTY_CREATE_PARAM;
	HISE_EMPTY_PROCESS;
	HISE_EMPTY_PROCESS_SINGLE;
	HISE_EMPTY_RESET;
	HISE_EMPTY_HANDLE_EVENT;
};



template <class ShaperType> struct snex_shaper
{
	SET_HISE_NODE_ID("snex_shaper");
	SN_GET_SELF_AS_OBJECT(snex_shaper);

	HISE_EMPTY_HANDLE_EVENT;
	
	void prepare(PrepareSpecs ps)
	{
		shaper.prepare(ps);
	}

	void reset()
	{
		shaper.reset();
	};

	void initialise(NodeBase* n)
	{
		if constexpr (prototypes::check::initialise<ShaperType>::value)
			shaper.initialise(n);
	}

	static constexpr bool isPolyphonic() { return false; }

	ShaperType shaper;

	template <typename ProcessDataType> void process(ProcessDataType& data)
	{
		shaper.process(data);
	}

	template <typename FrameDataType> void processFrame(FrameDataType& data)
	{
		shaper.processFrame(data);
	}

	void setExternalData(const ExternalData& d, int index)
	{
		if constexpr (prototypes::check::setExternalData<ShaperType>::value)
			shaper.setExternalData(d, index);
	}

	HISE_EMPTY_CREATE_PARAM;
};

template <int NV> class ramp_impl : public HiseDspBase
{
public:

	enum class Parameters
	{
		PeriodTime,
		LoopStart,
		Gate,
		numParameters
	};

	static constexpr int NumVoices = NV;

	SET_HISE_POLY_NODE_ID("ramp");
	SN_GET_SELF_AS_OBJECT(ramp_impl);

	ramp_impl()
	{
		setPeriodTime(100.0);
	}

	void prepare(PrepareSpecs ps)
	{
		state.prepare(ps);

		sr = ps.sampleRate;
		setPeriodTime(periodTime);
	}

	void reset() noexcept
	{
		for (auto& s : state)
		{
			s.data.reset();
		}
	}

	static constexpr bool isNormalisedModulation() { return true; };

	DEFINE_PARAMETERS
	{
		DEF_PARAMETER(PeriodTime, ramp_impl);
		DEF_PARAMETER(LoopStart, ramp_impl);
		DEF_PARAMETER(Gate, ramp_impl);
	}

	template <typename ProcessDataType> void process(ProcessDataType& d)
	{
		auto& thisState = state.get();

		if (thisState.enabled)
		{
			double thisUptime = thisState.data.uptime;
			double thisDelta = thisState.data.uptimeDelta;

			for (auto c : d)
			{
				thisUptime = thisState.data.uptime;

				for (auto& s : d.toChannelData(c))
				{
					if (thisUptime > 1.0)
						thisUptime = thisState.loopStart;

					s += (float)thisUptime;

					thisUptime += thisDelta;
				}
			}

			thisState.data.uptime = thisUptime;
			thisState.modValue.setModValue(thisUptime);
		}
	}

	bool handleModulation(double& v)
	{
		return state.get().modValue.getChangedValue(v);
	}

	template <typename FrameDataType> void processFrame(FrameDataType& data)
	{
		auto& s = state.get();

		if (s.enabled)
		{
			auto newValue = s.data.tick();

			if (newValue > 1.0)
			{
				newValue = s.loopStart;
				s.data.uptime = newValue;
			}

			for (auto& s : data)
				s += (float)newValue;
			
			s.modValue.setModValue(newValue);
		}
	}

	void createParameters(ParameterDataList& data) override
	{
		{
			DEFINE_PARAMETERDATA(ramp_impl, PeriodTime);
			p.setRange({ 0.1, 1000.0, 0.1 });
			p.setDefaultValue(100.0);
			data.add(std::move(p));
		}

		{
			DEFINE_PARAMETERDATA(ramp_impl, LoopStart);
			p.setDefaultValue(0.0);
			data.add(std::move(p));
		}

		{
			DEFINE_PARAMETERDATA(ramp_impl, Gate);
			p.setDefaultValue(1.0);
			data.add(std::move(p));
		}
	}

	HISE_EMPTY_HANDLE_EVENT;

	void setGate(double onOffValue)
	{
		bool shouldBeOn = onOffValue > 0.5;

		for (auto& s : state)
		{
			if (s.enabled != shouldBeOn)
			{
				s.enabled = shouldBeOn;
				s.data.uptime = 0.0;
			}
		}
	}

	void setPeriodTime(double periodTimeMs)
	{
		if (periodTimeMs > 0.0)
		{
			periodTime = periodTimeMs;

			if (sr > 0.0)
			{
				auto s = periodTime * 0.001;
				auto inv = 1.0 / jmax(0.00001, s);

				auto newUptimeDelta = jmax(0.0000001, inv / sr);

				for (auto& s : state)
					s.data.uptimeDelta = newUptimeDelta;
			}
		}
	}

	void setLoopStart(double newLoopStart)
	{
		auto v = jlimit(0.0, 1.0, newLoopStart);

		for (auto& d : state)
			d.loopStart = v;
	}

private:

	struct State
	{
		OscData data;
		double loopStart = 0.0;
		bool enabled = false;
		ModValue modValue;
	};

	double sr = 44100.0;
	double periodTime = 500.0;
	PolyData<State, NumVoices> state;
};


DEFINE_EXTERN_NODE_TEMPLATE(ramp, ramp_poly, ramp_impl);






template <int NV> class oscillator_impl: public OscillatorDisplayProvider
{
public:

	enum class Parameters
	{
		Mode,
		Frequency,
		PitchMultiplier,
		numParameters
	};

	constexpr static int NumVoices = NV;

	SET_HISE_POLY_NODE_ID("oscillator");
	SN_GET_SELF_AS_OBJECT(oscillator_impl);

	oscillator_impl() = default;

	HISE_EMPTY_INITIALISE;

	void reset()
	{
		for (auto& s : voiceData)
			s.reset();
	}

	void prepare(PrepareSpecs ps)
	{
		voiceData.prepare(ps);
		sr = ps.sampleRate;
		setFrequency(freqValue);
	}
	
	void process(snex::Types::ProcessData<1>& data)
	{
		auto f = data.toFrameData();

		currentVoiceData = &voiceData.get();

		while (f.next())
			processFrame(f.toSpan());
	}

	void processFrame(snex::Types::span<float, 1>& data)
	{
		if (currentVoiceData == nullptr)
			currentVoiceData = &voiceData.get();

		auto& s = data[0];

		switch (currentMode)
		{
		case Mode::Sine:	 s += tickSine(*currentVoiceData); break;
		case Mode::Triangle: s += tickTriangle(*currentVoiceData); break;
		case Mode::Saw:		 s += tickSaw(*currentVoiceData); break;
		case Mode::Square:	 s += tickSquare(*currentVoiceData); break;
		case Mode::Noise:	 s += Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
		}
	}

	void handleHiseEvent(HiseEvent& e)
	{
		if (useMidi && e.isNoteOn())
		{
			setFrequency(e.getFrequency());
		}
	}


	void createParameters(ParameterDataList& data)
	{
		{
			DEFINE_PARAMETERDATA(oscillator_impl, Mode);
			p.setParameterValueNames(modes);
			data.add(std::move(p));
		}
		{
			DEFINE_PARAMETERDATA(oscillator_impl, Frequency);
			p.setRange({ 20.0, 20000.0, 0.1 });
			p.setDefaultValue(220.0);
			p.setSkewForCentre(1000.0);
			data.add(std::move(p));
		}
		{
			parameter::data p("Freq Ratio");
			p.setRange({ 1.0, 16.0, 1.0 });
			p.setDefaultValue(1.0);
			p.callback = parameter::inner<oscillator_impl, (int)Parameters::PitchMultiplier>(*this);
			data.add(std::move(p));
		}
	}

	void setMode(double newMode)
	{
		currentMode = (Mode)(int)newMode;
	}

	void setFrequency(double newFrequency)
	{
		freqValue = newFrequency;

		if (sr > 0.0)
		{
			auto newUptimeDelta = (double)(freqValue / sr * (double)sinTable->getTableSize());

			for (auto& d : voiceData)
				d.uptimeDelta = newUptimeDelta;
		}
	}

	void setPitchMultiplier(double newMultiplier)
	{
		auto pitchMultiplier = jlimit(0.01, 100.0, newMultiplier);

		for (auto& d : voiceData)
			d.multiplier = pitchMultiplier;
	}

	DEFINE_PARAMETERS
	{
		DEF_PARAMETER(Mode, oscillator_impl);
		DEF_PARAMETER(Frequency, oscillator_impl);
		DEF_PARAMETER(PitchMultiplier, oscillator_impl);
	}

	PARAMETER_MEMBER_FUNCTION;

	double sr = 44100.0;
	PolyData<OscData, NumVoices> voiceData;

	OscData* currentVoiceData = nullptr;

	double freqValue = 220.0;
};

using oscillator = wrap::fix<1, oscillator_impl<1>>;
using oscillator_poly = wrap::fix<1, oscillator_impl<NUM_POLYPHONIC_VOICES>>;

class fm : public HiseDspBase
{
public:

	enum class Parameters
	{
		Frequency,
		Modulator,
		FreqMultiplier
	};

	DEFINE_PARAMETERS
	{
		DEF_PARAMETER(Frequency, fm);
		DEF_PARAMETER(Modulator, fm);
		DEF_PARAMETER(FreqMultiplier, fm);
	}

	SET_HISE_NODE_ID("fm");
	SN_GET_SELF_AS_OBJECT(fm);

	bool isPolyphonic() const { return true; }

	void prepare(PrepareSpecs ps);
	void reset();

	template <typename ProcessDataType> void process(ProcessDataType& data)
	{
		FrameConverters::forwardToFrameMono(this, data);
	}

	template <typename FrameDataType> void processFrame(FrameDataType& d)
	{
		auto& od = oscData.get();
		double modValue = (double)d[0];
		d[0] = sinTable->getInterpolatedValue(od.tick());
		od.uptime += modGain.get() * modValue;
	}

	void createParameters(ParameterDataList& data) override;

	void handleHiseEvent(HiseEvent& e);
	
	void setFreqMultiplier(double input);
	void setModulator(double newGain);
	void setFrequency(double newFrequency);

private:

	double sr = 0.0;
	double freqMultiplier = 1.0;

	PolyData<OscData, NUM_POLYPHONIC_VOICES> oscData;
	PolyData<double, NUM_POLYPHONIC_VOICES> modGain;

	SharedResourcePointer<SineLookupTable<2048>> sinTable;
};

template <int V> class gain_impl : public HiseDspBase
{
public:

	enum class Parameters
	{
		Gain,
		Smoothing,
		ResetValue
	};

	DEFINE_PARAMETERS
	{
		DEF_PARAMETER(Gain, gain_impl);
		DEF_PARAMETER(Smoothing, gain_impl);
		DEF_PARAMETER(ResetValue, gain_impl);
	}

	static constexpr int NumVoices = V;

	SET_HISE_POLY_NODE_ID("gain");
	SN_GET_SELF_AS_OBJECT(gain_impl);

	void prepare(PrepareSpecs ps)
	{
		gainer.prepare(ps);
		sr = ps.sampleRate;

		setSmoothing(smoothingTime);
	}

	template <typename FrameDataType> void processFrame(FrameDataType& data)
	{
		auto gainFactor = gainer.get().advance();

		for (auto& s : data)
			s *= gainFactor;
	}

	template <typename ProcessDataType> void process(ProcessDataType& data)
	{
		auto& thisGainer = gainer.get();

		if (thisGainer.isActive())
			FrameConverters::forwardToFrame16(this, data);
		else
		{
			auto gainFactor = thisGainer.get();

			for (auto ch : data)
				data.toChannelData(ch) *= gainFactor;
		}
	}

	void reset() noexcept
	{
		if (sr == 0.0)
			return;

		for (auto& g : gainer)
		{
			g.set(resetValue);
			g.reset();
			g.set((float)gainValue);
		}
	}

	void createParameters(ParameterDataList& data) override
	{
		{
			DEFINE_PARAMETERDATA(gain_impl, Gain);
			p.setRange({ -100.0, 0.0, 0.1 });
			p.setSkewForCentre(-12.0);
			p.setDefaultValue(0.0);
			data.add(std::move(p));
		}
		{
			DEFINE_PARAMETERDATA(gain_impl, Smoothing);
			p.setRange({ 0.0, 1000.0, 0.1 });
			p.setSkewForCentre(100.0);
			p.setDefaultValue(20.0);
			data.add(std::move(p));
		}
		{
			DEFINE_PARAMETERDATA(gain_impl, ResetValue);
			p.setRange({ -100.0, 0.0, 0.1 });
			p.setSkewForCentre(-12.0);
			p.setDefaultValue(0.0);
			data.add(std::move(p));
		}
	}

	void handleHiseEvent(HiseEvent& e)
	{
		if (e.isNoteOn())
			reset();
	}


	void setGain(double newValue)
	{
		gainValue = Decibels::decibelsToGain(newValue);

		float gf = (float)gainValue;

		for (auto& g : gainer)
			g.set(gf);
	}

	void setSmoothing(double smoothingTimeMs)
	{
		smoothingTime = smoothingTimeMs;

		if (sr <= 0.0)
			return;

		for (auto& g : gainer)
			g.prepare(sr, smoothingTime);
	}

	void setResetValue(double newResetValue)
	{
		resetValue = Decibels::decibelsToGain(newResetValue);
	}

	double gainValue = 1.0;
	double sr = 0.0;
	double smoothingTime = 0.02;
	double resetValue = 0.0;

	PolyData<sfloat, NumVoices> gainer;
};

DEFINE_EXTERN_NODE_TEMPLATE(gain, gain_poly, gain_impl);



template <int NV> class smoother_impl : public HiseDspBase
{
public:

	enum class Parameters
	{
		SmoothingTime,
		DefaultValue
	};

	DEFINE_PARAMETERS
	{
		DEF_PARAMETER(SmoothingTime, smoother_impl);
		DEF_PARAMETER(DefaultValue, smoother_impl);
	}

	static constexpr int NumVoices = NV;

	SET_HISE_POLY_NODE_ID("smoother");
	SN_GET_SELF_AS_OBJECT(smoother_impl);

	smoother_impl();

	HISE_EMPTY_INITIALISE;
    
	void createParameters(ParameterDataList& data) override
	{
		{
			DEFINE_PARAMETERDATA(smoother_impl, DefaultValue);
			p.setDefaultValue(0.0);
			data.add(std::move(p));
		}
		{
			DEFINE_PARAMETERDATA(smoother_impl, SmoothingTime);
			p.setRange({ 0.0, 2000.0, 0.1 });
			p.setSkewForCentre(100.0);
			p.setDefaultValue(100.0);
			data.add(std::move(p));
		}
	}

	void prepare(PrepareSpecs ps) 
	{
		modValue.prepare(ps);
		smoother.prepare(ps);
		auto sr = ps.sampleRate;
		auto sm = smoothingTimeMs;

		for (auto& s : smoother)
		{
			s.prepareToPlay(sr);
			s.setSmoothingTime((float)sm);
		}
	}

	void reset()
	{
		auto d = defaultValue;

		for (auto& s : smoother)
			s.resetToValue(d, 0.0);
	}

	template <typename FrameDataType> void processFrame(FrameDataType& data)
	{
		data[0] = smoother.get().smooth(data[0]);
		modValue.get().setModValue(smoother.get().getDefaultValue());
	}

	template <typename ProcessDataType> void process(ProcessDataType& data)
	{
		smoother.get().smoothBuffer(data[0].data, data.getNumSamples());
		modValue.get().setModValue(smoother.get().getDefaultValue());
	}

	bool handleModulation(double& value)
	{
		return modValue.get().getChangedValue(value);
	}

	void handleHiseEvent(HiseEvent& e)
	{
		if (e.isNoteOn())
			reset();
	}

	void setSmoothingTime(double newSmoothingTime)
	{
		smoothingTimeMs = newSmoothingTime;

		for (auto& s : smoother)
			s.setSmoothingTime((float)newSmoothingTime);
	}

	void setDefaultValue(double newDefaultValue)
	{
		defaultValue = (float)newDefaultValue;

		auto d = defaultValue; auto sm = smoothingTimeMs;

		for (auto& s : smoother)
			s.resetToValue((float)d, (float)sm);
	}

	double smoothingTimeMs = 100.0;
	float defaultValue = 0.0f;
	PolyData<hise::Smoother, NumVoices> smoother;
	PolyData<ModValue, NumVoices> modValue;
};

DEFINE_EXTERN_NODE_TEMPLATE(smoother, smoother_poly, smoother_impl);


template <int V> class ramp_envelope_impl : public HiseDspBase
{
public:

	enum class Parameters
	{
		Gate,
		RampTime
	};

	DEFINE_PARAMETERS
	{
		DEF_PARAMETER(Gate, ramp_envelope_impl);
		DEF_PARAMETER(RampTime, ramp_envelope_impl);
	}

	static constexpr int NumVoices = V;

	SET_HISE_POLY_NODE_ID("ramp_envelope");
	SN_GET_SELF_AS_OBJECT(ramp_envelope_impl);

	/* ============================================================================ ramp_impl */
	ramp_envelope_impl()
	{

	}

	void createParameters(ParameterDataList& data) override
	{
		{
			DEFINE_PARAMETERDATA(ramp_envelope_impl, Gate);
			p.setParameterValueNames({ "On", "Off" });
			data.add(std::move(p));
		}

		{
			DEFINE_PARAMETERDATA(ramp_envelope_impl, RampTime);
			p.setRange({ 0.0, 2000.0, 0.1 });
			data.add(std::move(p));
		}
	}

	void prepare(PrepareSpecs ps)
	{
		gainer.prepare(ps);
		sr = ps.sampleRate;
		setRampTime(attackTimeSeconds * 1000.0);
	}

	void reset()
	{
		for (auto& g : gainer)
			g.setValueWithoutSmoothing(0.0f);
	}

	template <typename FrameDataType> void processFrame(FrameDataType& data)
	{
		auto& thisG = gainer.get();
		auto v = thisG.getNextValue();

		for (auto& s : data)
			s *= v;
	}

	template <typename ProcessDataType> void process(ProcessDataType& data)
	{
		auto& thisG = gainer.get();

		if (thisG.isSmoothing())
			snex::Types::FrameConverters::forwardToFrame16(this, data);
		else
		{
			auto v = thisG.getTargetValue();

			for (auto ch : data)
				hmath::vmuls(data.toChannelData(ch), v);

			if (thisG.getCurrentValue() == 0.0)
				data.setResetFlag();
		}
	}

	void handleHiseEvent(HiseEvent& e)
	{
		if (e.isNoteOnOrOff())
			gainer.get().setTargetValue(e.isNoteOn() ? 1.0f : 0.0f);
	}

	void setGate(double newValue)
	{
		for (auto& g : gainer)
			g.setTargetValue(newValue > 0.5 ? 1.0f : 0.0f);
	}

	void setRampTime(double newAttackTimeMs)
	{
		attackTimeSeconds = newAttackTimeMs * 0.001;

		if (sr > 0.0)
		{
			for (auto& g : gainer)
				g.reset(sr, attackTimeSeconds);
		}
	}

	double attackTimeSeconds = 0.01;
	double sr = 0.0;

	PolyData<LinearSmoothedValue<float>, NumVoices> gainer;
};

DEFINE_EXTERN_NODE_TEMPLATE(ramp_envelope, ramp_envelope_poly, ramp_envelope_impl);


template <typename T> struct snex_osc_base
{
	void initialise(NodeBase* n)
	{
		if constexpr (prototypes::check::initialise<T>::value)
			oscType.initialise(n);
	}

	void setExternalData(const ExternalData& d, int index)
	{
		if constexpr (prototypes::check::setExternalData<T>::value)
			oscType.setExternalData(d, index);
	}

	T oscType;
};

template <int NV, typename T> struct snex_osc_impl : snex_osc_base<T>
{
	enum class Parameters
	{
		Frequency,
		PitchMultiplier
	};

	static constexpr int NumVoices = NV;

	DEFINE_PARAMETERS
	{
		DEF_PARAMETER(Frequency, snex_osc_impl);
		DEF_PARAMETER(PitchMultiplier, snex_osc_impl);

		if (P > 1)
		{
			auto typed = static_cast<snex_osc_impl*>(obj);
			typed->oscType.setParameter<P - 2>(value);
		}
	}

	PARAMETER_MEMBER_FUNCTION;

	SET_HISE_POLY_NODE_ID("snex_osc");
	SN_GET_SELF_AS_OBJECT(snex_osc_impl);

	void prepare(PrepareSpecs ps)
	{
		sampleRate = ps.sampleRate;
		voiceIndex = ps.voiceIndex;
		oscData.prepare(ps);
		reset();
	}

	void reset()
	{
		for (auto& o : oscData)
			o.reset();
	}

	template <typename FrameDataType> void processFrame(FrameDataType& data)
	{
		auto& thisData = oscData.get();
		auto uptime = thisData.tick();
		data[0] += this->oscType.tick(uptime);
	}

	template <typename ProcessDataType> void process(ProcessDataType& data)
	{
		auto& thisData = oscData.get();

		OscProcessData op;

		op.data.referToRawData(data.getRawDataPointers()[0], data.getNumSamples());
		op.uptime = thisData.uptime;
		op.delta = thisData.uptimeDelta * thisData.multiplier;
		op.voiceIndex = voiceIndex->getVoiceIndex();

		this->oscType.process(op);
		thisData.uptime += op.delta * (double)data.getNumSamples();
	}

	void handleHiseEvent(HiseEvent& e)
	{
		if (e.isNoteOn())
			setFrequency(e.getFrequency());
	}

	void setFrequency(double newValue)
	{
		if (sampleRate > 0.0)
		{
			auto cyclesPerSecond = newValue;
			auto cyclesPerSample = cyclesPerSecond / sampleRate;

			for (auto& o : oscData)
				o.uptimeDelta = cyclesPerSample;
		}
	}

	void setPitchMultiplier(double newMultiplier)
	{
		newMultiplier = jlimit(0.01, 100.0, newMultiplier);

		for (auto& o : oscData)
			o.multiplier = newMultiplier;
	}

	

	double sampleRate = 0.0;

	void createParameters(ParameterDataList& data)
	{
		{
			DEFINE_PARAMETERDATA(snex_osc_impl, Frequency);
			p.setRange({ 20.0, 20000.0, 0.1 });
			p.setSkewForCentre(1000.0);
			p.setDefaultValue(220.0);
			data.add(std::move(p));
		}

		{
			DEFINE_PARAMETERDATA(snex_osc_impl, PitchMultiplier);
			p.setRange({ 1.0, 16.0, 1.0 });
			p.setDefaultValue(1.0);
			data.add(std::move(p));
		}
	}

	PolyHandler* voiceIndex = nullptr;
	PolyData<OscData, NumVoices> oscData;
};

template <typename OscType> using snex_osc = snex_osc_impl<1, OscType>;
template <typename OscType> using snex_osc_poly = snex_osc_impl<NUM_POLYPHONIC_VOICES, OscType>;

} // namespace core




}