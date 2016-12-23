
#include "cconnection.h"



namespace async
{

	cconnection::cconnection(
		boost::asio::io_service & io_srv,
		std::unique_ptr<crequest_handler> && hnd
	):
		m_sock(io_srv),
		m_strand(io_srv),
		m_buf(init_buffer_size),
		m_hnd( std::move(hnd) )
	{
		;
	}

	boost::asio::ip::tcp::socket& cconnection::get_socket() noexcept
	{
		return m_sock;
	}

	void cconnection::start()
	{
		;
	}

}









