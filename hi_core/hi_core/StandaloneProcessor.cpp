namespace hise { using namespace juce;

AudioDeviceDialog::AudioDeviceDialog(AudioProcessorDriver *ownerProcessor_) :
ownerProcessor(ownerProcessor_)
{
	setName("Audio Settings");

	setOpaque(false);

	selector = new AudioDeviceSelectorComponent(*ownerProcessor->deviceManager, 0, 0, 2, 2, true, false, true, false);

	setLookAndFeel(&alaf);

	selector->setLookAndFeel(&alaf);

	addAndMakeVisible(cancelButton = new TextButton("Cancel"));
	addAndMakeVisible(applyAndCloseButton = new TextButton("Apply changes & close window"));

	cancelButton->addListener(this);
	applyAndCloseButton->addListener(this);

	addAndMakeVisible(selector);
}


void AudioDeviceDialog::buttonClicked(Button *b)
{
	if (b == applyAndCloseButton)
	{
		ownerProcessor->saveDeviceSettingsAsXml();
		auto deviceData = ownerProcessor->deviceManager->createStateXml();
		ownerProcessor->initialiseAudioDriver(deviceData.get());
	}

#if USE_BACKEND
	findParentComponentOfClass<FloatingTilePopup>()->deleteAndClose();
#endif
}

AudioProcessorDriver::AudioProcessorDriver(AudioDeviceManager* manager, AudioProcessorPlayer* callback_):
	GlobalSettingManager(),
	callback(callback_),
	deviceManager(manager)
{}

AudioProcessorDriver::~AudioProcessorDriver()
{
	saveDeviceSettingsAsXml();
        

	deviceManager = nullptr;
	callback = nullptr;
}

double AudioProcessorDriver::getCurrentSampleRate()
{
	return callback->getCurrentProcessor()->getSampleRate();
}

int AudioProcessorDriver::getCurrentBlockSize()
{
	return callback->getCurrentProcessor()->getBlockSize();
}

void AudioProcessorDriver::setCurrentSampleRate(double newSampleRate)
{
	// Before changing sample rate, clear both input and output channels to prevent
	// ASIO drivers from hanging when they reconfigure for double/quad-speed modes
	// (88.2k+), which reduce the available channel count. If previously selected
	// channels are beyond the new channel count, the driver will hang.
	auto savedInputChannel = activeInputChannel;

	AudioDeviceManager::AudioDeviceSetup currentSetup;
	deviceManager->getAudioDeviceSetup(currentSetup);

	auto savedOutputChannels = currentSetup.outputChannels;

	if (activeInputChannel >= 0)
		setInputChannel(-1);

	// Clear output channels and set the new sample rate
	deviceManager->getAudioDeviceSetup(currentSetup);
	currentSetup.sampleRate = newSampleRate;
	currentSetup.outputChannels.clear();
	deviceManager->setAudioDeviceSetup(currentSetup, true);

	// Force a full device close/reopen cycle so that the ASIO driver re-enumerates
	// its channel names. Without this, JUCE's cached channel name arrays remain stale
	// after a sample rate change that alters the channel count (e.g. RME double-speed mode).
	deviceManager->closeAudioDevice();
	deviceManager->restartLastAudioDevice();

	// Restore output channels, clamping to the new channel count
	auto* device = deviceManager->getCurrentAudioDevice();

	if (device != nullptr)
	{
		auto numOutputs = device->getOutputChannelNames().size();

		deviceManager->getAudioDeviceSetup(currentSetup);

		if (savedOutputChannels.getHighestBit() < numOutputs)
		{
			currentSetup.outputChannels = savedOutputChannels;
		}
		else
		{
			// Previously selected outputs are out of range, fall back to first stereo pair
			currentSetup.outputChannels.clear();

			for (int i = 0; i < jmin((int)HISE_NUM_STANDALONE_OUTPUTS, numOutputs); i++)
				currentSetup.outputChannels.setBit(i, true);
		}

		currentSetup.useDefaultOutputChannels = false;
		deviceManager->setAudioDeviceSetup(currentSetup, true);
	}

	if (savedInputChannel >= 0 && device != nullptr)
	{
		auto numInputs = device->getInputChannelNames().size();

		if (savedInputChannel < numInputs)
			setInputChannel(savedInputChannel);
	}
}

void AudioProcessorDriver::setCurrentBlockSize(int newBlockSize)
{
	AudioDeviceManager::AudioDeviceSetup currentSetup;

	deviceManager->getAudioDeviceSetup(currentSetup);
	currentSetup.bufferSize = newBlockSize;
	deviceManager->setAudioDeviceSetup(currentSetup, true);
}

void AudioProcessorDriver::setOutputChannelName(const int channelIndex)
{
	AudioDeviceManager::AudioDeviceSetup currentSetup;

	deviceManager->getAudioDeviceSetup(currentSetup);
		
	BigInteger thisChannels = 0;
	thisChannels.setBit(channelIndex);
	currentSetup.outputChannels = thisChannels;

	deviceManager->setAudioDeviceSetup(currentSetup, true);
}

void AudioProcessorDriver::setAudioDevice(const String& deviceName)
{
	AudioDeviceManager::AudioDeviceSetup currentSetup;

	deviceManager->getAudioDeviceSetup(currentSetup);
	currentSetup.outputDeviceName = deviceName;
	deviceManager->setAudioDeviceSetup(currentSetup, true);
}

void AudioProcessorDriver::setInputChannel(int monoChannelIndex)
{
	activeInputChannel = monoChannelIndex;

	AudioDeviceManager::AudioDeviceSetup config;
	deviceManager->getAudioDeviceSetup(config);

	config.inputChannels.clear();

	if (monoChannelIndex >= 0)
	{
		config.inputChannels.setBit(monoChannelIndex, true);

		// For non-ASIO drivers (DirectSound, WASAPI), the input device name
		// must be set explicitly. Use the same device as the output.
		if (config.inputDeviceName.isEmpty())
			config.inputDeviceName = config.outputDeviceName;
	}

	config.useDefaultInputChannels = false;
	deviceManager->setAudioDeviceSetup(config, true);
}

int AudioProcessorDriver::getActiveInputChannel() const
{
	return activeInputChannel;
}

void AudioProcessorDriver::toggleMidiInput(const String& midiInputName, bool enableInput)
{
	if (midiInputName.isNotEmpty())
	{
		deviceManager->setMidiInputEnabled(midiInputName, enableInput);
	}
}

File AudioProcessorDriver::getDeviceSettingsFile()
{

	File parent = getSettingDirectory();

	File savedDeviceData = parent.getChildFile("DeviceSettings.xml");

	return savedDeviceData;
}


void AudioProcessorDriver::restoreSettings(MainController* /*mc*/)
{
	
}

void AudioProcessorDriver::saveDeviceSettingsAsXml()
{
    
	std::unique_ptr<XmlElement> deviceData = deviceManager != nullptr ?
                                           deviceManager->createStateXml():
                                           nullptr;

	if (deviceData != nullptr)
	{
		deviceData->writeToFile(getDeviceSettingsFile(), "");

		
	}
}

void AudioProcessorDriver::setAudioDeviceType(const String deviceName)
{
	// Before switching device types, clear the input channels on the current device
	// so that JUCE doesn't carry over numInputChansNeeded to the new device type.
	// This prevents ASIO drivers from hanging when they're opened with input channels
	// enabled during a device type switch.
	auto savedInputChannel = activeInputChannel;

	if (activeInputChannel >= 0)
		setInputChannel(-1);

	deviceManager->setCurrentAudioDeviceType(deviceName, true);

	// Re-apply the input channel on the new device type if one was previously selected
	if (savedInputChannel >= 0 && deviceManager->getCurrentAudioDevice() != nullptr)
		setInputChannel(savedInputChannel);
}

void AudioProcessorDriver::resetToDefault()
{
	auto prevState = getMidiInputState();
	auto names = MidiInput::getDevices();

	deviceManager->initialiseWithDefaultDevices(0, 2);
	activeInputChannel = -1;

	for (int i = 0; i < prevState.getHighestBit() + 1; i++)
	{
		deviceManager->setMidiInputEnabled(names[i], prevState[i]);
	}
}

String GlobalSettingManager::getHiseVersion()
{
#if USE_BACKEND
	return PresetHandler::getVersionString();
#else
	return FrontendHandler::getHiseVersion();
#endif
}

void GlobalSettingManager::setDiskMode(int mode)
{
	diskMode = mode;

	if (MainController* mc = dynamic_cast<MainController*>(this))
	{
		mc->getSampleManager().setDiskMode((MainController::SampleManager::DiskMode)mode);
	}
}

AudioDeviceDialog::~AudioDeviceDialog()
{

}


StandaloneProcessor::StandaloneProcessor()
{
	LOG_START("Creating Device Manager");

	deviceManager = new AudioDeviceManager();
	callback = new AudioProcessorPlayer();

#if HISE_IOS
    if(!HiseDeviceSimulator::isAUv3())
    {
        const String portName = FrontendHandler::getProjectName() + " Virtual MIDI";
        
        if(virtualMidiPort = MidiInput::createNewDevice(portName, callback).release())
        {
            virtualMidiPort->start();
        }
    }
#endif
    




    
	LOG_START("Create Main Processor");



	wrappedProcessor = createProcessor();


	ScopedPointer<XmlElement> xml = AudioProcessorDriver::getSettings();

#if USE_BACKEND
	if(!CompileExporter::shouldSkipAudioDriverInitialisation())
	{
		if(xml != nullptr)
		{
			BigInteger numOutputChannels;

			numOutputChannels.parseString(xml->getStringAttribute("audioDeviceOutChans"), 2);

			auto numOutputsInDeviceSetting = numOutputChannels.countNumberOfSetBits();

			if(numOutputsInDeviceSetting != HISE_NUM_STANDALONE_OUTPUTS)
			{
				if(PresetHandler::showYesNoWindow("Channel amount mismatch", "The number of channels used in the audio device settings do not match the amount of channels defined by `HISE_NUM_STANDALONE_OUTPUTS`.  \nPress OK to remove the xml file and initialise the default value."))
				{
					AudioProcessorDriver::getDeviceSettingsFile().deleteFile();
					xml = nullptr;
				}
			}
		}

		dynamic_cast<AudioProcessorDriver*>(wrappedProcessor.get())->initialiseAudioDriver(xml);
	}
		
#else
	
    auto apd = dynamic_cast<AudioProcessorDriver*>(wrappedProcessor.get());
    
    jassert(apd != nullptr);
    
	LOG_START("Initialise Audio Driver...");

    apd->initialiseAudioDriver(xml);
    
	LOG_START("OK");

#endif
	
}

StandaloneProcessor::~StandaloneProcessor()
{
		
	deviceManager->removeAudioCallback(callback);
	deviceManager->removeMidiInputCallback(String(), callback);
	deviceManager->closeAudioDevice();
        
	callback = nullptr;
	wrappedProcessor = nullptr;
	deviceManager = nullptr;
}

AudioProcessorEditor* StandaloneProcessor::createEditor()
{
	return wrappedProcessor->createEditor();
}

float StandaloneProcessor::getScaleFactor() const
{ 
#if USE_BACKEND
	return 1.0;
#else
		return scaleFactor; 
#endif
}

AudioProcessor* StandaloneProcessor::getCurrentProcessor()
{ return wrappedProcessor.get(); }

const AudioProcessor* StandaloneProcessor::getCurrentProcessor() const
{ return wrappedProcessor.get(); }


void StandaloneProcessor::requestQuit()
{
	auto mc = dynamic_cast<MainController*>(wrappedProcessor.get());
	mc->getKillStateHandler().requestQuit();
}

XmlElement * AudioProcessorDriver::getSettings()
{
	File savedDeviceData = getDeviceSettingsFile();

	return XmlDocument::parse(savedDeviceData).release();
}

void AudioProcessorDriver::initialiseAudioDriver(XmlElement *deviceData)
{
    jassert(deviceManager != nullptr);
    
	DebugLogger& logger = dynamic_cast<MainController*>(this)->getDebugLogger();

	// Use 0 input channels for initialization so that switching audio device types
	// (especially ASIO) doesn't automatically try to configure input channels,
	// which can cause hangs on some ASIO drivers. Input channels from saved XML
	// settings are still restored by JUCE. Active input is applied after init
	// via setInputChannel() when activeInputChannel >= 0.
	const int numInputChannels = 0;

	if (deviceData != nullptr && deviceData->hasTagName("DEVICESETUP"))
	{
		String errorMessage = deviceManager->initialise(numInputChannels, HISE_NUM_STANDALONE_OUTPUTS, deviceData, true);

		if (errorMessage.isNotEmpty() || deviceManager->getCurrentAudioDevice() == nullptr)
		{
			logger.logMessage("Error initialising with stored settings: " + errorMessage);

			logger.logMessage("Audio Driver Default Initialisation");

			const String error = deviceManager->initialiseWithDefaultDevices(numInputChannels, HISE_NUM_STANDALONE_OUTPUTS);

			if (error.isNotEmpty())
				logger.logMessage("Error initialising with default settings: " + error);
		}
	}
	else
	{
		logger.logMessage("Audio Driver Default Initialisation");

		const String error = deviceManager->initialiseWithDefaultDevices(numInputChannels, HISE_NUM_STANDALONE_OUTPUTS);

		if (error.isNotEmpty())
			logger.logMessage("Error initialising with default settings: " + error);
	}

	callback->setProcessor(dynamic_cast<AudioProcessor*>(this));

	deviceManager->addAudioCallback(callback);
	deviceManager->addMidiInputCallback(String(), callback);

	// Restore activeInputChannel from the device setup that was just loaded from XML
	{
		AudioDeviceManager::AudioDeviceSetup config;
		deviceManager->getAudioDeviceSetup(config);

		if (config.inputChannels.isZero())
			activeInputChannel = -1;
		else
			activeInputChannel = config.inputChannels.findNextSetBit(0);
	}

	getSettingsObject().initialiseAudioDriverData();

	// Apply saved MIDI input settings to ensure MIDI inputs are properly connected
	// This fixes the issue where MIDI keyboards appear selected but don't work until manually toggled
	auto midiInputSetting = getSettingsObject().getSetting(HiseSettings::Midi::MidiInput);
	if (midiInputSetting.isInt64())
	{
		auto state = BigInteger((int64)midiInputSetting);
		auto mc = dynamic_cast<MainController*>(this);

		StringArray midiNames;
		if (mc != nullptr && !mc->isFlakyThreadingAllowed())
		{
			midiNames = MidiInput::getDevices();
		}

		if (midiNames.size() > 0)
		{
			for (int i = 0; i < midiNames.size(); i++)
			{
				toggleMidiInput(midiNames[i], state[i]);
			}
		}
	}
}

void GlobalSettingManager::setGlobalScaleFactor(double newScaleFactor, NotificationType notifyListeners/*=dontSendNotification*/)
{
	if (scaleFactor != newScaleFactor)
	{
		scaleFactor = newScaleFactor;

		if (notifyListeners != dontSendNotification)
		{
			WeakReference<GlobalSettingManager> safeThis(this);

			auto f = [safeThis, newScaleFactor]()
			{
				if (safeThis.get() != nullptr)
				{
					for (int i = 0; i < safeThis->listeners.size(); i++)
					{
						if (safeThis->listeners[i].get() != nullptr)
							safeThis->listeners[i]->scaleFactorChanged((float)newScaleFactor);
					}
				}
			};

			if (notifyListeners == sendNotificationSync)
				f();
			else
				MessageManager::callAsync(f);
		}
	}
}

void AudioProcessorDriver::updateMidiToggleList(MainController* mc, ToggleButtonList* listToUpdate)
{
#if USE_BACKEND || IS_STANDALONE_APP

	auto state = dynamic_cast<AudioProcessorDriver*>(mc)->getMidiInputState();

	int numBits = state.getHighestBit()+1;

	for (int i = 0; i < numBits; i++)
	{
		listToUpdate->setValue(i, state[i], dontSendNotification);
	}

#else
	ignoreUnused(mc, listToUpdate);
#endif
}


juce::BigInteger AudioProcessorDriver::getMidiInputState() const
{
	if (deviceManager == nullptr)
		return 0;

	BigInteger state = 0;

	

    StringArray midiInputs;
    
    if(auto mc = dynamic_cast<const MainController*>(this))
    {
        if(!mc->isFlakyThreadingAllowed())
            midiInputs = MidiInput::getDevices();
    }
        

	for (int i = 0; i < midiInputs.size(); i++)
	{
		if (deviceManager->isMidiInputEnabled(midiInputs[i]))
			state.setBit(i, true);
	}


	return state;
}

GlobalSettingManager::ScaleFactorListener::~ScaleFactorListener()
{
	masterReference.clear();
}

GlobalSettingManager::~GlobalSettingManager()
{
	saveSettingsAsXml();
}

File GlobalSettingManager::getGlobalSettingsFile()
{
	return getSettingDirectory().getChildFile("GeneralSettings.xml");
}

void GlobalSettingManager::storeAllSamplesFound(bool areFound) noexcept
{
	allSamplesFound = areFound;
}

void GlobalSettingManager::setVoiceAmountMultiplier(int newVoiceAmountMultiplier)
{
	voiceAmountMultiplier = newVoiceAmountMultiplier;
}

void GlobalSettingManager::setEnabledMidiChannels(int newMidiChannelNumber)
{
	channelData = newMidiChannelNumber;
}

int GlobalSettingManager::getChannelData() const
{ return channelData; }

float GlobalSettingManager::getGlobalScaleFactor() const noexcept
{ return (float)scaleFactor; }

void GlobalSettingManager::addScaleFactorListener(ScaleFactorListener* newListener)
{
	listeners.addIfNotAlreadyThere(newListener);
}

void GlobalSettingManager::removeScaleFactorListener(ScaleFactorListener* listenerToRemove)
{
	listeners.removeAllInstancesOf(listenerToRemove);
}

HiseSettings::Data& GlobalSettingManager::getSettingsObject()
{ 
	jassert(dataObject != nullptr);
	return *dataObject;
}

const HiseSettings::Data& GlobalSettingManager::getSettingsObject() const
{
	jassert(dataObject != nullptr);
	return *dataObject;
}

HiseSettings::Data* GlobalSettingManager::getSettingsAsPtr()
{
	return dataObject.get();
}

void GlobalSettingManager::initData(MainController* mc)
{
	dataObject = new HiseSettings::Data(mc);
}

GlobalSettingManager::GlobalSettingManager()
{
	ScopedPointer<XmlElement> xml = AudioProcessorDriver::getSettings();

	if (xml != nullptr)
	{
		scaleFactor = (float)xml->getDoubleAttribute("SCALE_FACTOR", 1.0);

#if HISE_USE_OPENGL_FOR_PLUGIN
		bool dv = (bool)HISE_DEFAULT_OPENGL_VALUE;
#else
		bool dv = false;
#endif

		useOpenGL = (bool)xml->getBoolAttribute("OPEN_GL", dv);
	}
}

void GlobalSettingManager::restoreGlobalSettings(MainController* mc, bool checkReferences)
{
	LOG_START("Restoring global settings");

	File savedDeviceData = getGlobalSettingsFile();

	auto globalSettings = XmlDocument::parse(savedDeviceData);

	if (globalSettings != nullptr)
	{
		GlobalSettingManager* gm = dynamic_cast<GlobalSettingManager*>(mc);

		gm->diskMode = globalSettings->getIntAttribute("DISK_MODE");
		gm->scaleFactor = globalSettings->getDoubleAttribute("SCALE_FACTOR", 1.0);

#if IS_STANDALONE_APP
		// Don't save this for plugins as they are usually synced to the host
		gm->globalBPM = globalSettings->getIntAttribute("GLOBAL_BPM", -1);
#endif

		gm->channelData = globalSettings->getIntAttribute("MIDI_CHANNELS", 1);

		gm->voiceAmountMultiplier = globalSettings->getIntAttribute("VOICE_AMOUNT_MULTIPLIER", 2);

#if HISE_USE_OPENGL_FOR_PLUGIN
		bool dv = (bool)HISE_DEFAULT_OPENGL_VALUE;
#else
		bool dv = false;
#endif

		gm->useOpenGL = globalSettings->getBoolAttribute("OPEN_GL", dv);

		LOG_START("Setting disk mode");

		mc->getSampleManager().setDiskMode((MainController::SampleManager::DiskMode)gm->diskMode);
		mc->getMainSynthChain()->getActiveChannelData()->restoreFromData(gm->channelData);

#if USE_FRONTEND

		if (checkReferences)
		{
			bool allSamplesThere = globalSettings->getBoolAttribute("SAMPLES_FOUND");

			auto& handler = mc->getSampleManager().getProjectHandler();

			if (!allSamplesThere)
			{
				LOG_START("Samples not validated. Checking references");
				handler.checkAllSampleReferences();
			}
			else
			{
				LOG_START("Samples are validated. Skipping reference check");
				handler.setAllSampleReferencesCorrect();
			}
		}
#else
		ignoreUnused(mc);
#endif
	}
}

void GlobalSettingManager::saveSettingsAsXml()
{
	ScopedPointer<XmlElement> settings = new XmlElement("GLOBAL_SETTINGS");

	settings->setAttribute("DISK_MODE", diskMode);
	settings->setAttribute("SCALE_FACTOR", scaleFactor);
	settings->setAttribute("VOICE_AMOUNT_MULTIPLIER", voiceAmountMultiplier);

#if IS_STANDALONE_APP
	// Dont save this in plugins
	settings->setAttribute("GLOBAL_BPM", globalBPM);
#endif

	settings->setAttribute("MIDI_CHANNELS", channelData);

#if USE_FRONTEND
	settings->setAttribute("SAMPLES_FOUND", allSamplesFound);
#endif

	settings->setAttribute("OPEN_GL", useOpenGL);

	settings->writeToFile(getGlobalSettingsFile(), "");

}

File GlobalSettingManager::getSettingDirectory()
{


#if ENABLE_APPLE_SANDBOX
	return NativeFileHandler::getAppDataDirectory().getChildFile("Resources/");
#else
	return NativeFileHandler::getAppDataDirectory(nullptr);
#endif

}

} // namespace hise
