# Semi lock-free concurrent software asset
 Semi lock-free concurrent software asset

# Pre-requirement
* C++17 standard is required.
* POSIX pthread thread local storage API is required.

# hazard_ptr.hpp
Generalized hazard pointer support package.
The below is non lock-free point.
* This classes in this header use new operator to allocate thread local hazard pointer management class.
  This point is not lock-free. On the other hand only this allocation  happened when a thread accesses a hazard pointer class at first.
* This classes in this header use std::list<T> to store the retired pointer. std::list<T> may allocate the memory from heap.
  This point is not lock-free.

# stm.hpp
This is experimental code of STM.


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
