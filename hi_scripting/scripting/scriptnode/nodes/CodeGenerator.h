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
 *   which also must be licenced for commercial applications:
 *
 *   http://www.juce.com
 *
 *   ===========================================================================
 */

#pragma once

namespace scriptnode
{
using namespace juce;
using namespace hise;


struct CppGen
{
	struct Accessor
	{
		enum class Format
		{
			ParameterDefinition,
			GetMethod,
			ID
		};

		String id;
		Array<int> path;

		String toString(Format f) const
		{
			String s;

			switch (f)
			{
			case Format::ID: return "\"" + id + "\"";
			case Format::GetMethod:
			{
				s << "FIND_NODE(obj, ";

				for (int i = 0; i < path.size(); i++)
				{
					s << path[i];

					if (i != path.size() - 1)
						s << ", ";
				}

				s << ")";
				return s;
			}
			case Format::ParameterDefinition:
			{

				

				s << "registerNode(";
				s << toString(Format::GetMethod) << ", ";
				s << toString(Format::ID);
				s << ");\n";
				return s;
			}
			}

			return s;
		}
	};

	enum class CodeLocation
	{
		InnerJitClasses,
		TemplateAlias,

		Definitions,
		PrepareBody,
		ProcessBody,
		ProcessSingleBody,
		PrivateMembers,
		HandleModulationBody,
		CreateParameters,
		numCodeLocation
	};

	struct MethodInfo
	{
		String returnType;
		String name;
		String specifiers;
		StringArray arguments;
		String body;
		bool addSemicolon = false;
		bool addNewLine = true;
	};

	struct Emitter
	{
		static String addNodeTemplateWrappers(String className, NodeBase* n);

		static void emitDefinition(String& s, const String& definition, const String& value, bool useQuotes = true);

		static void emitConstexprVariable(String& s, const String& variableName, int value);
		static void emitArgumentList(String& s, const StringArray& args);
		static void emitFunctionDefinition(String& s, const MethodInfo& f);
		static void emitCommentLine(String& code, int tabLevel, const String& comment);
		static void emitClassEnd(String& s);
		static String createLine(String& content);
		static String createDefinition(String& name, String& assignment);

		static String createFactoryMacro(bool shouldCreatePoly, bool isSnexClass);

		static String createRangeString(NormalisableRange<double> range);

		static String surroundWithBrackets(const String& s);

		static String createClass(const String& content, const String& templateId, bool createPolyphonicClass);

		static String createJitClass(const String& className, const String& content);

		static String createPrettyNumber(double value, bool createFloat);

		static String getVarAsCode(const var& v);

		static String createAlias(const String& aliasName, const String& className);
		static String createTemplateAlias(const String& aliasName, const String& className, const StringArray& templateArguments);
		static String wrapIntoTemplate(const String& className, const String& outerTemplate);
		static String prependNamespaces(const String& className, const Array<Identifier>& namespaces);
		static String wrapIntoNamespace(const String& s, const String& namespaceId);
	};

	struct Helpers
	{
		static String createIntendation(const String& code);	
	};

};




}