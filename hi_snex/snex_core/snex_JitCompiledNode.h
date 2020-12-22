/*  ===========================================================================
*
*   This file is part of HISE.
*   Copyright 2016 Christoph Hart
*
*   HISE is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option any later version.
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

#pragma once

namespace snex {
namespace jit {
using namespace juce;


struct JitCompiledNode : public ReferenceCountedObject,
	public ExternalDataProviderBase
{
	int getNumRequiredDataObjects(ExternalData::DataType t) const override;

	void setExternalData(const ExternalData& b, int index) override;

	using CompilerInitFunction = std::function<void(Compiler& c, int numChannels)>;

	using Ptr = ReferenceCountedObjectPtr<JitCompiledNode>;

	static void defaultInitialiser(Compiler& c, int numChannels)
	{
		c.reset();
		SnexObjectDatabase::registerObjects(c, numChannels);
	}

	JitCompiledNode(Compiler& c, const String& code, const String& classId, int numChannels_, const CompilerInitFunction& cf = defaultInitialiser);

	static void addParameterMethod(String& s, const String& parameterName, int index)
	{
		s << "void set" << parameterName << "(double value) { instance.setParameter<" << String(index) << ">(value);}\n";
	}

	static void addCallbackWrapper(String& s, const FunctionData& d)
	{
		s << d.getSignature({}, false) << "{ ";

		if (d.returnType != TypeInfo(Types::ID::Void))
			s << "return ";

		s << "instance." << d.id.getIdentifier() << "(";

		for (int i = 0; i < d.args.size(); i++)
		{
			s << d.args[i].id.getIdentifier();
			if (isPositiveAndBelow(i, d.args.size() - 1))
				s << ", ";
		}

		s << "); }\n";
	}

	void prepare(PrepareSpecs ps)
	{
		jassert(ps.numChannels >= numChannels);

		lastSpecs = ps;

		callbacks[Types::ScriptnodeCallbacks::PrepareFunction].callVoid(&lastSpecs);
	}

	template <typename T> void process(T& data)
	{
		jassert(data.getNumChannels() >= numChannels);
		callbacks[Types::ScriptnodeCallbacks::ProcessFunction].callVoid(&data);
	}

	void reset()
	{
		callbacks[Types::ScriptnodeCallbacks::ResetFunction].callVoid();
	}

	void handleEvent(HiseEvent& e)
	{
		callbacks[Types::ScriptnodeCallbacks::HandleEventFunction].callVoid(&e);
	}

	template <typename T> void processFrame(T& data)
	{
		jassert(data.size() >= numChannels);
		callbacks[Types::ScriptnodeCallbacks::ProcessFrameFunction].callVoid(data.begin());
	}

	OpaqueSnexParameter::List getParameterList() const
	{
		return parameterList;
	}

	JitObject getJitObject() { return obj; }

	void* thisPtr = nullptr;

	Result r;

	int getNumChannels() const { return numChannels; }

private:

	int numRequiredDataTypes[(int)ExternalData::DataType::numDataTypes];

	PrepareSpecs lastSpecs;

	OpaqueSnexParameter::List parameterList;

	int numChannels = 0;
	bool ok = false;
	FunctionData callbacks[Types::ScriptnodeCallbacks::numFunctions];

	FunctionData setExternalDataFunction;

	JitObject obj;
	ComplexType::Ptr instanceType;
};


}
}