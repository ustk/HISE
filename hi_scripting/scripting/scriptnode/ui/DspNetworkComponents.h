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
using namespace juce;

class ProcessorWithScriptingContent;
}

namespace scriptnode
{

using namespace juce;
using namespace hise;

class KeyboardPopup : public Component,
					  public TextEditor::Listener,
					  public KeyListener,
					  public ButtonListener
{
public:

	enum Mode
	{
		Wrap,
		Surround,
		New,
		numModes
	};

	struct TagList: public Component
	{
		String currentTag;

		void toggleTag(const String& tag)
		{
			if (currentTag == tag)
			{
				currentTag = {};
			}
			else
				currentTag = tag;

			findParentComponentOfClass<KeyboardPopup>()->setSearchText(currentTag);

			refreshTags();
		}

		void refreshTags()
		{
			for (auto t : tags)
			{
				t->setActive(currentTag.isEmpty() ? 0.5f : (currentTag == t->getName() ? 1.0f : 0.2f));
			}
		}

		struct Tag : public Component
		{
			void setActive(float alpha_)
			{
				alpha = alpha_;
				active = alpha > 0.5f;
				
				repaint();
			}

			Tag(const String& t)
			{
				setRepaintsOnMouseActivity(true);

				setName(t);

				auto w = GLOBAL_BOLD_FONT().getStringWidth(t) + 25;
				setSize(w, 28);
			}

			void mouseDown(const MouseEvent& )
			{
				findParentComponentOfClass<TagList>()->toggleTag(getName());
			}

			float alpha = 0.5f;
			bool active = false;

			void paint(Graphics& g) override;
		};

		TagList(DspNetwork* n)
		{
			auto factories = n->getFactoryList();

			for (auto f : factories)
				tags.add(new Tag(f));

			int x = 0;

			for (auto t : tags)
			{
				x += t->getWidth();
				addAndMakeVisible(t);
				x += 3;
			}

			tagWidth = x;
				
			
			setSize(x, 32);
		}

		int getTagWidth() const
		{
			return tagWidth;
		}

		int tagWidth;

		void resized() override
		{
			int x = 0;

			for (auto t : tags)
			{
				t->setTopLeftPosition(x, 2);

				x += t->getWidth() + 3;
			}
		}

		OwnedArray<Tag> tags;
		
	};

	struct Factory : public PathFactory
	{
		String getId() const override { return ""; }

		Path createPath(const String& url) const override
		{
			Path p;

			LOAD_PATH_IF_URL("help", MainToolbarIcons::help);

			return p;
		}
	} factory;

	int addPosition;

	Image screenshot;

	struct ImagePreviewCreator: public Timer
	{
		ImagePreviewCreator(KeyboardPopup& kp, const String& path);

		~ImagePreviewCreator();

		void timerCallback();

		KeyboardPopup& kp;
		NodeBase::Holder holder;
		DspNetwork* network;
		WeakReference<NodeBase> createdNode;
		ScopedPointer<Component> createdComponent;
		String path;
	};

	ScopedPointer<ImagePreviewCreator> currentPreview;

	KeyboardPopup(NodeBase* container, int addPosition_):
		node(container),
		network(node->getRootNetwork()),
		list(network),
		tagList(network),
		addPosition(addPosition_),
		helpButton("help", this, factory)
	{
		laf.bg = Colours::transparentBlack;
		viewport.setLookAndFeel(&laf);

		setName("Create Node");

		addAndMakeVisible(helpButton);
		helpButton.setToggleModeWithColourChange(true);
		addAndMakeVisible(tagList);
		addAndMakeVisible(nodeEditor);

		nodeEditor.setFont(GLOBAL_MONOSPACE_FONT());
		nodeEditor.setColour(TextEditor::ColourIds::backgroundColourId, Colours::white.withAlpha(0.8f));
		nodeEditor.setColour(TextEditor::ColourIds::highlightColourId, Colour(SIGNAL_COLOUR).withAlpha(0.5f));
		nodeEditor.setSelectAllWhenFocused(true);
		nodeEditor.setColour(TextEditor::ColourIds::focusedOutlineColourId, Colour(SIGNAL_COLOUR));

		nodeEditor.addListener(this);
		addAndMakeVisible(viewport);
		viewport.setViewedComponent(&list, false);

		viewport.setScrollBarThickness(14);

		viewport.setScrollOnDragEnabled(true);
		helpViewport.setScrollOnDragEnabled(true);

		helpButton.addListener(this);
		setSize(tagList.getWidth(), 64 + 200 + 2 * UIValues::NodeMargin);
		nodeEditor.addKeyListener(this);
		list.rebuild(getWidthForListItems());
	}

	void buttonClicked(Button* )
	{
		if (help == nullptr)
		{
			help = new Help(network);
			help->showDoc(list.getCurrentText());
			helpViewport.setViewedComponent(help, false);
			helpViewport.setVisible(true);
			setSize(tagList.getTagWidth() + 100, tagList.getBottom() + 400);
			resized();
		}
		else
		{
			help = nullptr;
			setSize(tagList.getTagWidth(), tagList.getBottom() + viewport.getHeight());
			helpViewport.setVisible(false);
			resized();
		}
	}

	void scrollToMakeVisible(int yPos)
	{
		auto r = viewport.getViewArea();

		Range<int> range(r.getY(), r.getBottom());

		Range<int> itemRange(yPos, yPos + PopupList::ItemHeight);

		if (range.contains(itemRange))
			return;

		bool scrollDown = itemRange.getEnd() > range.getEnd();

		if (scrollDown)
		{
			auto delta = itemRange.getEnd() - range.getEnd();
			viewport.setViewPosition(0, range.getStart() + delta);
		}
		else
			viewport.setViewPosition(0, yPos);
	}

	bool keyPressed(const KeyPress& k, Component*) override;

	void addNodeAndClose(String path);

	

	struct Help : public Component
	{
		Help(DspNetwork* n);

		void paint(Graphics& g) override
		{
			g.fillAll(Colour(0xFF262626).withAlpha(0.3f));

			renderer.draw(g, getLocalBounds().toFloat().reduced(10.0f));
		}

		void showDoc(const String& text);

		void rebuild(int maxWidth)
		{
			auto height = renderer.getHeightForWidth((float)(maxWidth)-20.0f);
			setSize(maxWidth, (int)height + 20);
		}

		File rootDirectory;

		MarkdownRenderer renderer;

		static void initGenerator(const File& f, MainController* mc);
		static bool initialised;

	};

	

	void textEditorTextChanged(TextEditor&) override
	{
		setSearchText(nodeEditor.getText());
	}

	int getWidthForListItems() const
	{
		return help != nullptr ? 0 : tagList.getWidth() / 2 - viewport.getScrollBarThickness();
	}

	void updateHelp()
	{
		if (help != nullptr)
			help->showDoc(list.getCurrentText());
	}

	void setSearchText(const String& text)
	{
		nodeEditor.setText(text, dontSendNotification);
		list.setSearchText(nodeEditor.getText());

		auto minY = tagList.getBottom();

		list.rebuild(getWidthForListItems());
		auto height = jmin(300, list.getHeight() + 10);

		if (help != nullptr)
			height = jmax(400, height);
		

		//setSize(tagList.getWidth(), minY + height);

		updateHelp();

		resized();
	}

	void resized() override
	{
		auto b = getLocalBounds();

		auto top = b.removeFromTop(32).reduced(3);

		helpButton.setBounds(top.removeFromRight(top.getHeight()).reduced(2));
		top.removeFromLeft(top.getHeight());
		nodeEditor.setBounds(top.reduced(8, 0));

		b.removeFromTop(UIValues::NodeMargin);

		tagList.setBounds(b.removeFromTop(32));
		list.rebuild(getWidthForListItems());
		
		b.removeFromTop(UIValues::NodeMargin);

		if (help != nullptr)
		{
			viewport.setBounds(b.removeFromLeft(list.getWidth() + viewport.getScrollBarThickness()));
			help->rebuild(b.getWidth() - helpViewport.getScrollBarThickness());
			helpViewport.setBounds(b);
		}
		else
		{
			helpViewport.setVisible(false);

			viewport.setBounds(b.removeFromLeft(getWidth() / 2));
		}
	}

	Rectangle<float> getPreviewBounds();

	void paint(Graphics& g) override;

	struct PopupList : public Component
	{
		static constexpr int ItemHeight = 24;

		enum Type
		{
			Clipboard,
			ExistingNode,
			NewNode
		};


		struct Entry
		{
			bool operator==(const Entry& other) const
			{
				return displayName == other.displayName;
			}

			Type t;
			String insertString;
			String displayName;
		};

		Array<Entry> allIds;
		String searchTerm;

		void rebuildItems()
		{
			allIds.clear();

			auto clipboardContent = SystemClipboard::getTextFromClipboard();

			if (clipboardContent.startsWith("ScriptNode"))
			{
				auto v = ValueTreeConverters::convertBase64ToValueTree(clipboardContent.fromFirstOccurrenceOf("ScriptNode", false, true), true);

				Entry cItem;
				cItem.t = Clipboard;
				cItem.insertString = clipboardContent;
				cItem.displayName = v[PropertyIds::ID].toString();

				allIds.add(cItem);
			}

			for (auto existingNodeId : network->getListOfUnusedNodeIds())
			{
				Entry eItem;
				eItem.t = ExistingNode;
				eItem.insertString = existingNodeId;
				eItem.displayName = existingNodeId;
				allIds.add(eItem);
			}

			for (auto newNodePath : network->getListOfAllAvailableModuleIds())
			{
				Entry nItem;
				nItem.t = NewNode;
				nItem.insertString = newNodePath;
				nItem.displayName = newNodePath;
				allIds.add(nItem);
			}

			rebuild(getWidth());
		}

		WeakReference<DspNetwork> network;

		PopupList(DspNetwork* n):
			network(n)
		{
			rebuildItems();
		}

		String getTextToInsert() const
		{
			if (auto i = items[selectedIndex])
				return i->entry.insertString;

			return {};
		}

		String getCurrentText() const
		{
			if (auto i = items[selectedIndex])
				return i->entry.displayName;

			return {};
		}


		int selectedIndex = 0;

		int selectNext(bool next)
		{
			if (next)
				selectedIndex = jmin(items.size(), selectedIndex + 1);
			else
				selectedIndex = jmax(0, selectedIndex - 1);

			rebuild(maxWidth);

			setSelected(items[selectedIndex]);

			return selectedIndex * ItemHeight;
		}

		void setSearchText(const String& text)
		{
			searchTerm = text.toLowerCase();
			rebuild(maxWidth);

			selectedIndex = 0;
			setSelected(items[selectedIndex]);
		}

		void paint(Graphics& g) override
		{
			//g.fillAll(Colours::black.withAlpha(0.2f));
		}

		struct Item : public Component,
					  public ButtonListener
		{
			Item(const Entry& entry_, bool isSelected_);

			void buttonClicked(Button* b) override;

			void mouseDown(const MouseEvent& );

			void mouseUp(const MouseEvent& event);

			void paint(Graphics& g) override;

			void resized() override;

			bool selected = false;
			Entry entry;

			Path p;
			NodeComponent::Factory f;
			HiseShapeButton deleteButton;
		};

		void setSelected(Item* i);

		void resized() override
		{
			int y = 0;

			for (auto i : items)
			{
				i->setBounds(0, y, getWidth(), ItemHeight);
				y += ItemHeight;
			}
		}

		int maxWidth = 0;

		void rebuild(int maxWidthToUse)
		{
			items.clear();

			int y = 0;
			
			maxWidth = maxWidthToUse;

			auto f = GLOBAL_MONOSPACE_FONT();

			for (auto id : allIds)
			{
				if (searchTerm.isNotEmpty() && !id.displayName.contains(searchTerm))
					continue;

				if (searchTerm == id.displayName)
					selectedIndex = items.size();

				auto newItem = new Item(id, selectedIndex == items.size());
				items.add(newItem);
				addAndMakeVisible(newItem);

				maxWidth = jmax(f.getStringWidth(id.displayName) + 20, maxWidth);
				y += ItemHeight;
			}

			setSize(maxWidth, y);
			resized();
		}

		OwnedArray<Item> items;
	};

	NodeBase::Ptr node;
	DspNetwork* network;
	
	TextEditor nodeEditor;
	TagList tagList;
	PopupList list;
	Viewport viewport;

	ZoomableViewport::Laf laf;

	ScopedPointer<Help> help;
	Viewport helpViewport;
	
	
	
	HiseShapeButton helpButton;
	
};



struct DspNetworkPathFactory : public PathFactory
{
	String getId() const override { return {}; }

	Path createPath(const String& url) const override;
};


class DspNetworkGraph : public Component,
	public AsyncUpdater,
	public DspNetwork::SelectionListener
{
public:

	struct ActionButton : public WrapperWithMenuBarBase::ActionButtonBase<DspNetworkGraph, DspNetworkPathFactory>,
						  public DspNetwork::SelectionListener
	{
		ActionButton(DspNetworkGraph* parent_, const String& name) :
			ActionButtonBase<DspNetworkGraph, DspNetworkPathFactory>(parent_, name)
		{
			parent.getComponent()->network->addSelectionListener(this);
		}

		~ActionButton()
		{
			if (parent.getComponent() != nullptr)
				parent.getComponent()->network->removeSelectionListener(this);
		}


		void selectionChanged(const NodeBase::List& l) override
		{
			repaint();
		}
	};

	struct WrapperWithMenuBar : public WrapperWithMenuBarBase
	{
		static bool selectionEmpty(DspNetworkGraph& g)
		{
			return !g.network->getSelection().isEmpty();
		}

		WrapperWithMenuBar(DspNetworkGraph* g):
			WrapperWithMenuBarBase(g),
			n(g->network)
		{
			addButton("zoom");

			addBookmarkComboBox();

			addSpacer(10);

			addButton("fold");
			addButton("foldunselected");
			addButton("swap-orientation");

			addSpacer(10);

			addButton("error");
			addButton("goto");
			addButton("cable");
			addButton("probe");
			addSpacer(10);
			addButton("add");
			addButton("wrap");
			addButton("export");
			addButton("deselect");

			addButton("bypass");
			addButton("colour");
			addButton("properties");
			addButton("profile");

			addSpacer(10);
			addButton("undo");
			addButton("redo");
			addSpacer(10);
			addButton("copy");
			addButton("duplicate");
			addButton("delete");
		};

		virtual void bookmarkUpdated(const StringArray& idsToShow)
		{
			n->deselectAll();

			NodeBase::List newSelection;

			for (const auto& s : idsToShow)
			{
				auto nv = n->get(s);

				if (auto no = dynamic_cast<NodeBase*>(nv.getObject()))
					n->addToSelection(no, ModifierKeys::shiftModifier);
			}

			Actions::foldUnselectedNodes(*canvas.getContent<DspNetworkGraph>());
		}

		virtual ValueTree getBookmarkValueTree()
		{
			return n->getValueTree().getOrCreateChildWithName(PropertyIds::Bookmarks, n->getUndoManager());
		}

		void addButton(const String& b) override;

		DspNetwork* n;
	};

	struct Actions
	{
		static void selectAndScrollToNode(DspNetworkGraph& g, NodeBase::Ptr node);

		static bool swapOrientation(DspNetworkGraph& g);

		static bool freezeNode(NodeBase::Ptr node);
		static bool unfreezeNode(NodeBase::Ptr node);

		static bool toggleBypass(DspNetworkGraph& g);
		static bool toggleFreeze(DspNetworkGraph& g);

		static bool toggleProbe(DspNetworkGraph& g);
		static bool setRandomColour(DspNetworkGraph& g);

		static bool copyToClipboard(DspNetworkGraph& g);
		static bool toggleCableDisplay(DspNetworkGraph& g);
		static bool toggleCpuProfiling(DspNetworkGraph& g);
		static bool editNodeProperty(DspNetworkGraph& g);
		static bool foldSelection(DspNetworkGraph& g);
		static bool foldUnselectedNodes(DspNetworkGraph& g);
		static bool arrowKeyAction(DspNetworkGraph& g, const KeyPress& k);
		static bool showKeyboardPopup(DspNetworkGraph& g, KeyboardPopup::Mode mode);
		static bool duplicateSelection(DspNetworkGraph& g);
		static bool deselectAll(DspNetworkGraph& g);;
		static bool deleteSelection(DspNetworkGraph& g);
		static bool showJSONEditorForSelection(DspNetworkGraph& g);
		static bool undo(DspNetworkGraph& g);
		static bool redo(DspNetworkGraph& g);

		static bool addBookMark(DspNetworkGraph& g);

		static bool zoomIn(DspNetworkGraph& g);
		static bool zoomOut(DspNetworkGraph& g);
		static bool zoomFit(DspNetworkGraph& g);
	};

	void selectionChanged(const NodeBase::List&) override
	{
		
	}

	DspNetworkGraph(DspNetwork* n);
	~DspNetworkGraph();

	bool keyPressed(const KeyPress& key) override;
	void handleAsyncUpdate() override;
	void rebuildNodes();
	void resizeNodes();
	void updateDragging(Point<int> position, bool copyNode);
	void finishDrag();
	void paint(Graphics& g) override;
	void resized() override;

	template <class T> static void fillChildComponentList(Array<T*>& list, Component* c)
	{
		for (int i = 0; i < c->getNumChildComponents(); i++)
		{
			auto child = c->getChildComponent(i);

			if (!child->isShowing())
				continue;

			if (auto typed = dynamic_cast<T*>(child))
			{
				list.add(typed);
			}

			fillChildComponentList(list, child);
		}
	}

	void paintOverChildren(Graphics& g) override;

	void toggleProbeMode()
	{
		probeSelectionEnabled = !probeSelectionEnabled;

		auto ft = findParentComponentOfClass<FloatingTile>();

		if (!probeSelectionEnabled && !ft->isRootPopupShown())
		{
			DynamicObject::Ptr obj = new DynamicObject();
			
			auto l = network->getListOfProbedParameters();

			for (auto p : l)
			{
				String key;
				key << p->parent->getId() << "." << p->getId();
				obj->setProperty(Identifier(key), p->getValue());
			}

			String s;
			s << "// Set the properties of this object to the parameter values\n";
			s << "var data = " << JSON::toString(var(obj)) << ";";


			auto n = new JSONEditor(s, new JavascriptTokeniser());

			n->setCompileCallback([this](const String& text, var& data)
			{
				ScopedPointer<JavascriptEngine> e = new JavascriptEngine();

				auto r = e->execute(text);

				data = e->getRootObjectProperties().getWithDefault("data", {});

				return r;
			}, false);

			n->setCallback([this](const var& d)
			{
				network->setParameterDataFromJSON(d);
				
			}, false);

			n->setEditable(true);
			n->setName("Edit Parameter List");
			n->setSize(600, 400);
			
			
			auto c = findParentComponentOfClass<WrapperWithMenuBar>()->actionButtons[3];
			
			ft->showComponentInRootPopup(n, c, c->getLocalBounds().getBottomRight());
		}

		repaint();
	}

	NodeComponent* getComponent(NodeBase::Ptr node);

	static void paintCable(Graphics& g, Rectangle<float> start, Rectangle<float> end, Colour c, float alpha=1.0f, Colour holeColour=Colour(0xFFAAAAAA))
	{
		if (start.getCentreY() > end.getCentreY())
			std::swap(start, end);

		if (alpha != 1.0f)
		{
			holeColour = c;
		}

		static const unsigned char pathData[] = { 110,109,233,38,145,63,119,190,39,64,108,0,0,0,0,227,165,251,63,108,0,0,0,0,20,174,39,63,108,174,71,145,63,0,0,0,0,108,174,71,17,64,20,174,39,63,108,174,71,17,64,227,165,251,63,108,115,104,145,63,119,190,39,64,108,115,104,145,63,143,194,245,63,98,55,137,
145,63,143,194,245,63,193,202,145,63,143,194,245,63,133,235,145,63,143,194,245,63,98,164,112,189,63,143,194,245,63,96,229,224,63,152,110,210,63,96,229,224,63,180,200,166,63,98,96,229,224,63,43,135,118,63,164,112,189,63,178,157,47,63,133,235,145,63,178,
157,47,63,98,68,139,76,63,178,157,47,63,84,227,5,63,43,135,118,63,84,227,5,63,180,200,166,63,98,84,227,5,63,14,45,210,63,168,198,75,63,66,96,245,63,233,38,145,63,143,194,245,63,108,233,38,145,63,119,190,39,64,99,101,0,0 };

		Path plug;

		plug.loadPathFromData(pathData, sizeof(pathData));
		PathFactory::scalePath(plug, start.expanded(1.5f));

		g.setColour(Colours::black);
		g.fillEllipse(start);
		g.setColour(Colour(0xFFAAAAAA));

		g.fillPath(plug);

		//g.drawEllipse(start, 2.0f);

		g.setColour(Colours::black);
		g.fillEllipse(end);
		g.setColour(holeColour);
		PathFactory::scalePath(plug, end.expanded(1.5f));
		g.fillPath(plug);
		//g.drawEllipse(end, 2.0f);

		

		Path p;

		p.startNewSubPath(start.getCentre());

		Point<float> controlPoint(start.getX() + (end.getX() - start.getX()) / 2.0f, end.getY() + 100.0f);

		p.quadraticTo(controlPoint, end.getCentre());

		g.setColour(Colours::black.withMultipliedAlpha(alpha));
		g.strokePath(p, PathStrokeType(3.0f, PathStrokeType::curved, PathStrokeType::rounded));
		g.setColour(c.withMultipliedAlpha(alpha));
		g.strokePath(p, PathStrokeType(2.0f, PathStrokeType::curved, PathStrokeType::rounded));
	};

	static Rectangle<float> getCircle(Component* c, bool getKnobCircle=true)
	{
		if (auto n = c->findParentComponentOfClass<DspNetworkGraph>())
		{
			float width = 6.0f;
			float height = 6.0f;
			float y = getKnobCircle ? 66.0f : (c->getHeight());

			float offsetY = (float)c->getProperties()["circleOffsetY"];
			float offsetX = (float)c->getProperties()["circleOffsetX"];

			y += offsetY;
			
			float circleX = c->getLocalBounds().toFloat().getWidth() / 2.0f - width / 2.0f;

			circleX += offsetX;

			Rectangle<float> circleBounds = { circleX, y, width, height };

			return n->getLocalArea(c, circleBounds.toNearestInt()).toFloat();
		}

		return {};
	};

	struct PeriodicRepainter : public PooledUIUpdater::SimpleTimer
	{
		PeriodicRepainter(DspNetworkGraph& p) :
			SimpleTimer(p.network->getScriptProcessor()->getMainController_()->getGlobalUIUpdater()),
			componentToUpdate(&p)
		{
			start();
		}
			

		void timerCallback() override
		{
			componentToUpdate->repaint();
		}

		Component* componentToUpdate;
	};

	void enablePeriodicRepainting(bool shouldBeEnabled)
	{
		if (shouldBeEnabled)
			periodicRepainter = new PeriodicRepainter(*this);
		else
			periodicRepainter = nullptr;
	}
	

	bool showCables = true;
	bool probeSelectionEnabled = false;

	bool setCurrentlyDraggedComponent(NodeComponent* n);

	struct DragOverlay : public Timer
	{
		DragOverlay(DspNetworkGraph& g) :
			parent(g)
		{};

		void timerCallback() override
		{
			float onDelta = JUCE_LIVE_CONSTANT_OFF(.1f);
			float offDelta = JUCE_LIVE_CONSTANT_OFF(.1f);

			if (enabled)
				alpha += onDelta;
			else
				alpha -= offDelta;

			if (alpha >= 1.0f || alpha <= 0.0f)
				stopTimer();

			alpha = jlimit(0.0f, 1.0f, alpha);

			parent.repaint();
		}

		void setEnabled(bool shouldBeEnabled)
		{
			if (shouldBeEnabled != enabled)
			{
				enabled = shouldBeEnabled;

				startTimer(30);
			}

			parent.repaint();
		}

		DspNetworkGraph& parent;
		bool enabled = false;
		float alpha = 0.0f;
	} dragOverlay;

	ValueTree dataReference;

	bool copyDraggedNode = false;

	valuetree::RecursivePropertyListener cableRepainter;
	valuetree::ChildListener rebuildListener;
	valuetree::RecursivePropertyListener resizeListener;

	valuetree::RecursiveTypedChildListener macroListener;

	ScopedPointer<NodeComponent> root;

	ScopedPointer<PeriodicRepainter> periodicRepainter;

	WeakReference<NodeDropTarget> currentDropTarget;
	ScopedPointer<NodeComponent> currentlyDraggedComponent;

	WeakReference<DspNetwork> network;
};


}

