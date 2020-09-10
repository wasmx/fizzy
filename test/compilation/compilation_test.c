// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include <fizzy/fizzy.h>

bool dummy(void);

bool dummy()
{
    return fizzy_validate(NULL, 0);
}
