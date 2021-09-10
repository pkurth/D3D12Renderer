#pragma once

#include "core/memory.h"


struct undo_stack
{
	undo_stack();

	template <typename T>
	void pushAction(const char* name, const T& entry); // Type T must have member functions void undo() and void redo().

	bool undoPossible();
	bool redoPossible();

	const char* getUndoName();
	const char* getRedoName();

	void undo();
	void redo();

	void display();
	void verify();

private:
	typedef void (*undo_func)(void*);
	typedef void (*redo_func)(void*);

	void pushAction(const char* name, const void* entry, uint64 dataSize, undo_func undo, redo_func redo);

	struct alignas(16) entry_header
	{
		undo_func undo;
		redo_func redo;

		entry_header* newer;
		entry_header* older;

		const char* name;
		uint64 dataSize;
	};

	uint8* memory;
	uint32 memorySize;

	uint8* nextToWrite;
	entry_header* oldest;
	entry_header* newest;
};

template<typename T>
inline void undo_stack::pushAction(const char* name, const T& entry)
{
	static_assert(std::is_trivially_copyable_v<T>, "Undo entries must be trivially copyable.");
	static_assert(std::is_trivially_destructible_v<T>, "Undo entries must be trivially destructible.");

	undo_func undo = [](void* data)
	{
		T* t = (T*)data;
		t->undo();
	};

	redo_func redo = [](void* data)
	{
		T* t = (T*)data;
		t->redo();
	};

	pushAction(name, &entry, sizeof(T), undo, redo);
}
