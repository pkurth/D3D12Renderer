#pragma once

#include "core/memory.h"
#include "material.h"

struct dx_command_list;
struct common_render_data;

template <typename key_t, typename command_header>
struct render_command_buffer
{
private:
	struct command_key
	{
		key_t key;
		void* data;
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

		commandWrapper->header.initialize<pipeline_t, command_wrapper>();

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



struct default_command_header
{
	typedef void (*generic_pipeline_render_func)(dx_command_list*, const mat4&, void*);

	pipeline_setup_func setup;
	generic_pipeline_render_func render;

	template <typename pipeline_t, typename command_wrapper>
	void initialize()
	{
		setup = pipeline_t::setup;
		render = [](dx_command_list* cl, const mat4& viewProj, void* data)
		{
			command_wrapper* wrapper = (command_wrapper*)data;
			pipeline_t::render(cl, viewProj, wrapper->command);
		};
	}
};

template <typename key_t>
struct default_render_command_buffer : render_command_buffer<key_t, default_command_header> {};





struct depth_prepass_command_header
{
	typedef void (*generic_pipeline_render_func)(dx_command_list*, const mat4&, const mat4&, void*);

	pipeline_setup_func setup;
	generic_pipeline_render_func render;

	template <typename pipeline_t, typename command_wrapper>
	void initialize()
	{
		setup = pipeline_t::setup;
		render = [](dx_command_list* cl, const mat4& viewProj, const mat4& prevFrameViewProj, void* data)
		{
			command_wrapper* wrapper = (command_wrapper*)data;
			pipeline_t::render(cl, viewProj, prevFrameViewProj, wrapper->command);
		};
	}
};

template <typename key_t>
struct depth_prepass_render_command_buffer : render_command_buffer<key_t, depth_prepass_command_header> {};





struct compute_command_header
{
	typedef void (*generic_compute_func)(dx_command_list*, void*);

	generic_compute_func compute;

	template <typename pipeline_t, typename command_wrapper>
	void initialize()
	{
		compute = [](dx_command_list* cl, void* data)
		{
			command_wrapper* wrapper = (command_wrapper*)data;
			pipeline_t::compute(cl, wrapper->command);
		};
	}
};

template <typename key_t>
struct compute_command_buffer : render_command_buffer<key_t, compute_command_header> {};


