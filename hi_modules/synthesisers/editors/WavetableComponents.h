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

#ifndef WAVETABLECOMPONENTS_H_INCLUDED
#define WAVETABLECOMPONENTS_H_INCLUDED

namespace hise {
using namespace juce;


class WaveformComponent : public Component,
						  public SafeChangeListener
{
public:

	struct WaveformFactory : public PathFactory
	{
		String getId() const override { return "Waveform Icons"; }

		Path createPath(const String& id) const override;
	};

	enum InterpolationMode
	{
		Truncate,
		LinearInterpolation,
		numInterpolationModes
	};

	using ScaleFunction = std::function<float(float)>;

	static float identity(float input) { return input; }

	enum WaveformType
	{
		Sine = 1,
		Triangle,
		Saw,
		Square,
		Noise,
		numWaveformTypes
	};

	class Broadcaster: public SuspendableTimer::Manager
	{
		class Updater : public SuspendableTimer
		{
		public:

			Updater(Broadcaster& p) :
				parent(p)
			{
				startTimer(30);
			}

			void timerCallback() override
			{
				if (changeFlag)
				{
					changeFlag = false;

					parent.updateData();
				}
			}

			std::atomic<bool> changeFlag;

			Broadcaster& parent;
		};


	public:

		Broadcaster():
			updater(*this)
		{}

		virtual ~Broadcaster() {};

		void suspendStateChanged(bool shouldBeSuspended) override
		{
			updater.suspendTimer(shouldBeSuspended);
		}
		
		void triggerWaveformUpdate() { updater.changeFlag = true; };

		void addWaveformListener(WaveformComponent* listener)
		{
			listeners.addIfNotAlreadyThere(listener);
		}

		void removeWaveformListener(WaveformComponent* listener)
		{
			listeners.removeAllInstancesOf(listener);
		
		}

		void updateData();


		ScaleFunction scaleFunction = identity;

		InterpolationMode interpolationMode = LinearInterpolation;

		virtual void getWaveformTableValues(int displayIndex, float const** tableValues, int& numValues, float& normalizeValue) = 0;

		virtual int getNumWaveformDisplays() const { return 1; }

	protected:

	private:

		Updater updater;
		Array<Component::SafePointer<WaveformComponent>> listeners;

		JUCE_DECLARE_WEAK_REFERENCEABLE(Broadcaster)
	};

	
	enum ColourIds
	{
		bgColour = 1024,
		fillColour,
		lineColour,
		numColourIds
	};

	WaveformComponent(Processor *p, int index=0);

	~WaveformComponent();

	void changeListenerCallback(SafeChangeBroadcaster* /*b*/) override
	{
		setBypassed(processor->isBypassed());
	}

	void setBypassed(bool shouldBeBypassed)
	{
		if (bypassed != shouldBeBypassed)
		{
			bypassed = shouldBeBypassed;
			rebuildPath();
		}
	}

	void setUseFlatDesign(bool shouldUseFlatDesign)
	{
		useFlatDesign = shouldUseFlatDesign;
		repaint();
	}

	void paint(Graphics &g);



	void resized() override
	{
		rebuildPath();
	}
	
	class Panel : public PanelWithProcessorConnection
	{
	public:

		SET_PANEL_NAME("Waveform");

		Panel(FloatingTile* parent) :
			PanelWithProcessorConnection(parent)
		{
			setDefaultPanelColour(FloatingTileContent::PanelColourId::bgColour, Colours::transparentBlack);
			setDefaultPanelColour(FloatingTileContent::PanelColourId::itemColour1, Colours::white);
			setDefaultPanelColour(FloatingTileContent::PanelColourId::itemColour2, Colours::white.withAlpha(0.5f));
		}

		Identifier getProcessorTypeId() const override;

		Component* createContentComponent(int /*index*/) override;

		void fillModuleList(StringArray& moduleList) override;
	};

protected:

	void setTableValues(const float* values, int numValues, float normalizeValue_);

	

private:

	bool bypassed = false;

	int index = 0;

	friend class Broadcaster;

	static Path getPathForBasicWaveform(WaveformType t);

	void rebuildPath();

	Path path;
	
	bool useFlatDesign = false;

	WeakReference<Processor> processor;

	

	float const *tableValues;

	int tableLength;

	float normalizeValue;

};

#if USE_BACKEND


class WavetablePreviewBase : public Component,
							 private SafeChangeListener
{
protected:

	WavetablePreviewBase(SampleMapToWavetableConverter& parent_) :
		parent(parent_)
	{
		parent.addChangeListener(this);
	};

	virtual ~WavetablePreviewBase()
	{
		parent.removeChangeListener(this);
	};

	virtual void updateGraphics() = 0;

	void changeListenerCallback(SafeChangeBroadcaster* /*b*/) override
	{
		updateGraphics();
	}

	SampleMapToWavetableConverter& parent;
};



class SampleMapToWavetableConverter::SampleMapPreview : public WavetablePreviewBase
{
public:

	SampleMapPreview(SampleMapToWavetableConverter& parent) :
		WavetablePreviewBase(parent)
	{};

	void updateGraphics() override;

	void paint(Graphics& g) override;

private:

	struct Sample
	{
		Sample(const ValueTree& data, Rectangle<int> totalArea);

		Rectangle<int> area;
		int index;
		bool analysed = false;
		bool active = false;
	};

	Array<Sample> samples;
};

class SampleMapToWavetableConverter::Preview : public WavetablePreviewBase
{
public:

	Preview(SampleMapToWavetableConverter& parent_);
	~Preview();

	void mouseMove(const MouseEvent& event) override;
	void mouseEnter(const MouseEvent& /*event*/) override;
	void mouseExit(const MouseEvent& /*event*/) override;
	void mouseDown(const MouseEvent& event) override;

	void updateGraphics() override;

	void paint(Graphics& g);

private:

	int getHoverIndex(int xPos) const;

	struct Harmonic
	{
		Rectangle<float> area;
		float lGain = 0.0f;
		float rGain = 0.0f;
	};

	void rebuildMap();

	int hoverIndex = 0;

	Array<Harmonic> harmonicList;
	

	Path p;
};


class CombinedPreview : public Component,
						public ButtonListener
{
public:

	CombinedPreview(SampleMapToWavetableConverter& parent)
	{
		setName("Preview");

		addAndMakeVisible(sampleMapButton = new TextButton("Sample Map"));
		addAndMakeVisible(spectrumButton = new TextButton("Spectrum"));
		addAndMakeVisible(sampleMap = new SampleMapToWavetableConverter::SampleMapPreview(parent));
		addAndMakeVisible(spectrum = new SampleMapToWavetableConverter::Preview(parent));

		sampleMapButton->addListener(this);
		spectrumButton->addListener(this);
	}

	void buttonClicked(Button* b) override
	{
		spectrum->setVisible(b != sampleMapButton);
		sampleMap->setVisible(b == sampleMapButton);
	}

	void resized() override
	{
		auto area = getLocalBounds();

		auto topBar = area.removeFromTop(22);

		spectrumButton->setBounds(topBar.removeFromLeft(getWidth() / 2).reduced(2));
		sampleMapButton->setBounds(topBar.reduced(2));

		spectrum->setBounds(area);
		sampleMap->setBounds(area);
	}

private:

	ScopedPointer<TextButton> sampleMapButton;
	ScopedPointer<TextButton> spectrumButton;
	ScopedPointer<SampleMapToWavetableConverter::Preview> spectrum;
	ScopedPointer<SampleMapToWavetableConverter::SampleMapPreview> sampleMap;
};

#endif

}

#endif