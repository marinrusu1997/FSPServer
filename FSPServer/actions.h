#pragma once
#include <boost/msm/front/euml/euml.hpp>

BOOST_MSM_EUML_ACTION(Connected_Entry)
{
	template <class Event, class FSM, class STATE>
	void operator()(Event const&, FSM&, STATE&)
	{
		std::cout << "entering: Connected" << std::endl;
	}
};
BOOST_MSM_EUML_ACTION(Connected_Exit)
{
	template <class Event, class FSM, class STATE>
	void operator()(Event const&, FSM&, STATE&)
	{
		std::cout << "leaving: Connected" << std::endl;
	}
};

BOOST_MSM_EUML_ACTION(Logged_Entry)
{
	template <class Event, class FSM, class STATE>
	void operator()(Event const&, FSM&, STATE&)
	{
		std::cout << "entering: Logged" << std::endl;
	}
};
BOOST_MSM_EUML_ACTION(Logged_Exit)
{
	template <class Event, class FSM, class STATE>
	void operator()(Event const&, FSM&, STATE&)
	{
		std::cout << "leaving: Logged" << std::endl;
	}
};

BOOST_MSM_EUML_ACTION(Running_Entry)
{
	template <class Event, class FSM, class STATE>
	void operator()(Event const&, FSM&, STATE&)
	{
		std::cout << "entering: Running" << std::endl;
	}
};
BOOST_MSM_EUML_ACTION(Running_Exit)
{
	template <class Event, class FSM, class STATE>
	void operator()(Event const&, FSM&, STATE&)
	{
		std::cout << "leaving: Running" << std::endl;
	}
};

BOOST_MSM_EUML_ACTION(Disconnect_Entry)
{
	template <class Event, class FSM, class STATE>
	void operator()(Event const&, FSM&, STATE&)
	{
		std::cout << "starting: Disconnect" << std::endl;
	}
};
BOOST_MSM_EUML_ACTION(Disconnect_Exit)
{
	template <class Event, class FSM, class STATE>
	void operator()(Event const&, FSM&, STATE&)
	{
		std::cout << "finishing: Disconnect" << std::endl;
	}
};

// transition actions
BOOST_MSM_EUML_ACTION(do_signin)
{
	template <class FSM, class EVT, class SourceState, class TargetState>
	void operator()(EVT const&, FSM&, SourceState&, TargetState&)
	{
		std::cout << "client_msm_::do_signin" << std::endl;
	}
};
BOOST_MSM_EUML_ACTION(do_signup)
{
	template <class FSM, class EVT, class SourceState, class TargetState>
	void operator()(EVT const&, FSM&, SourceState&, TargetState&)
	{
		std::cout << "client_msm_::do_signup" << std::endl;
	}
};
BOOST_MSM_EUML_ACTION(do_push_files)
{
	template <class FSM, class EVT, class SourceState, class TargetState>
	void operator()(EVT const&, FSM&, SourceState&, TargetState&)
	{
		std::cout << "client_msm_::do_push_files" << std::endl;
	}
};
BOOST_MSM_EUML_ACTION(do_logout)
{
	template <class FSM, class EVT, class SourceState, class TargetState>
	void operator()(EVT const&, FSM&, SourceState&, TargetState&)
	{
		std::cout << "client_msm_::do_logout" << std::endl;
	}
};
BOOST_MSM_EUML_ACTION(do_delete_account)
{
	template <class FSM, class EVT, class SourceState, class TargetState>
	void operator()(EVT const&, FSM&, SourceState&, TargetState&)
	{
		std::cout << "client_msm_::do_delete_account" << std::endl;
	}
};

// Handler called when no_transition detected
BOOST_MSM_EUML_ACTION(Log_No_Transition)
{
	template <class FSM, class Event>
	void operator()(Event const& e, FSM&, int state)
	{
		std::cout << "Log_No_Transition no transition from state " << state
			<< " on event " << typeid(e).name() << std::endl;
	}
};