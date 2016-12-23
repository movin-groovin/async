
#include "server.hpp"
#include "cconnection.h"



namespace async
{

	server::server ( std::string const & host, std::string const & port, size_t threads_num ):
		m_acceptor(m_iosrv),
		m_signals(m_iosrv),
		m_thrgrp_size(threads_num)
	{
		m_signals.add(SIGINT);
		m_signals.add(SIGTERM);
		m_signals.add(SIGQUIT);
		m_signals.async_wait(
			std::bind(&server::process_signal, this, std::placeholders::_1, std::placeholders::_2)
		);

		boost::asio::ip::tcp::resolver resolver( m_iosrv );
		boost::asio::ip::tcp::resolver::query query( host, port );
		boost::asio::ip::tcp::endpoint endpoint(* resolver.resolve(query) );
		m_acceptor.open( endpoint.protocol() );
		m_acceptor.set_option( boost::asio::socket_base::reuse_address(true) );
		m_acceptor.bind(endpoint);
		m_acceptor.listen();

		start_accept();
	}

	void server::start_accept()
	{
		std::shared_ptr<cconnection> cnct(
			std::make_shared<cconnection>( m_iosrv, std::make_unique<crequest_handler>() )
		);
		m_acceptor.async_accept(
			cnct -> get_socket(),
			std::bind( &server::handle_accept, this, cnct, std::placeholders::_1 )
		);
	}

	void server::handle_accept( std::shared_ptr<cconnection> cnct, boost::system::error_code const & ec )
	{
		if (!ec) {
			cnct -> start();
		}
		else {
			// handle error
		}

		start_accept();
	}

	void server::start()
	{
		for (uint64_t i = 0; i < m_thrgrp_size; ++i)
		{
			m_thrgrp -> create_thread(
				[this] () { thread_worker(); }
			);
		}
		m_thrgrp -> join_all();
	}

	void server::thread_worker() noexcept
	{
		do {
			try {
				boost::system::error_code erc;
				m_iosrv.run( erc );

				if (erc) {
					// log error
				}
			}
			catch(std::exception & exc) {
				// log exception
			}
			catch (...) {
				// log unexpected exception
			}
		} while(true);
	}

	void server::stop()
	{
		m_iosrv.stop();
	}

	void server::process_signal(
		boost::system::error_code const & error,
		int signal_number
	) {
		switch(signal_number)
		{
			case SIGINT:
			case SIGTERM:
			case SIGQUIT:
				stop();
				break;
			default:
				;
			//
		}

		//
	}

}




