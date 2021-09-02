#pragma once

#include "core/memory.h"


struct dx_command_list;
struct common_material_info;

typedef void (*pipeline_setup_func)(dx_command_list*, const common_material_info&);
typedef void (*pipeline_render_func)(dx_command_list*, const mat4&, void*);

template <typename key_t>
struct render_command_buffer
{
private:
	struct command_key
	{
		key_t key;
		void* data;
	};

	struct command_header
	{
		pipeline_setup_func setup;
		pipeline_render_func render;
	};

	struct command_wrapper_base
	{
		command_header header;

		virtual ~command_wrapper_base() {}
	};

	std::vector<command_key> keys;
	memory_arena arena;

	template <typename pipeline_t, typename command_t>
	command_t& pushInternal(key_t sortKey)
	{
		struct command_wrapper : command_wrapper_base
		{
			command_t command;
		};

		command_wrapper* commandWrapper = arena.allocate<command_wrapper>();
		new (commandWrapper) command_wrapper;

		commandWrapper->header.setup = pipeline_t::setup;
		commandWrapper->header.render = [](dx_command_list* cl, const mat4& viewProj, void* data)
		{
			command_wrapper* wrapper = (command_wrapper*)data;
			pipeline_t::render(cl, viewProj, wrapper->command);
		};

		command_key key;
		key.key = sortKey;
		key.data = commandWrapper;

		keys.push_back(key);
		return commandWrapper->command;
	}

public:
	render_command_buffer()
	{
		arena.initialize(0, GB(4));
		keys.reserve(128);
	}

	uint64 size() const { return keys.size(); }
	void sort() { std::sort(keys.begin(), keys.end(), [](command_key a, command_key b) { return a.key < b.key; }); }

	template <typename pipeline_t, typename command_t, typename... args_t>
	command_t& emplace_back(key_t sortKey, args_t&&... args)
	{
		command_t& command = pushInternal<pipeline_t, command_t>(sortKey);
		new (&command) command_t(std::forward<args_t>(args)...);
		return command;
	}

	template <typename pipeline_t, typename command_t>
	void push_back(key_t sortKey, const command_t& commandToPush)
	{
		command_t& command = pushInternal<pipeline_t, command_t>(sortKey);
		new (&command) command_t(commandToPush);
	}

	template <typename pipeline_t, typename command_t>
	void push_back(key_t sortKey, command_t&& commandToPush)
	{
		command_t& command = pushInternal<pipeline_t, command_t>(sortKey);
		new (&command) command_t(std::move(commandToPush));
	}

	void clear()
	{
		for (auto& key : keys)
		{
			command_wrapper_base* wrapperBase = (command_wrapper_base*)key.data;
			wrapperBase->~command_wrapper_base();
		}

		arena.reset();
		keys.clear();
	}

	struct iterator_return : command_header
	{
		void* data;
	};

	struct iterator
	{
		typename std::vector<command_key>::const_iterator keyIterator;

		friend bool operator==(const iterator& a, const iterator& b) { return a.keyIterator == b.keyIterator; }
		friend bool operator!=(const iterator& a, const iterator& b) { return !(a == b); }
		iterator& operator++() { ++keyIterator; return *this; }

		iterator_return operator*() 
		{ 
			command_wrapper_base* wrapperBase = (command_wrapper_base*)keyIterator->data;
			void* data = keyIterator->data;
			return iterator_return{ wrapperBase->header, data };
		}
	};

	iterator begin() { return iterator{ keys.begin() }; }
	iterator end() { return iterator{ keys.end() }; }

	iterator begin() const { return iterator{ keys.begin() }; }
	iterator end() const { return iterator{ keys.end() }; }

};

template <typename key_t, typename value_t>
struct sort_key_vector
{
private:
	struct sort_key
	{
		key_t key;
		uint32 index;
	};

	std::vector<sort_key> keys;
	std::vector<value_t> values;

public:

	uint64 size() const { return keys.size(); }
	void reserve(uint64 newCapacity) { keys.reserve(newCapacity); values.reserve(newCapacity); }
	void resize(uint64 newSize) { keys.resize(newSize); values.resize(newSize); }

	void clear() { keys.clear(); values.clear(); }

	void push_back(key_t key, const value_t& value) { uint32 index = (uint32)size(); keys.push_back({ key, index }); values.push_back(value); }
	void push_back(key_t key, value_t&& value) { uint32 index = (uint32)size(); keys.push_back({ key, index }); values.push_back(std::move(value)); }

	template <typename... args_t>
	value_t& emplace_back(key_t key, args_t&&... args) { uint32 index = (uint32)size(); keys.push_back({ key, index }); return values.emplace_back(std::forward<args_t>(args)...); }

	void sort() { std::sort(keys.begin(), keys.end(), [](sort_key a, sort_key b) { return a.key < b.key; }); }

	struct iterator
	{
		typename std::vector<sort_key>::iterator keyIterator;
		value_t* values;

		friend bool operator==(const iterator& a, const iterator& b) { return a.keyIterator == b.keyIterator && a.values == b.values; }
		friend bool operator!=(const iterator& a, const iterator& b) { return !(a == b); }
		iterator& operator++() { ++keyIterator; return *this; }

		value_t& operator*() { return values[keyIterator->index]; }
	};

	struct const_iterator
	{
		typename std::vector<sort_key>::const_iterator keyIterator;
		const value_t* values;

		friend bool operator==(const const_iterator& a, const const_iterator& b) { return a.keyIterator == b.keyIterator && a.values == b.values; }
		friend bool operator!=(const const_iterator& a, const const_iterator& b) { return !(a == b); }
		const_iterator& operator++() { ++keyIterator; return *this; }

		const value_t& operator*() { return values[keyIterator->index]; }
	};

	iterator begin() { return iterator{ keys.begin(), values.data() }; }
	iterator end() { return iterator{ keys.end(), values.data() }; }
	
	const_iterator begin() const { return const_iterator{ keys.begin(), values.data() }; }
	const_iterator end() const { return const_iterator{ keys.end(), values.data() }; }
};



