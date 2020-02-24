//
// Created by danielsf97 on 2/20/20.
//

#ifndef P2PFS_EMPTY_VIEW_EXCEPTION_H
#define P2PFS_EMPTY_VIEW_EXCEPTION_H

#include <exception>

struct EmptyViewException : public std::exception
{
	const char * what () const throw ()
    {
    	return "Empty View Exception!!!";
    }
};

#endif //P2PFS_EMPTY_VIEW_EXCEPTION_H
