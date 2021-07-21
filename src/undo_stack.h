#pragma once

#include "memory.h"

typedef void(*undo_func)(void*);
typedef void(*redo_func)(void*);

struct undo_stack
{
	undo_stack();

	template <typename T>
	void pushAction(const char* name, undo_func undo, redo_func redo, const T& userData);
	void pushAction(const char* name, undo_func undo, redo_func redo, const void* userData, uint64 dataSize);

	bool undoPossible();
	bool redoPossible();

	const char* getUndoName();
	const char* getRedoName();

	void undo();
	void redo();

	void display();
	void verify();

private:
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
inline void undo_stack::pushAction(const char* name, undo_func undo, redo_func redo, const T& userData)
{
	static_assert(std::is_trivially_copyable_v<T>, "Undo entries must be trivially copyable.");
	static_assert(std::is_trivially_destructible_v<T>, "Undo entries must be trivially destructible.");

	pushAction(name, undo, redo, &userData, sizeof(T));
}
