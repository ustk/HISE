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

namespace hise
{
#if !USE_BACKEND
	struct FrontendHostFactory;
#endif
}

namespace scriptnode
{
using namespace juce;
using namespace hise;



class NodeFactory;

struct DeprecationChecker
{
	enum class DeprecationId
	{
		OK,
		OpTypeNonSet,
		ConverterNotIdentity,
		numDeprecationIds
	};

	DeprecationChecker(DspNetwork* n_, ValueTree v_);

	static String getErrorMessage(int id);

	DspNetwork* n;
	ValueTree v;
	bool notOk = false;

	void throwIf(DeprecationId id);

	bool check(DeprecationId id);
};



class ScriptnodeExceptionHandler
{
	struct Item
	{
		bool operator==(const Item& other) const
		{
			return node.get() == other.node.get();
		}

		String toString() const
		{
			if (node == nullptr || error.error == Error::OK)
				return {};
			else
			{
				String s;
				s << node->getCurrentId() << " - " << getErrorMessage(error);
				return s;
			}
		}

		WeakReference<NodeBase> node;
		Error error;
		
	};

public:

	bool isOk() const noexcept
	{
		return items.isEmpty();
	}

	void addError(NodeBase* n, Error e)
	{
		for (auto& i : items)
		{
			if (i.node == n)
			{
				i.error = e;
				return;
			}
				
		}

		items.add({ n, e });
	}

	void removeError(NodeBase* n, Error::ErrorCode errorToRemove=Error::numErrorCodes)
	{
		for (int i = 0; i < items.size(); i++)
		{
			auto e = items[i].error.error;

			auto isErrorCode = e == errorToRemove ||
				(errorToRemove == Error::numErrorCodes && e != Error::ErrorCode::DeprecatedNode);

			if ((n == nullptr || (items[i].node == n)) && isErrorCode)
				items.remove(i--);
		}
	}

	static String getErrorMessage(Error e)
	{
		String s;

		s << "**";

		switch (e.error)
		{
		case Error::ChannelMismatch: s << "Channel amount mismatch";  break;
		case Error::BlockSizeMismatch: s << "Blocksize mismatch"; break;
		case Error::IllegalFrameCall: s << "Can't be used in frame processing context"; return s;
		case Error::IllegalBlockSize: s << "Illegal block size: " << String(e.actual); return s;
		case Error::SampleRateMismatch: s << "Samplerate mismatch"; break;
		case Error::InitialisationError: return "Initialisation error";
		case Error::TooManyChildNodes: s << "Number of child nodes (" << e.actual << ") exceed channels (" << e.expected << ")."; return s;
		case Error::NoMatchingParent:	 return "Can't find suitable parent node";
		case Error::RingBufferMultipleWriters: return "Buffer used multiple times";
		case Error::NodeDebuggerEnabled: return "Node is being debugged";
		case Error::DeprecatedNode:		 return DeprecationChecker::getErrorMessage(e.actual);
		case Error::IllegalPolyphony: return "Can't use this node in a polyphonic network";
		case Error::CompileFail:	s << "Compilation error** at Line " << e.expected << ", Column " << e.actual; return s;
		default:
			break;
		}

		s << "**:  \n`" << String(e.actual) << "` (expected: `" << String(e.expected) << "`)";

		return s;
	}

	String getErrorMessage(NodeBase* n) const
	{
		for (auto& i : items)
		{
			if (i.node == n)
				return i.toString();
		}

		return {};
	}

private:


	Array<Item> items;
};


class SnexSource;




/** A network of multiple DSP objects that are connected using a graph. */
class DspNetwork : public ConstScriptingObject,
				   public Timer,
				   public ControlledObject,
				   public NodeBase::Holder
{
public:

	struct AnonymousNodeCloner
	{
		AnonymousNodeCloner(DspNetwork& p, NodeBase::Holder* other);

		~AnonymousNodeCloner();

		NodeBase::Ptr clone(NodeBase::Ptr p);

		DspNetwork& parent;
		WeakReference<NodeBase::Holder> prevHolder;
	};

	struct VoiceSetter
	{
		VoiceSetter(DspNetwork& p, int newVoiceIndex):
			internalSetter(*p.getPolyHandler(), newVoiceIndex)
		{}

	private:

		const snex::Types::PolyHandler::ScopedVoiceSetter internalSetter;
	};

	struct NoVoiceSetter
	{
		NoVoiceSetter(DspNetwork& p):
			internalSetter(*p.getPolyHandler())
		{}

	private:

		const snex::Types::PolyHandler::ScopedAllVoiceSetter internalSetter;
	};

	class Holder
	{
	public:

		Holder();

		virtual ~Holder() {};

		DspNetwork* getOrCreate(const ValueTree& v);

		DspNetwork* getOrCreate(const String& id);
		StringArray getIdList();
		void saveNetworks(ValueTree& d) const;
		void restoreNetworks(const ValueTree& d);

		virtual bool isPolyphonic() const { return false; };

		void clearAllNetworks()
		{
			ReferenceCountedArray<DspNetwork> oldNetworks;

			{
				SimpleReadWriteLock::ScopedWriteLock l(getNetworkLock());
				std::swap(networks, oldNetworks);
				networks.clear();
				activeNetwork = nullptr;
			}
		}

		void setActiveNetwork(DspNetwork* n)
		{
			SimpleReadWriteLock::ScopedWriteLock l(getNetworkLock());
			activeNetwork = n;
		}

		ScriptParameterHandler* getCurrentNetworkParameterHandler(const ScriptParameterHandler* contentHandler) const
		{
			if (auto n = getActiveNetwork())
			{
				if (n->isForwardingControlsToParameters())
				{
					if(n->projectNodeHolder.isActive())
						return const_cast<ScriptParameterHandler*>(static_cast<const ScriptParameterHandler*>(&n->projectNodeHolder));
					else
						return const_cast<ScriptParameterHandler*>(static_cast<const ScriptParameterHandler*>(&n->networkParameterHandler));
					
				}
			}

			return const_cast<ScriptParameterHandler*>(contentHandler);
		}

		DspNetwork* getActiveNetwork() const
		{
			return activeNetwork.get();
		}

		void setProjectDll(dll::ProjectDll::Ptr pdll)
		{
			projectDll = pdll;
		}

		dll::ProjectDll::Ptr projectDll;

		ExternalDataHolder* getExternalDataHolder()
		{
			return dataHolder;
		}

		void setExternalDataHolderToUse(ExternalDataHolder* newHolder)
		{
			dataHolder = newHolder;
		}

		void setVoiceKillerToUse(snex::Types::VoiceResetter* vk_)
		{
			if (isPolyphonic())
				vk = vk_;
		}

		SimpleReadWriteLock& getNetworkLock() { return connectLock; }

		DspNetwork* addEmbeddedNetwork(DspNetwork* parent, const ValueTree& v, ExternalDataHolder* holderToUse)
		{
			auto n = new DspNetwork(parent->getScriptProcessor(), v, parent->isPolyphonic(), holderToUse);
			embeddedNetworks.add(n);
			n->setParentNetwork(parent);
			return n;
		}

	protected:

		ReferenceCountedArray<DspNetwork> embeddedNetworks;

		SimpleReadWriteLock connectLock;

		WeakReference<snex::Types::VoiceResetter> vk;

		ExternalDataHolder* dataHolder = nullptr;

		WeakReference<DspNetwork> activeNetwork;

		ReferenceCountedArray<DspNetwork> networks;

		JUCE_DECLARE_WEAK_REFERENCEABLE(Holder);
	};

	DspNetwork(ProcessorWithScriptingContent* p, ValueTree data, bool isPolyphonic, ExternalDataHolder* dataHolder=nullptr);
	~DspNetwork();

#if HISE_INCLUDE_SNEX
	struct CodeManager
	{
		CodeManager(DspNetwork& p):
			parent(p)
		{
			
		}

		struct SnexSourceCompileHandler : public snex::ui::WorkbenchData::CompileHandler,
										  public ControlledObject,
									      public Thread
		{
			using TestBase = snex::ui::WorkbenchData::TestRunnerBase;
			
			struct SnexCompileListener
			{
				virtual ~SnexCompileListener() {};

				/** This is called directly after the compilation (if it was OK) and can 
				    be used to run the test and update the UI display. */
				virtual void postCompileSync() = 0;

				JUCE_DECLARE_WEAK_REFERENCEABLE(SnexCompileListener);
			};

			SnexSourceCompileHandler(snex::ui::WorkbenchData* d, ProcessorWithScriptingContent* sp_);;

			void processTestParameterEvent(int parameterIndex, double value) final override {};
			void prepareTest(PrepareSpecs ps, const Array<snex::ui::WorkbenchData::TestData::ParameterEvent>& initialParameters) final override {};
			void processTest(ProcessDataDyn& data) final override {};

			void run() override;

			void postCompile(ui::WorkbenchData::CompileResult& lastResult) override
			{
				auto currentThread = Thread::getCurrentThread();

				if (currentThread != this)
				{
					runTestNext.store(true);
					startThread();
					return;
				}

				if (lastResult.compiledOk() && test != nullptr && getParent()->getGlobalScope().isDebugModeEnabled())
				{
					getParent()->getGlobalScope().getBreakpointHandler().setExecutingThread(currentThread);
					lastResult.compileResult = test->runTest(lastResult);
					getParent()->getGlobalScope().getBreakpointHandler().setExecutingThread(nullptr);
				}

				runTestNext.store(false);
			}

			bool triggerCompilation() override;
			
			/** This returns the mutex used for synchronising the compilation. 
			
				Any access to a JIT compiled function must be locked using a read lock.

				The write lock will be held for a short period before compiling (where you need to reset 
				the state) and then after the compilation where you need to setup the object.
			*/
			SimpleReadWriteLock& getCompileLock() { return compileLock; }

			void addCompileListener(SnexCompileListener* l)
			{
				compileListeners.addIfNotAlreadyThere(l);
			}

			void removeCompileListener(SnexCompileListener* l)
			{
				compileListeners.removeAllInstancesOf(l);
			}

			void setTestBase(TestBase* ownedTest)
			{
				test = ownedTest;
			}

		private:

			std::atomic<bool> runTestNext = false;

			ScopedPointer<TestBase> test;

			ProcessorWithScriptingContent* sp;

			hise::SimpleReadWriteLock compileLock;

			Array<WeakReference<SnexCompileListener>> compileListeners;
		};

		snex::ui::WorkbenchData::Ptr getOrCreate(const Identifier& typeId, const Identifier& classId)
		{
			using namespace snex::ui;

			for (auto e : entries)
			{
				if (e->wb->getInstanceId() == classId && e->type == typeId)
					return e->wb;
			}

			auto targetFile = getCodeFolder().getChildFile(typeId.toString()).getChildFile(classId.toString()).withFileExtension("h");
			entries.add(new Entry(typeId, targetFile, parent.getScriptProcessor()));
			return entries.getLast()->wb;
		}

		ValueTree getParameterTree(const Identifier& typeId, const Identifier& classId)
		{
			for (auto e : entries)
			{
				if (e->type == typeId && e->wb->getInstanceId() == classId)
					return e->parameterTree;
			}

			jassertfalse;
			return {};
		}
		
		StringArray getClassList(const Identifier& id)
		{
			auto f = getCodeFolder();

			if (id.isValid())
				f = f.getChildFile(id.toString());

			StringArray sa;

			for (auto& l : f.findChildFiles(File::findFiles, true, "*.h"))
			{
				sa.add(l.getFileNameWithoutExtension());
			}

			return sa;
		}

	private:

		struct Entry
		{
			Entry(const Identifier& t, const File& targetFile, ProcessorWithScriptingContent* sp):
				type(t),
				parameterFile(targetFile.withFileExtension("xml"))
			{
				targetFile.create();

				cp = new snex::ui::WorkbenchData::DefaultCodeProvider(wb, targetFile);
				wb = new snex::ui::WorkbenchData();
				wb->setCodeProvider(cp, dontSendNotification);
				wb->setCompileHandler(new SnexSourceCompileHandler(wb, sp));

				if (auto xml = XmlDocument::parse(parameterFile))
					parameterTree = ValueTree::fromXml(*xml);
				else
					parameterTree = ValueTree(PropertyIds::Parameters);

				pListener.setCallback(parameterTree, valuetree::AsyncMode::Asynchronously, BIND_MEMBER_FUNCTION_2(Entry::parameterAddedOrRemoved));
				propListener.setCallback(parameterTree, RangeHelpers::getRangeIds(), valuetree::AsyncMode::Asynchronously, BIND_MEMBER_FUNCTION_2(Entry::propertyChanged));
			}

			const Identifier type;
			const File parameterFile;

			ScopedPointer<snex::ui::WorkbenchData::CodeProvider> cp;

			

			snex::ui::WorkbenchData::Ptr wb;

			void parameterAddedOrRemoved(ValueTree, bool)
			{
				updateFile();
			}

			void propertyChanged(ValueTree, Identifier)
			{
				updateFile();
			}

			ValueTree parameterTree;
			
		private:

			void updateFile()
			{
				auto xml = parameterTree.createXml();
				parameterFile.replaceWithText(xml->createDocument(""));
			}

			valuetree::ChildListener pListener;
			valuetree::RecursivePropertyListener propListener;
		};

		OwnedArray<Entry> entries;

		File getCodeFolder() const;

		DspNetwork& parent;
	} codeManager;
#endif

	void setNumChannels(int newNumChannels);

	void createAllNodesOnce();

	Identifier getObjectName() const override { return "DspNetwork"; };

	String getDebugName() const override { return "DspNetwork"; }
	String getDebugValue() const override { return getId(); }
	void rightClickCallback(const MouseEvent& e, Component* c) override;

	NodeBase* getNodeForValueTree(const ValueTree& v);
	NodeBase::List getListOfUnconnectedNodes() const;

	ValueTree getListOfAvailableModulesAsTree() const;

	StringArray getListOfAllAvailableModuleIds() const;
	StringArray getListOfUsedNodeIds() const;
	StringArray getListOfUnusedNodeIds() const;
	StringArray getFactoryList() const;

	NodeBase::Holder* getCurrentHolder() const;

	void registerOwnedFactory(NodeFactory* ownedFactory);

	NodeBase::List getListOfNodesWithPath(const NamespacedIdentifier& id, bool includeUnusedNodes)
	{
		NodeBase::List list;

		for (auto n : nodes)
		{
			auto path = n->getPath();

			if ((includeUnusedNodes || isInSignalPath(n)) && path == id)
				list.add(n);
		}

		return list;
	}

	template <class T> NodeBase::List getListOfNodesWithType(bool includeUsedNodes)
	{
		NodeBase::List list;

		for (auto n : nodes)
		{
			if ((includeUsedNodes || isInSignalPath(n)) && dynamic_cast<T*>(n) != nullptr)
				list.add(n);
		}

		return list;
	}

	void reset();

	void handleHiseEvent(HiseEvent& e);

	void process(ProcessDataDyn& data);

	void process(AudioSampleBuffer& b, HiseEventBuffer* e);

	bool isPolyphonic() const { return isPoly; }

	bool handleModulation(double& v)
	{
		if (isFrozen())
			return projectNodeHolder.handleModulation(v);
		else
			return networkModValue.getChangedValue(v);
	}

	Identifier getParameterIdentifier(int parameterIndex);

	// ===============================================================================

	/** Defines whether the UI controls of this script control the parameters or regular script callbacks. */
	void setForwardControlsToParameters(bool shouldForward);

	/** Initialise processing of all nodes. */
	void prepareToPlay(double sampleRate, double blockSize);

	/** Process the given channel array with the node network. */
	void processBlock(var data);

	/** Creates and returns a node with the given path (`factory.node`). If a node with the id already exists, it returns this node. */
	var create(String path, String id);

	/** Returns a reference to the node with the given id. */
	var get(var id) const;

	/** Any scripting API call has to be checked using this method. */
	void checkValid() const
	{
		if (parentHolder == nullptr)
			reportScriptError("Parent of DSP Network is deleted");
	}

	/** Deletes the node if it is not in a signal path. */
	bool deleteIfUnused(String id);

	/** Sets the parameters of this node according to the JSON data. */
	bool setParameterDataFromJSON(var jsonData);

	Array<Parameter*> getListOfProbedParameters();

	String getId() const { return data[PropertyIds::ID].toString(); }

	ValueTree getValueTree() const { return data; };

	SimpleReadWriteLock& getConnectionLock() { return parentHolder->getNetworkLock(); }
	bool updateIdsInValueTree(ValueTree& v, StringArray& usedIds);
	NodeBase* createFromValueTree(bool createPolyIfAvailable, ValueTree d, bool forceCreate=false);
	bool isInSignalPath(NodeBase* b) const;

	bool isCurrentlyRenderingVoice() const noexcept { return isPolyphonic() && getPolyHandler()->getVoiceIndex() != -1; }

	bool isRenderingFirstVoice() const noexcept { return !isPolyphonic() || getPolyHandler()->getVoiceIndex() == 0; }

	bool isInitialised() const noexcept { return currentSpecs.blockSize > 0; };

	bool isForwardingControlsToParameters() const
	{
		return forwardControls;
	}

	PrepareSpecs getCurrentSpecs() const { return currentSpecs; }

	NodeBase* getNodeWithId(const String& id) const;

	void checkIfDeprecated();

	Result checkBeforeCompilation();

	struct SelectionListener
	{
		virtual ~SelectionListener() {};
		virtual void selectionChanged(const NodeBase::List& selection) = 0;

		JUCE_DECLARE_WEAK_REFERENCEABLE(SelectionListener);
	};

	void addSelectionListener(SelectionListener* l) { if(selectionUpdater != nullptr) selectionUpdater->listeners.addIfNotAlreadyThere(l); }
	void removeSelectionListener(SelectionListener* l) { if(selectionUpdater != nullptr) selectionUpdater->listeners.removeAllInstancesOf(l); }

	bool isSelected(NodeBase* node) const { return selection.isSelected(node); }

	void deselect(NodeBase* node)
	{
		selection.deselect(node);
	}

	void deselectAll() { selection.deselectAll(); }

	void addToSelection(NodeBase* node, ModifierKeys mods);

	NodeBase::List getSelection() const { return selection.getItemArray(); }

	struct NetworkParameterHandler : public hise::ScriptParameterHandler
	{
		int getNumParameters() const final override { return root->getNumParameters(); }
		Identifier getParameterId(int index) const final override
		{
			return Identifier(root->getParameter(index)->getId());
		}

		float getParameter(int index) const final override
		{
			if(isPositiveAndBelow(index, getNumParameters()))
				return (float)root->getParameter(index)->getValue();

			return 0.0f;
		}

		void setParameter(int index, float newValue) final override
		{
			if(isPositiveAndBelow(index, getNumParameters()))
				root->getParameter(index)->setValueAndStoreAsync((double)newValue);
		}

		NodeBase::Ptr root;
	} networkParameterHandler;

	ValueTree cloneValueTreeWithNewIds(const ValueTree& treeToClone);

	void setEnableUndoManager(bool shouldBeEnabled)
	{
		enableUndo = shouldBeEnabled;
		if (enableUndo)
		{
			startTimer(1500);
		}
		else
			stopTimer();
	}

	ScriptnodeExceptionHandler& getExceptionHandler()
	{
		return exceptionHandler;
	}

	//int* getVoiceIndexPtr() { return &voiceIndex; }

	void timerCallback() override
	{
		um.beginNewTransaction();
	}

	

	void changeNodeId(ValueTree& c, const String& oldId, const String& newId, UndoManager* um);

	UndoManager* getUndoManager()
	{ 
		if (!enableUndo)
			return nullptr;

		if (um.isPerformingUndoRedo())
			return nullptr;
		else
			return &um;
	}

	double getOriginalSampleRate() const noexcept { return originalSampleRate; }

	Array<WeakReference<SnexSource>>& getSnexObjects() 
	{
		return snexObjects;
	}

	ExternalDataHolder* getExternalDataHolder() { return dataHolder; }

	

	void setExternalDataHolder(ExternalDataHolder* newHolder)
	{
		dataHolder = newHolder;
	}

	bool& getCpuProfileFlag() { return enableCpuProfiling; };

	void setVoiceKiller(VoiceResetter* newVoiceKiller)
	{
		if (isPolyphonic())
			getPolyHandler()->setVoiceResetter(newVoiceKiller);	
	}

	void setUseFrozenNode(bool shouldBeEnabled);

	bool canBeFrozen() const { return projectNodeHolder.loaded; }

	bool isFrozen() const { return projectNodeHolder.isActive(); }

	bool hashMatches();

	void setExternalData(const snex::ExternalData & d, int index);

	ScriptParameterHandler* getCurrentParameterHandler();

	Holder* getParentHolder() { return parentHolder; }

	DspNetwork* getParentNetwork() { return parentNetwork.get(); }
	const DspNetwork* getParentNetwork() const { return parentNetwork.get(); }

	void setParentNetwork(DspNetwork* p)
	{
		parentNetwork = p;
	}

	PolyHandler* getPolyHandler()
	{
		if (auto pn = getParentNetwork())
			return pn->getPolyHandler();

		return &polyHandler;
	}

	const PolyHandler* getPolyHandler() const
	{
		if (auto pn = getParentNetwork())
			return pn->getPolyHandler();

		return &polyHandler;
	}

	ModValue& getNetworkModValue() { return networkModValue; }

private:

	ModValue networkModValue;

	bool enableCpuProfiling = false;

	WeakReference<ExternalDataHolder> dataHolder;
	WeakReference<DspNetwork> parentNetwork;

	PrepareSpecs currentSpecs;

	Array<WeakReference<SnexSource>> snexObjects;

	double originalSampleRate = 0.0;

	ScriptnodeExceptionHandler exceptionHandler;

#if USE_BACKEND
	bool enableUndo = true;
#else
	// disable undo on compiled plugins unless explicitely stated
	bool enableUndo = false; 
#endif

	bool forwardControls = true;

	UndoManager um;

	const bool isPoly;

	snex::Types::DllBoundaryTempoSyncer tempoSyncer;
	snex::Types::PolyHandler polyHandler;

	SelectedItemSet<NodeBase::Ptr> selection;

	struct SelectionUpdater : public ChangeListener
	{
		SelectionUpdater(DspNetwork& parent_);
		~SelectionUpdater();

		void changeListenerCallback(ChangeBroadcaster* ) override;

		Array<WeakReference<SelectionListener>> listeners;

		DspNetwork& parent;

		valuetree::RecursiveTypedChildListener deleteChecker;
	};

	ScopedPointer<SelectionUpdater> selectionUpdater;

	OwnedArray<NodeFactory> ownedFactories;

	Array<WeakReference<NodeFactory>> nodeFactories;

	String getNonExistentId(String id, StringArray& usedIds) const;

	valuetree::RecursivePropertyListener idUpdater;
	valuetree::RecursiveTypedChildListener exceptionResetter;

	

	WeakReference<Holder> parentHolder;

	ValueTree data;

	float* currentData[NUM_MAX_CHANNELS];
	friend class DspNetworkGraph;

	struct Wrapper;

	DynamicObject::Ptr loader;

	WeakReference<NodeBase::Holder> currentNodeHolder;

	bool createAnonymousNodes = false;

	struct ProjectNodeHolder: public hise::ScriptParameterHandler
	{
		ProjectNodeHolder(DspNetwork& parent):
			network(parent)
		{

		}

		Identifier getParameterId(int index) const override { return network.networkParameterHandler.getParameterId(index); }
		int getNumParameters() const override { return n.numParameters; }

		void setParameter(int index, float newValue) override
		{
			if (isPositiveAndBelow(index, n.numParameters))
			{
				parameterValues[index] = newValue;
				n.parameterFunctions[index](n.parameterObjects[index], (double)newValue);
			}
		}

		float getParameter(int index) const override 
		{ 
			if(isPositiveAndBelow(index, 16))
				return parameterValues[index]; 

			return 0.0f;
		};

		~ProjectNodeHolder()
		{
			if (loaded)
			{
				n.callDestructor();
			}
		}

		bool isActive() const { return forwardToNode; }

		void prepare(PrepareSpecs ps)
		{
			n.prepare(ps);
			n.reset();
		}

		void process(ProcessDataDyn& data);

		bool handleModulation(double& modValue)
		{
			return n.handleModulation(modValue);
		}

		void setEnabled(bool shouldBeEnabled)
		{
			if (!loaded)
				return;

			if (shouldBeEnabled != forwardToNode)
			{
				forwardToNode = shouldBeEnabled;

				auto s1 = static_cast<ScriptParameterHandler*>(&network.networkParameterHandler);
				auto s2 = static_cast<ScriptParameterHandler*>(this);

				auto oh = forwardToNode ? s1 : s2;
				auto nh = forwardToNode ? s2 : s1;

				if (forwardToNode && network.currentSpecs)
				{
					prepare(network.currentSpecs);
					n.reset();
				}

				for (int i = 0; i < nh->getNumParameters(); i++)
					nh->setParameter(i, oh->getParameter(i));
			}
		}

		void init(dll::ProjectDll::Ptr dllToUse);

		bool hashMatches = false;
		float parameterValues[16];
		DspNetwork& network;
		dll::ProjectDll::Ptr dll;
		OpaqueNode n;
		bool loaded = false;
		bool forwardToNode = false;
	} projectNodeHolder;

	JUCE_DECLARE_WEAK_REFERENCEABLE(DspNetwork);
};


struct OpaqueNetworkHolder
{
	SN_GET_SELF_AS_OBJECT(OpaqueNetworkHolder);

	bool isPolyphonic() const { return false; }

	HISE_EMPTY_INITIALISE;
	HISE_EMPTY_PROCESS_SINGLE;

	OpaqueNetworkHolder()
	{

	}

	~OpaqueNetworkHolder()
	{
		ownedNetwork = nullptr;
	}

	void handleHiseEvent(HiseEvent& e)
	{
		ownedNetwork->handleHiseEvent(e);
	}

	bool handleModulation(double& modValue)
	{
		return ownedNetwork->handleModulation(modValue);
	}

	void process(ProcessDataDyn& d)
	{
		ownedNetwork->process(d);
	}

	void reset()
	{
		ownedNetwork->reset();
	}

	void prepare(PrepareSpecs ps)
	{
		snex::Types::DllBoundaryTempoSyncer::ScopedModValueChange smvs(*ps.voiceIndex->getTempoSyncer(), ownedNetwork->getNetworkModValue());
		ownedNetwork->prepareToPlay(ps.sampleRate, ps.blockSize);
	}

	void createParameters(ParameterDataList& l);

	void setCallback(parameter::data& d, int index);

	template <int P> static void setParameterStatic(void* obj, double v)
	{
		auto t = static_cast<OpaqueNetworkHolder*>(obj);
		t->ownedNetwork->getCurrentParameterHandler()->setParameter(P, (float)v);
	}

	void setNetwork(DspNetwork* n);

	DspNetwork* getNetwork() {
		return ownedNetwork;
	}

	void setExternalData(const ExternalData& d, int index);

private:

	struct DeferedDataInitialiser
	{
		ExternalData d;
		int index;
	};

	Array<DeferedDataInitialiser> deferredData;
	ReferenceCountedObjectPtr<DspNetwork> ownedNetwork;
};

struct HostHelpers
{
    struct NoExtraComponent
    {
        static Component* createExtraComponent(void* , PooledUIUpdater*) { return nullptr; }
    };
    
	static int getNumMaxDataObjects(const ValueTree& v, snex::ExternalData::DataType t);

	static void setNumDataObjectsFromValueTree(OpaqueNode& on, const ValueTree& v);

	template <typename WrapperType> static NodeBase* initNodeWithNetwork(DspNetwork* p, ValueTree nodeTree, const ValueTree& embeddedNetworkTree, bool useMod)
	{
		auto t = dynamic_cast<WrapperType*>(WrapperType::template createNode<OpaqueNetworkHolder, NoExtraComponent, false, false>(p, nodeTree));

		auto& on = t->getWrapperType().getWrappedObject();
		setNumDataObjectsFromValueTree(on, embeddedNetworkTree);
		auto ed = t->setOpaqueDataEditor(useMod);

		auto onh = static_cast<OpaqueNetworkHolder*>(on.getObjectPtr());
		onh->setNetwork(p->getParentHolder()->addEmbeddedNetwork(p, embeddedNetworkTree, ed));

		ParameterDataList pList;
		onh->createParameters(pList);
		on.fillParameterList(pList);

		t->postInit();
		auto asNode = dynamic_cast<NodeBase*>(t);
		asNode->setEmbeddedNetwork(onh->getNetwork());

		return asNode;
	}
};

#if USE_BACKEND
struct DspNetworkListeners
{
	struct Base : public valuetree::AnyListener
	{
		template <bool AllowRootParameterChange> static bool isValueProperty(const ValueTree& v, const Identifier& id)
		{
			if (v.getType() == PropertyIds::Parameter && id == PropertyIds::Value)
			{
				if (AllowRootParameterChange)
				{
					auto possibleRoot = v.getParent().getParent().getParent();

					if (possibleRoot.getType() == PropertyIds::Network)
						return false;
				}

				return !v[PropertyIds::Automated];
			}

			if (id == PropertyIds::NumChannels)
				return false;

			return true;
		}

		Base(DspNetwork* n, bool isSync) :
			AnyListener(isSync ? valuetree::AsyncMode::Synchronously : valuetree::AsyncMode::Asynchronously),
			network(n),
			creationTime(Time::getMillisecondCounter())
		{}

		void postInit(bool sync)
		{
			if(!sync)
				setMillisecondsBetweenUpdate(1000);

			setRootValueTree(network->getValueTree());
		}

		virtual ~Base()
		{

		}

		void anythingChanged(CallbackType cb) override
		{
			auto t = Time::getMillisecondCounter();

			if ((t - creationTime) < 2000)
				return;

			changed = true;
			networkChanged();
		}

		virtual bool isChanged() const { return changed; }
		virtual void networkChanged() = 0;

	protected:

		uint32 creationTime;

		bool changed = false;
		WeakReference<DspNetwork> network;
	};

	struct LambdaAtNetworkChange : public Base
	{
		LambdaAtNetworkChange(scriptnode::DspNetwork* n) :
			Base(n, true)
		{
			setPropertyCondition(Base::isValueProperty<true>);
			postInit(true);
		}

		void networkChanged() override
		{
			if (additionalCallback)
				additionalCallback();
		}

		std::function<void()> additionalCallback;
	};

	struct PatchAutosaver : public Base
	{
		PatchAutosaver(scriptnode::DspNetwork* n, const File& directory) :
			Base(n, false)
		{
			setPropertyCondition(Base::isValueProperty<false>);
			postInit(false);

			auto id = "autosave_" + n->getRootNode()->getId();
			d = directory.getChildFile(id).withFileExtension("xml");
		}

		~PatchAutosaver()
		{
			if (d.existsAsFile())
				d.deleteFile();
		}

		bool isChanged() const override { return Base::isChanged() && d.existsAsFile(); }

		void closeAndDelete(bool forceDelete)
		{
			if (changed || forceDelete)
			{
				anythingChanged(valuetree::AnyListener::PropertyChange);

				auto ofile = d.getSiblingFile(network->getRootNode()->getId()).withFileExtension(".xml");
				auto backup = ofile.loadFileAsString();
				auto ok = ofile.deleteFile();

				if(ok)
					ok = d.moveFileTo(ofile);

				if (!ok)
					ofile.replaceWithText(backup);

				creationTime = Time::getMillisecondCounter();
			}
		}

		void networkChanged() override
		{
			auto saveCopy = network->getValueTree().createCopy();

			cppgen::ValueTreeIterator::forEach(saveCopy, snex::cppgen::ValueTreeIterator::IterationType::Forward, stripValueTree);

			auto xml = saveCopy.createXml();

			d.replaceWithText(xml->createDocument(""));
		}

	private:

		static void removeIfDefault(ValueTree v, const Identifier& id, const var& defaultValue)
		{
			if (v[id] == defaultValue)
				v.removeProperty(id, nullptr);
		};

		static bool removePropIfDefault(ValueTree v, const Identifier& id, const var& defaultValue)
		{
			if (v[PropertyIds::ID].toString() == id.toString() &&
				v[PropertyIds::Value] == defaultValue)
			{
				return true;
			}

			return false;
		};

		static void removeIfNoChildren(ValueTree v)
		{
			if (v.isValid() && v.getNumChildren() == 0)
				v.getParent().removeChild(v, nullptr);
		}

		static void removeIfDefined(ValueTree v, const Identifier& id, const Identifier& definedId)
		{
			if (v.hasProperty(definedId))
				v.removeProperty(id, nullptr);
		}

		static bool stripValueTree(ValueTree& v)
		{
			// Remove all child nodes from a project node
			// (might be a leftover from the extraction process)
			if (v.getType() == PropertyIds::Node && v[PropertyIds::FactoryPath].toString().startsWith("project"))
			{
				v.removeChild(v.getChildWithName(PropertyIds::Nodes), nullptr);

				for (auto p : v.getChildWithName(PropertyIds::Parameters))
					p.removeChild(p.getChildWithName(PropertyIds::Connections), nullptr);
			}

			auto propChild = v.getChildWithName(PropertyIds::Properties);

			// Remove all properties with the default value
			for (int i = 0; i < propChild.getNumChildren(); i++)
			{
				if (removePropIfDefault(propChild.getChild(i), PropertyIds::IsVertical, 1))
					propChild.removeChild(i--, nullptr);
			}

			removeIfNoChildren(propChild);

			for (auto id : PropertyIds::Helpers::getDefaultableIds())
				removeIfDefault(v, id, PropertyIds::Helpers::getDefaultValue(id));

			removeIfDefined(v, PropertyIds::Value, PropertyIds::Automated);

			removeIfNoChildren(v.getChildWithName(PropertyIds::Bookmarks));
			removeIfNoChildren(v.getChildWithName(PropertyIds::ModulationTargets));

			return false;
		}

		File d;
	};
};
#endif



}


