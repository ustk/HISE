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
*   Commercial licences for using HISE in an closed source project are
*   available on request. Please visit the project's website to get more
*   information about commercial licencing:
*
*   http://www.hartinstruments.net/hise/
*
*   HISE is based on the JUCE library,
*   which also must be licenced for commercial applications:
*
*   http://www.juce.com
*
*   ===========================================================================
*/

HardcodedScriptProcessor::HardcodedScriptProcessor(MainController *mc, const String &id, ModulatorSynth *ms):
	ScriptBaseMidiProcessor(mc, id),
	Message(this),
	Synth(this, ms),
	Engine(this),
	Console(this),
	Content(this),
	Sampler(this, dynamic_cast<ModulatorSampler*>(ms))
{
	
	content = &Content;

	jassert(ms != nullptr);

	allowObjectConstructors = true;
	
	onInit();

	allowObjectConstructors = false;

};

ProcessorEditorBody *HardcodedScriptProcessor::createEditor(ProcessorEditor *parentEditor)
{
#if USE_BACKEND

	return new HardcodedScriptEditor(parentEditor);

#else

	ignoreUnused(parentEditor);
	jassertfalse;

	return nullptr;

#endif
};

void HardcodedScriptProcessor::processHiseEvent(HiseEvent &m)
{
	currentEvent = &m;

	Message.setHiseEvent(m);

	Message.ignoreEvent(false);

	if(m.isNoteOn())
	{
		Synth.increaseNoteCounter();
		onNoteOn();
		
	}
	else if (m.isNoteOff())
	{
		Synth.decreaseNoteCounter();
		onNoteOff();
	}
	else if (m.isController())
	{
		onController();
	}
}

void HardcodedScriptFactoryType::fillTypeNameList()
{
	ADD_NAME_TO_TYPELIST(LegatoProcessor);
	ADD_NAME_TO_TYPELIST(CCSwapper);
	ADD_NAME_TO_TYPELIST(ReleaseTriggerScriptProcessor);
	ADD_NAME_TO_TYPELIST(CCToNoteProcessor);
	ADD_NAME_TO_TYPELIST(ChannelFilterScriptProcessor);
	ADD_NAME_TO_TYPELIST(ChannelSetterScriptProcessor);
	ADD_NAME_TO_TYPELIST(MuteAllScriptProcessor);
}

Processor *HardcodedScriptFactoryType::createProcessor(int typeIndex, const String &id) 
{
	MainController *m = getOwnerProcessor()->getMainController();
	ModulatorSynth *owner = dynamic_cast<ModulatorSynth*>(getOwnerProcessor());
	MidiProcessor *mp = nullptr;

	switch(typeIndex)
	{
	case legatoWithRetrigger:	mp = new LegatoProcessor(m, id, owner); break;
	case ccSwapper:				mp = new CCSwapper(m, id, owner); break;
	case releaseTrigger:		mp = new ReleaseTriggerScriptProcessor(m, id, owner); break;
	case cc2Note:				mp = new CCToNoteProcessor(m, id, owner); break;
	case channelFilter:			mp = new ChannelFilterScriptProcessor(m, id, owner); break;
	case channelSetter:			mp = new ChannelSetterScriptProcessor(m, id, owner); break;
	case muteAll:				mp = new MuteAllScriptProcessor(m, id, owner); break;
	default:					jassertfalse; break;
	}

	if(mp != nullptr)
	{
		mp->setOwnerSynth(owner);
	}

	return mp;
}

