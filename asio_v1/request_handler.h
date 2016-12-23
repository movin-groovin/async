#ifndef REQUEST_HANDLER_H
#define REQUEST_HANDLER_H

#include <memory>



namespace async
{

	class crequest_handler
	{
	public:
		crequest_handler();

		virtual ~ crequest_handler() = default;
	};

}

#endif // REQUEST_HANDLER_H
