
#include <iostream>
#include <string>

#include <boost/lexical_cast.hpp>

#include "server.hpp"



int main (int argc, char *argv[])
{
	try {
		if (argc < 4) {
			std::cout << "Usage: async <address> <port> <threads>\n";
			return 1001;
		}

		async::server srv( argv[1], argv[2], boost::lexical_cast<size_t>(argv[3]) );
		srv.start();
	}
	catch (...) {
		//STD_LOG( error, "Unexcpected exception" );
	}


	return 0;
}
