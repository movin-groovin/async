
// g++ -pthread -std=c++14 reactor.cpp -o reactor -lboost_system  -lboost_thread

#include <iostream>
#include <string>
#include <future>
#include <mutex>
#include <vector>
#include <functional>
#include <unordered_map>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <atomic>

#include <cstdint>
#include <cstring>
#include <cerrno>
#include <cassert>

#include <unistd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <boost/threadpool.hpp>



class reactor
{
public:
	using byte_type = uint8_t;
	using buffer_type = std::vector<byte_type>;
	using descriptor_type = int;
	using handler_operation_type = std::function<void(uint64_t)>;
	using threadpool_type = boost::threadpool::thread_pool< std::function<void()> >;
	
	struct hanlers_holder_type
	{
		handler_operation_type read_hnd;
		handler_operation_type write_hnd;
		handler_operation_type connect_hnd;
		handler_operation_type accept_hnd;
		handler_operation_type error_hnd;
	};
	
	struct descriptor_data_type
	{
		bool check;
		buffer_type* buffer;
		uint64_t offset;
		hanlers_holder_type hnds;
	};

private:
	void process_events(uint64_t ready_num)
	{
		decltype(m_fds) ready_fds;
		{
			std::lock_guard<std::mutex> lock(m_synch);
			for (uint64_t i = 0; i < ready_num; ++i)
			{
				descriptor_type fd = m_events[i].data.u32;
				ready_fds[fd] = m_fds[fd];
			}
		}
		
		for (auto & ready_fd : ready_fds)
		{
			uint64_t num;
			descriptor_type fd = ready_fd.first;
			auto & dat_ref = ready_fd.second;
			
			if (!dat_ref.check) {
				epoll_ctl( m_epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
				continue;
			}
			
			if (dat_ref.hnds.read_hnd) {
				num = read( fd, dat_ref.buffer->data() + dat_ref.offset, dat_ref.buffer->size() - dat_ref.offset );
				if (num == -1 && errno != EINTR)
				{
					dat_ref.check = false;
					dat_ref.hnds.error_hnd( errno );
					continue;
				}
				
				if (num != -1) dat_ref.offset += num;
				
				if (dat_ref.offset == dat_ref.buffer->size())
				{
					dat_ref.check = false;
					dat_ref.hnds.read_hnd( num );
				}
			}
			else if (dat_ref.hnds.write_hnd) {
				num = write( fd, dat_ref.buffer->data() + dat_ref.offset, dat_ref.buffer->size() - dat_ref.offset );
				if (num == -1 && errno != EINTR)
				{
					dat_ref.check = false;
					dat_ref.hnds.error_hnd( errno );
					continue;
				}
				
				if (num != -1) dat_ref.offset += num;
				
				if (dat_ref.offset == dat_ref.buffer->size())
				{
					dat_ref.check = false;
					dat_ref.hnds.write_hnd( num );
				}
			}
			else if (dat_ref.hnds.connect_hnd) {
				int val = 0;
				socklen_t val_len = sizeof val;
				int ret = getsockopt( fd, SOL_SOCKET, SO_ERROR, &val, &val_len );
				if (ret == -1)
				{
					// error of getsockopt
					dat_ref.check = false;
					dat_ref.hnds.error_hnd( errno );
					continue;
				}
				if (val != 0)
				{
					if (val == EINPROGRESS) continue;
					// error of connect
					dat_ref.check = false;
					dat_ref.hnds.error_hnd( ret );
					continue;
				}
				
				dat_ref.check = false;
				m_fds[fd].hnds.connect_hnd( 0 );
			}
			else if (dat_ref.hnds.accept_hnd) {
				// it is good to use: 'struct sockaddr *addr' and 'socklen_t *addrlen'
				int sock = accept(fd, nullptr, nullptr);
				if ( sock == -1 && (errno != EAGAIN && errno != EWOULDBLOCK) )
				{
					dat_ref.check = false;
					dat_ref.hnds.error_hnd( errno );
					continue;
				}
				
				if (sock > 0)
				{
					dat_ref.check = false;
					dat_ref.hnds.accept_hnd( sock );
				}
			}
			else {
				;
				assert( 1 != 1 );
				//std::runtime_error("No appropriate handler");
			}
		}
	}
	
	void worker_loop()
	{
		while( !m_stop.load() )
		{
			uint64_t ready_num = epoll_wait(
				m_epoll_fd,
				m_events.data(),
				m_events.size(),
				static_cast<uint64_t>(m_default_timeout_millisec)
			);
			
			if (ready_num == -1 && errno != EINTR) process_error(errno);
			
			process_events(ready_num);
			//std::this_thread::sleep_for( std::chrono::milliseconds(static_cast<uint64_t>(m_default_timeout_millisec)) );
		}
	}
	
	void process_error(int err_val)
	{
		std::vector<char> str_buf(1024);
		char* str_ret = strerror_r(err_val, str_buf.data(), str_buf.size() - 1);
		if (!str_ret)
			throw std::runtime_error("Unknown error, epoll_create");
		else
			throw std::runtime_error( (std::string() + ", epoll_create").c_str() );
	}
	
	void add_operation(descriptor_type fd, uint64_t operations, hanlers_holder_type & hnds, buffer_type & ref_buf)
	{
		epoll_event event { static_cast<uint32_t>(operations) };
		
		// EPOLLET ??? - EAGAIN - // http://stackoverflow.com/questions/9162712/what-is-the-purpose-of-epolls-edge-triggered-option
		if (-1 == epoll_ctl( m_epoll_fd, EPOLL_CTL_ADD, fd, &event))
		{
			process_error(errno);
		}
		
		std::lock_guard<std::mutex> lock(m_synch);
		auto it = m_fds.end();
		if ( m_fds.end() != (it = m_fds.find(fd)) ) {
			auto & ref_dat = m_fds[fd];
			ref_dat.check = true;
			ref_dat.buffer = &ref_buf;
			ref_dat.offset = decltype(ref_dat.offset)();
			ref_dat.hnds = hnds;
		} else {
			m_fds[fd] = descriptor_data_type {true, &ref_buf, 0, hnds};
		}
	}
	
public:
	reactor(threadpool_type & threadpool): m_events(m_events_per_time), m_threadpool(threadpool), m_stop(false)
	{
		if (-1 == (m_epoll_fd = epoll_create(1000)))
		{
			process_error(errno);
		}
	}
	
	void start()
	{
		m_threadpool.get().schedule( [this] () { worker_loop(); } );
	}
	
	void stop() noexcept
	{
		m_stop = true;
	}
	
	void add_read_operation (
		descriptor_type fd,
		handler_operation_type read_hnd,
		handler_operation_type error_hnd,
		buffer_type & ref_buf
	)
	{
		hanlers_holder_type hnds {
			read_hnd,
			handler_operation_type(),
			handler_operation_type(),
			handler_operation_type(),
			error_hnd
		};
		add_operation( fd, EPOLLIN | EPOLLET, hnds, ref_buf );
	}
	
	void add_write_operation (
		descriptor_type fd,
		handler_operation_type write_hnd,
		handler_operation_type error_hnd,
		buffer_type & ref_buf
	)
	{
		hanlers_holder_type hnds {
			handler_operation_type(),
			write_hnd,
			handler_operation_type(),
			handler_operation_type(),
			error_hnd
		};
		add_operation( fd, EPOLLOUT | EPOLLET, hnds, ref_buf );
	}
	
	void add_connect_operation (
		descriptor_type fd,
		handler_operation_type connect_hnd,
		handler_operation_type error_hnd,
		buffer_type & ref_buf,
		const struct sockaddr *addr,
		socklen_t addrlen
	)
	{
		int ret = connect(fd, addr, addrlen);
		if (ret == -1 && errno != EINPROGRESS)
		{
			error_hnd(errno);
			return;
		}
		
		hanlers_holder_type hnds {
			handler_operation_type(),
			handler_operation_type(),
			connect_hnd,
			handler_operation_type(),
			error_hnd
		};
		add_operation( fd, EPOLLIN | EPOLLET, hnds, ref_buf );
	}
	
	void add_accept_operation (
		descriptor_type fd,
		handler_operation_type accept_hnd,
		handler_operation_type error_hnd,
		buffer_type & ref_buf
	)
	{
		int sock = accept(fd, nullptr, nullptr);
		if ( sock == -1 && (errno != EAGAIN && errno != EWOULDBLOCK) )
		{
			error_hnd(errno);
			return;
		} else if (sock > 0)
		{
			accept_hnd(sock);
			return;
		}
		
		hanlers_holder_type hnds {
			handler_operation_type(),
			handler_operation_type(),
			handler_operation_type(),
			accept_hnd,
			error_hnd
		};
		add_operation( fd, EPOLLIN | EPOLLET, hnds, ref_buf );
	}
	
private:
	static const uint64_t m_default_timeout_millisec = 1;
	static const uint64_t m_events_per_time = 1024;
	
	std::mutex m_synch; // lock for m_fds
	std::unordered_map<descriptor_type, descriptor_data_type> m_fds;
	std::vector<epoll_event> m_events;
	
	std::reference_wrapper<threadpool_type> m_threadpool;
	descriptor_type m_epoll_fd;
	std::atomic<bool> m_stop;
};



int main ()
{
	try {
		uint64_t thr_number = !std::thread::hardware_concurrency() ? 2 : std::thread::hardware_concurrency();
		boost::threadpool::thread_pool< std::function<void()> > thr_pool( thr_number );
		reactor react(thr_pool);
		
		std::cout << "Start\n";
		react.start();
		std::this_thread::sleep_for( std::chrono::milliseconds( 5 * 1000 ) );
		react.stop();
		std::cout << "Stop\n";
	}
	catch (std::exception & exc) {
		std::cout << "Exception: " << exc.what() << "\n";
	}
	
	return 0;
}














