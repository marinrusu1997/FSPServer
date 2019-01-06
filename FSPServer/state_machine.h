#pragma once
// back-end
#include <boost/msm/back/state_machine.hpp>
//front-end
#include <boost/msm/front/state_machine_def.hpp>
#include <boost/msm/front/euml/euml.hpp>
//std stuff
#include <string>
#include <memory>
#include <set>
#include <iostream>
//custom stuff
#include "actions.h"
#include "guards.h"

namespace msm = boost::msm;
namespace mpl = boost::mpl;

namespace
{
	// events
	struct authentication_info
	{
		authentication_info() = default;

		authentication_info(const std::string& nick, const std::string& pswd)
			: nickname_(std::move(nick)), password_(std::move(pswd))
		{}

		_NODISCARD const auto& nickname() const noexcept { return nickname_; }
		_NODISCARD const auto& password() const noexcept { return password_; }
	private:
		const std::string nickname_;
		const std::string password_;
	};

	struct signin_impl final : authentication_info, msm::front::euml::euml_event<signin_impl>
	{
		signin_impl() = default;

		signin_impl(const std::string& nick, const std::string& pswd) :
			authentication_info(nick,pswd)
		{}
	};

	struct signup_impl final : authentication_info, msm::front::euml::euml_event<signup_impl>
	{
		signup_impl() = default;

		signup_impl(const std::string& nick, const std::string& pswd) :
			authentication_info(nick, pswd)
		{}
	};

	struct push_files_impl final : msm::front::euml::euml_event<push_files_impl>
	{
		push_files_impl() : files_{nullptr}
		{}

		explicit push_files_impl(std::unique_ptr<std::set<std::string>> files_ref)
			: files_(std::move(files_ref))
		{}

		_NODISCARD const auto& files() const noexcept { return files_; }
	private:
		std::shared_ptr<std::set<std::string>> files_;
	};

	struct connection_closed_impl final : msm::front::euml::euml_event<connection_closed_impl>
	{};

	struct logout_impl final : msm::front::euml::euml_event<logout_impl>
	{};

	struct delete_account_impl final : msm::front::euml::euml_event<delete_account_impl>
	{};

	// define some dummy instances for use in the transition table
	signin_impl				signin;
	signup_impl				signup;
	push_files_impl			push_files;
	connection_closed_impl	connection_closed;
	logout_impl				logout;
	delete_account_impl		delete_account;

	// The list of FSM states
	// they have to be declared outside of the front-end only to make VC happy :(
	// note: gcc would have no problem

	struct Connected_impl : public msm::front::state<>, public msm::front::euml::euml_state<Connected_impl>
	{};

	struct Logged_impl : public msm::front::state<>, public msm::front::euml::euml_state<Logged_impl>
	{
		template <class Event, class FSM>
		void on_entry(Event const&, FSM&) { std::cout << "entering: Logged" << std::endl; }
		template <class Event, class FSM>
		void on_exit(Event const&, FSM&) { std::cout << "leaving: Logged" << std::endl; }
	};

	struct Running_impl : public msm::front::state<>, public msm::front::euml::euml_state<Running_impl>
	{
		template <class Event, class FSM>
		void on_entry(Event const&, FSM&) { std::cout << "entering: Running" << std::endl; }
		template <class Event, class FSM>
		void on_exit(Event const&, FSM&) { std::cout << "leaving: Running" << std::endl; }
	};

	struct Disconnect_impl : public msm::front::state<>, public msm::front::euml::euml_state<Disconnect_impl>
	{};

	//to make the transition table more readable
	Connected_impl const	Connected;
	Logged_impl const		Logged;
	Running_impl const		Running;
	Disconnect_impl const	Disconnected;

	// front-end: define the FSM structure 
	struct client_msm_ : public msm::front::state_machine_def<client_msm_ >
	{
		// the initial state of the client SM. Must be defined
		typedef Connected_impl initial_state;

		// Transition table for client
		BOOST_MSM_EUML_DECLARE_TRANSITION_TABLE((
			Connected + signin [guard_signin] / do_signin					== Logged,
			Connected + signup [guard_signup] / do_signup					== Logged,
			Connected + connection_closed									== Disconnected,
			//  +------------------------------------------------------------------------------+
			Logged + push_files [guard_push_files] / do_push_files			== Running,
			Logged + logout / do_logout										== Disconnected,
			Logged + delete_account / do_delete_account						== Disconnected,
			Logged + connection_closed 										== Disconnected,
			//  +------------------------------------------------------------------------------+
			Running + logout / do_logout									== Disconnected,
			Running + connection_closed										== Disconnected
			), transition_table)
		
		// Replaces the default no-transition response.
		template <class FSM, class Event>
		void no_transition(Event const& e, FSM&, int state)
		{
			std::cout << "no transition from state " << state
				<< " on event " << typeid(e).name() << std::endl;
		}
	};
	// Pick a back-end
	typedef msm::back::state_machine<client_msm_> client_connection;

	//
	// Testing utilities.
	//
	static char const* const state_names[] = { "Connected", "Logged", "Running", "Disconnected" };
	void pstate(client_connection const& c)
	{
		std::cout << " -> " << state_names[c.current_state()[0]] << std::endl;
	}

	void test()
	{
		client_connection p;
		// needed to start the highest-level SM. This will call on_entry and mark the start of the SM
		p.start();
		// go to Open, call on_exit on Empty, then action, then on_entry on Open
		p.process_event(push_files_impl{}); pstate(p);
		p.process_event(signin_impl{ "marin","235689" }); pstate(p);
		p.process_event(push_files_impl{ std::make_unique<std::set<std::string>>() }); pstate(p);
		p.process_event(logout_impl{}); pstate(p);

		// at this point client is in disconnected state      
		p.process_event(connection_closed_impl{}); pstate(p);
		
	}
}
