# Semi lock-free concurrent software asset
 Semi lock-free concurrent software asset

# Pre-requirement
* C++17 standard and library are required.
* POSIX pthread thread local storage API is required.

# fifo_list class in lf_fifo.hpp
Semi-lock free FIFO type queue

Template 1st parameter T should be trivially copyable.

In case of no avialable free node that carries a value, new node is allocated from heap internally.
In this case, this queue may be locked. And push() may trigger this behavior.

On the other hand, used free node will be recycled without a memory allocation. In this case, push() is lock free.

To reduce lock behavior, pre-allocated nodes are effective.
get_allocated_num() provides the number of the allocated nodes. This value is hint to configuration.

## Supplement
To resolve ABA issue, this FIFO queue uses hazard pointer approach.

Non lock free behavior cases are below;
* Construct a instance itself.
* Any initial API call by each thread.
* push() call in case of no free internal node.
** In case that template parameter ALLOW_TO_ALLOCATE is false, push() is lock free.


# stack_list class in lf_stack.hpp
Semi-lock free Stack type queue

Template 1st parameter T should be trivially copyable.

In case of no avialable free node that carries a value, new node is allocated from heap internally.
In this case, this queue may be locked. And push() may trigger this behavior.

On the other hand, used free node will be recycled without a memory allocation. In this case, push() is lock free.

To reduce lock behavior, pre-allocated nodes are effective.
get_allocated_num() provides the number of the allocated nodes. This value is hint to configuration.

## Supplement
To resolve ABA issue, this Stack queue uses hazard pointer approach.

Non lock free behavior cases are below;
* Construct a instance itself.
* Any initial API call by each thread.
* push() call in case of no free internal node.
** In case that template parameter ALLOW_TO_ALLOCATE is false, push() is lock free.


# dynamic_tls class in dynamic_tls.hpp
Support dynamic allocatable thread local storage.

When allocating dynamic allocatable thread local storage for a thread, it is not lock free. This will be happened by 1st access of a value instance.
After allocation, this class may be lock free. This depends whether pthread_getspecific() is lock free or not.

# stm.hpp
This is experimental code of STM.
Not resolved the memory leak issue yet.


# Important points
Whether the provided class is lock-free depends on whether the POSIX API for thread-local storage is lock-free.

If the POSIX thread-local storage API is lock-free, the main operations such as push () / pop () will behave as lock-free.

# License
License type: 0BSD license

Copyright (C) 2020 by Teruaki Ata <PFA03027@nifty.com>

Permission to use, copy, modify, and/or distribute this software for any purpose
with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
THIS SOFTWARE.
