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

namespace scriptnode
{
using namespace juce;
using namespace hise;
using namespace snex;

namespace core
{
String snex_node::getEmptyText(const Identifier& id) const
{
	return WorkbenchData::getDefaultNodeTemplate(id);
}

bool snex_node::preprocess(String& code)
{
	SnexSource::preprocess(code);

	using namespace cppgen;

	Base c(Base::OutputType::AddTabs);

	int nc = getParentNode()->getNumChannelsToProcess();

	String def1, def2;

	def1 << "void dummyProcess(ProcessData<" << nc << ">& d)"; c << def1;
	{ StatementBlock body(c);
	    c << getCurrentClassId() + " instance;";
		c << "instance.process(d);";
	}

	def2 << "void dummyProcessFrame(span<float, " << nc << ">& d)"; c << def2;
	{ StatementBlock body(c);
	c << getCurrentClassId() + " instance;";
	c << "instance.processFrame(d);";
	}

	code << c.toString();

	DBG(code);

	return true;
}

}



}

