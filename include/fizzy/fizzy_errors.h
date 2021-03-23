// Fizzy: A fast WebAssembly interpreter
// Copyright 2021 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

/// Fizzy error codes.
/// @file
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum FizzyErrorCode
{
    FIZZY_SUCCESS = 0,
    FIZZY_ERROR_PARSER_INVALID_MODULE_PREFIX,
    FIZZY_ERROR_OTHER
} FizzyErrorCode;

#ifdef __cplusplus
}
#endif
