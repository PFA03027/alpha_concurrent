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

#ifndef ALCONCURRENT_INC_INTERNAL_CPP_STD_CONFIGURE_HPP_
#define ALCONCURRENT_INC_INTERNAL_CPP_STD_CONFIGURE_HPP_

// configuration for constexpr constructor
#if __cpp_constexpr >= 201304L
#define ALCC_INTERNAL_CONSTEXPR_CONSTRUCTOR_BODY constexpr
#else
#define ALCC_INTERNAL_CONSTEXPR_CONSTRUCTOR_BODY
#endif

// configuration for constexpr adaptation
#if __cplusplus >= 201703L
#define ALCC_INTERNAL_CPPSTD17_CONSTEXPR constexpr
#else
#define ALCC_INTERNAL_CPPSTD17_CONSTEXPR
#endif

// configura for nodiscard attribute adaptation
#if __has_cpp_attribute( nodiscard )
#define ALCC_INTERNAL_NODISCARD_ATTR [[nodiscard]]
#else
#define ALCC_INTERNAL_NODISCARD_ATTR
#endif

#endif
