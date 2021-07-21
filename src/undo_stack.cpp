#include "pch.h"
#include "undo_stack.h"
#include "imgui.h"

undo_stack::undo_stack()
{
	memorySize = MB(2);
	memory = (uint8*)malloc(memorySize);
	nextToWrite = memory;
	oldest = 0;
	newest = 0;
}

void undo_stack::pushAction(const char* name, undo_func undo, redo_func redo, const void* userData, uint64 dataSize)
{
	uint64 requiredSpace = dataSize + sizeof(entry_header);
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
			void* oldestEnd = (uint8*)(oldest + 1) + oldest->dataSize;

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

	header->undo = undo;
	header->redo = redo;

	header->name = name;
	header->dataSize = dataSize;

	memcpy(data, userData, dataSize);

	nextToWrite = end;
}

bool undo_stack::undoPossible()
{
	return newest != 0;
}

bool undo_stack::redoPossible()
{
	return newest && newest->newer || !newest && oldest;
}

const char* undo_stack::getUndoName()
{
	return newest ? newest->name : 0;
}

const char* undo_stack::getRedoName()
{
	return (newest && newest->newer) ? newest->newer->name : (!newest && oldest) ? oldest->name : 0;
}

void undo_stack::undo()
{
	if (newest)
	{
		void* data = (void*)(newest + 1);
		newest->undo(data);

		newest = newest->older;
		if (newest)
		{
			nextToWrite = (uint8*)alignTo(((uint8*)data + newest->dataSize), 16);
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

		void* data = (void*)(newest + 1);
		newest->redo(data);

		nextToWrite = (uint8*)alignTo(((uint8*)data + newest->dataSize), 16);
	}

	if (!newest && oldest)
	{
		void* data = (void*)(oldest + 1);
		oldest->redo(data);

		nextToWrite = (uint8*)alignTo(((uint8*)data + oldest->dataSize), 16);
		newest = oldest;
	}
}

void undo_stack::display()
{
	if (ImGui::Begin("Undo stack"))
	{
		for (auto entry = oldest; entry; entry = entry->newer)
		{
			if (entry == newest)
			{
				ImGui::TextColored(ImVec4(1, 1, 0, 1), entry->name);
			}
			else
			{
				ImGui::Text(entry->name);
			}
		}
	}
	ImGui::End();
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
