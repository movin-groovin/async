#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>
#include <iostream>



namespace async
{

	struct cbase_logger
	{
		static void log_error(std::string const & s) { std::cout << "Error: " << s << "\n"; }
		static void log_warning(std::string const & s) { std::cout << "Warning: " << s << "\n"; }
		static void log_info(std::string const & s) { std::cout << "Info: " << s << "\n"; }
	};

	// levels: error, warning, info
	#define LOG(log_class, level, fmt_str, ...)\
		log_class::log_##level(fmt_str, ##__VA_ARGS__)

	#define STD_LOG(level, fmt_str, ...)\
		LOG(cbase_logger, level, fmt_str, ##__VA_ARGS__)

}

#endif // LOGGER_HPP
