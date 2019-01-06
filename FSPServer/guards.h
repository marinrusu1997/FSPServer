#pragma once
#include <boost/msm/front/euml/euml.hpp>

BOOST_MSM_EUML_ACTION(guard_signin)
{
	template <class FSM, class EVT, class SourceState, class TargetState>
	bool operator()(EVT const&, FSM&, SourceState&, TargetState&)
	{
		std::cout << "client_msm_::guard_signin" << std::endl;
		return true;
	}
};

BOOST_MSM_EUML_ACTION(guard_signup)
{
	template <class FSM, class EVT, class SourceState, class TargetState>
	bool operator()(EVT const&, FSM&, SourceState&, TargetState&)
	{
		std::cout << "client_msm_::guard_signup" << std::endl;
		return true;
	}
};

BOOST_MSM_EUML_ACTION(guard_push_files)
{
	template <class FSM, class EVT, class SourceState, class TargetState>
	bool operator()(EVT const&, FSM&, SourceState&, TargetState&)
	{
		std::cout << "client_msm_::guard_push_files" << std::endl;
		return true;
	}
};