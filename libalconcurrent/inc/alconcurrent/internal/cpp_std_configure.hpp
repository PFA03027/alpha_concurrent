/**
 * @file configure.hpp
 * @author Teruaki Ata (PFA03027@nifty.com)
 * @brief
 * @version 0.1
 * @date 2024-05-01
 *
 * @copyright Copyright (c) 2024, Teruaki Ata (PFA03027@nifty.com)
 *
 */

#ifndef ALCONCURRENT_INTERNAL_CPP_STD_CONFIGURE_HPP_
#define ALCONCURRENT_INTERNAL_CPP_STD_CONFIGURE_HPP_

#if __cplusplus >= 201703L
#define ALCC_INTERNAL_CPPSTD17_CONSTEXPR constexpr
#else
#define ALCC_INTERNAL_CPPSTD17_CONSTEXPR
#endif

#endif
