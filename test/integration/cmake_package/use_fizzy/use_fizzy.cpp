// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include <fizzy/fizzy.h>

int main()
{
    return fizzy_validate(nullptr, 0) ? 0 : 1;
}
