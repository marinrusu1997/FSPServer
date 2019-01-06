#pragma once
#include "priority_scheduler.h"
#include <experimental/thread_pool>
#include <experimental/future>
#include <thread>
#include "scheduler_priorities.h"

class request_scheduler final
{
public:

	request_scheduler(request_scheduler const&) = delete;
	request_scheduler& operator=(request_scheduler const&) = delete;

	static request_scheduler& get_instance() {
		static request_scheduler scheduler;
		return scheduler;
	}

	void	join_threads() 
	{
		request_threads_.join();
	}

	void	stop_threads()
	{
		scheduler_.stop();

		request_threads_.stop();
	}

	template <class F>
	auto	post(scheduler_priority _priority, F&& f)
	{
		static std::once_flag once_flag;
		std::call_once(once_flag, [this]() {
			for (size_t i = 0; i < no_of_threads; i++)
				std::experimental::dispatch(request_threads_, [this]() { scheduler_.run(); });
		});

		priority_scheduler::executor_type* executor_to_do_work = nullptr;

		switch (_priority)
		{
		case scheduler_priority::very_low:
		{
			executor_to_do_work = &this->very_low_;
		}
		break;
		case scheduler_priority::low:
		{
			executor_to_do_work = &this->low_;
		}
		break;
		case scheduler_priority::medium:
		{
			executor_to_do_work = &this->medium_;
		}
		break;
		case scheduler_priority::high:
		{
			executor_to_do_work = &this->high_;
		}
		break;
		case scheduler_priority::very_high:
		{
			executor_to_do_work = &this->very_high_;
		}
		break;
		}

		assert(executor_to_do_work != nullptr);

		return std::experimental::post(*executor_to_do_work, std::forward<F>(f));
	}

private:
	size_t											no_of_threads;
	std::experimental::thread_pool					request_threads_;
	mutable priority_scheduler						scheduler_;

	priority_scheduler::executor_type				very_low_;
	priority_scheduler::executor_type				low_;
	priority_scheduler::executor_type				medium_;
	priority_scheduler::executor_type				high_;
	priority_scheduler::executor_type				very_high_;
	
	request_scheduler() :
		no_of_threads(std::thread::hardware_concurrency()),
		request_threads_(no_of_threads),
		scheduler_(),

		very_low_(scheduler_, scheduler_priority::very_low),
		low_(scheduler_, scheduler_priority::low),
		medium_(scheduler_, scheduler_priority::medium),
		high_(scheduler_, scheduler_priority::high),
		very_high_(scheduler_, scheduler_priority::very_high)
	{}
};


