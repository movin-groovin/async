#ifndef CCONNECTION_H
#define CCONNECTION_H

#include <memory>
#include <vector>

#include <cstdint>

#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>

#include "request_handler.h"



namespace async
{

	class cconnection:
		public std::enable_shared_from_this<cconnection>,
		private boost::noncopyable
	{
	public:
		using request_handler_type = std::unique_ptr<crequest_handler>;
		static const u_int64_t init_buffer_size = 8 * 1024;

	public:
		cconnection( boost::asio::io_service & io_srv, std::unique_ptr<crequest_handler> && hnd );

		boost::asio::ip::tcp::socket& get_socket() noexcept;

		void start();

	private:
		boost::asio::ip::tcp::socket m_sock;
		boost::asio::io_service::strand m_strand;
		std::vector<uint8_t> m_buf;
		request_handler_type m_hnd;
	};

}

#endif // CCONNECTION_H







