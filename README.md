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



Copyright: Teruaki Ata


