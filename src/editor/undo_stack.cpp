#include "pch.h"
#include "undo_stack.h"
#include "core/imgui.h"

undo_stack::undo_stack()
{
	memorySize = MB(2);
	memory = (uint8*)malloc(memorySize);
	reset();
}

void undo_stack::pushAction(const char* name, const void* entry, uint64 entrySize, toggle_func toggle)
{
	uint64 requiredSpace = sizeof(entry_header) + entrySize;
	uint64 availableSpaceAtEnd = memory + memorySize - nextToWrite;

	uint8* address;

	if (!newest) // Stack is empty.
	{
		nextToWrite = memory;
		address = nextToWrite;
		oldest = 0;
	}
	else
	{
		assert(oldest);
		address = (requiredSpace <= availableSpaceAtEnd) ? nextToWrite : memory;
	}

	uint8* end = (uint8*)alignTo(address + requiredSpace, 16);

	// Clean up blocks which are overriden by the new block.
	oldest = newest;
	if (oldest)
	{
		while (true)
		{
			void* oldestBegin = oldest;
			void* oldestEnd = (uint8*)(oldest + 1) + oldest->entrySize;

			if (rangesOverlap(address, end, oldestBegin, oldestEnd))
			{
				oldest = oldest->newer;
				break;
			}
			if (!oldest->older) { break; }

			oldest = oldest->older;
		}
	}

	if (oldest)
	{
		oldest->older = 0;
	}


	entry_header* header = (entry_header*)address;
	void* data = (header + 1);

	header->older = newest;
	if (newest)
	{
		newest->newer = header;
	}
	if (!oldest)
	{
		oldest = header;
	}
	newest = header;

	header->newer = 0;

	header->name = name;
	header->toggle = toggle;
	header->entrySize = entrySize;

	memcpy(data, entry, entrySize);

	nextToWrite = end;
}

std::pair<bool, const char*> undo_stack::undoPossible()
{
	return 
	{ 
		newest != 0, 
		newest ? newest->name : 0 
	};
}

std::pair<bool, const char*> undo_stack::redoPossible()
{
	return 
	{ 
		newest && newest->newer || !newest && oldest,
		(newest && newest->newer) ? newest->newer->name : (!newest && oldest) ? oldest->name : 0 
	};
}

void undo_stack::undo()
{
	if (newest)
	{
		void* data = (newest + 1);
		newest->toggle(data);

		newest = newest->older;
		if (newest)
		{
			nextToWrite = (uint8*)alignTo(((uint8*)data + newest->entrySize), 16);
		}
		else
		{
			nextToWrite = memory;
		}

		// Keep link to newer.
	}
}

void undo_stack::redo()
{
	if (newest && newest->newer)
	{
		newest = newest->newer;

		void* data = (newest + 1);
		newest->toggle(data);

		nextToWrite = (uint8*)alignTo(((uint8*)data + newest->entrySize), 16);
	}

	if (!newest && oldest)
	{
		void* data = (oldest + 1);
		oldest->toggle(data);

		nextToWrite = (uint8*)alignTo(((uint8*)data + oldest->entrySize), 16);
		newest = oldest;
	}
}

void undo_stack::reset()
{
	nextToWrite = memory;
	oldest = 0;
	newest = 0;
}

bool undo_stack::showHistory(bool& open)
{
	bool result = false;

	if (open)
	{
		if (ImGui::Begin(ICON_FA_HISTORY "  Undo history", &open))
		{
			if (ImGui::DisableableButton("Clear history", newest != 0))
			{
				reset();
			}

			ImGui::Separator();

			bool currentFound = false;

			bool clicked = false;
			entry_header* target = 0;
			int32 direction = 0;

			bool current = !newest;
			currentFound |= current;
			if (ImGui::Selectable("no changes##UndoHistory", current) && !current)
			{
				clicked = true;
				target = 0;
				direction = -1;
			}


			for (entry_header* entry = oldest; entry; entry = entry->newer)
			{
				ImGui::PushID(entry);

				bool current = entry == newest;
				currentFound |= current;
				if (ImGui::Selectable(entry->name, current) && !current)
				{
					clicked = true;
					target = entry;
					direction = currentFound ? 1 : -1;
				}

				ImGui::PopID();
			}

			assert(currentFound);

			if (clicked)
			{
				assert(direction != 0);

				while (newest != target)
				{
					(direction == 1) ? redo() : undo();
				}

				result = true;
			}
		}
		ImGui::End();
	}

	return result;
}

void undo_stack::verify()
{
	for (auto entry = oldest; entry; entry = entry->newer)
	{
		if (entry->older)
		{
			assert(entry->older->newer == entry);
		}
		if (entry->newer)
		{
			assert(entry->newer->older == entry);
		}
	}
}
