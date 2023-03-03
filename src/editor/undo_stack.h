#pragma once

#include "core/memory.h"


struct undo_stack
{
	undo_stack();

	template <typename T>
	void pushAction(const char* name, const T& entry); // Type T must have member function void toggle().


	std::pair<bool, const char*> undoPossible();
	std::pair<bool, const char*> redoPossible();

	void undo();
	void redo();

	void reset();
	bool showHistory(bool& open);
	void verify();

private:
	typedef void (*toggle_func)(void*);

	void pushAction(const char* name, const void* entry, uint64 entrySize, toggle_func toggle);
	struct alignas(16) entry_header
	{
		toggle_func toggle;

		entry_header* newer;
		entry_header* older;

		const char* name;
		uint64 entrySize;
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
	//satic_assert(std::is_trivially_copyable_v<T>, "Undo entries must be trivially copyable.");
	static_assert(std::is_trivially_destructible_v<T>, "Undo entries must be trivially destructible.");

	toggle_func toggle = [](void* data)
	{
		T* t = (T*)data;
		t->toggle();
	};

	pushAction(name, &entry, sizeof(T), toggle);
}
