#ifndef SERVER_HPP_
#define SERVER_HPP_

#include <memory>
#include <functional>
#include <type_traits>
#include <exception>

#include <cassert>

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/threadpool.hpp>
#include <boost/noncopyable.hpp>
#include <boost/system/error_code.hpp>

#include "logger.hpp"
#include "cconnection.h"



namespace async
{

	template <typename T>
	struct ret_traits
	{
		using type = std::unique_ptr<T>;

		template <typename Task, typename ... Args>
		static type return_result(Task & t, Args ... args)
		{
			return std::make_unique<T>( t(args...) );
		}
	};

	template <>
	struct ret_traits<void>
	{
		using type = void;

		template <typename Task, typename ... Args>
		static type return_result(Task & t, Args ... args)
		{
			return t(args...);
		}
	};


	template <typename TaskType, typename LoggerType = cbase_logger>
	class csafe_task
	{
	public:
		using task_type = TaskType;
		using logger_type = LoggerType;

	public:
		csafe_task() = default;

		csafe_task(task_type const & t): m_task(t) {}
		csafe_task(task_type && t): m_task( std::move(t) ) {}

		csafe_task& operator = (task_type const & t) { m_task = t; }
		csafe_task& operator = (task_type && t) { m_task = std::move(t); }

		template <typename ... Args>
		auto operator() (Args ... args)
		{
			using ret_type = typename std::result_of<task_type(Args...)>::type;

			try {
				return ret_traits<ret_type>::return_result( m_task, args... );
			}
			catch (std::exception & exc) {
				LOG( logger_type, error, exc.what() );
				return typename ret_traits<ret_type>::type();
			}
			catch (...) {
				LOG( logger_type, error, "Unexcpected exception" );
				return typename ret_traits<ret_type>::type();
			}

			assert( 1 != 1 );
		}

	private:
		task_type m_task;
	};


	class server: private boost::noncopyable
	{
	public:
		server( std::string const & host, std::string const & port, size_t threads_num );

		void start();

		void stop();

	private:
		void process_signal(
			boost::system::error_code const & error,
			int signal_number
		);

		void start_accept();

		void handle_accept( std::shared_ptr<cconnection> cnct, boost::system::error_code const & ec );

		void thread_worker() noexcept;

	private:
		boost::asio::io_service m_iosrv;
		boost::asio::ip::tcp::acceptor m_acceptor;
		boost::asio::signal_set m_signals;
		//boost::threadpool::thread_pool< std::function<void()> > m_thrpool;
		std::shared_ptr<boost::thread_group> m_thrgrp;
		size_t m_thrgrp_size;
	};

}

#endif // SERVER_HPP_




