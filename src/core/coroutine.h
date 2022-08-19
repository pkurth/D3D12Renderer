#pragma once


#if _MSVC_LANG >= 202002L

#include <coroutine>

template <typename param_t>
struct coroutine_return
{
	struct promise_type
	{
		param_t value;

		coroutine_return get_return_object()
		{
			return { .handle = std::coroutine_handle<promise_type>::from_promise(*this) };
		}
		std::suspend_never initial_suspend() { return {}; }
		std::suspend_always final_suspend() noexcept { return {}; } // Don't destroy after co_return. This means the caller must destroy, but he can check for .done().
		void return_void() {}
		void unhandled_exception() {}

		std::suspend_always yield_value(const param_t& value)
		{
			this->value = value;
			return {};
		}
	};

	std::coroutine_handle<promise_type> handle;
};

template <typename value_t>
struct coroutine
{
	std::coroutine_handle<typename coroutine_return<value_t>::promise_type> h;
	coroutine_return<value_t>::promise_type& promise;

	coroutine(coroutine_return<value_t> ret)
		: h(ret.handle), promise(h.promise()) {}

	const auto& value() const { return promise.value; }
	void operator()() { h(); }
	void destroy() { h.destroy(); }

	operator bool() const { return !h.done(); }
};


#if 0

	//Example:



	coroutine_return<uint32> counter()
	{
		for (uint32 i = 0; i < 3; ++i)
		{
			co_yield i;
		}
	}

	void call()
	{
		coroutine<uint32> h = counter();

		for (int i = 0; h; ++i)
		{
			std::cout << "counter3: " << h.value() << std::endl;
			h();
		}
		h.destroy();
	}

#endif



#endif
