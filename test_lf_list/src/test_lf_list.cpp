//============================================================================
// Name        : test_lf_list.cpp
// Author      : Teruaki Ata
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C, Ansi-style
//============================================================================

#include <stdio.h>
#include <stdlib.h>

#include "lf_list.hpp"

int main( void )
{
	alpha::concurrent::lockfree_list<int> count_list;

	return EXIT_SUCCESS;
}
