// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

//! This is a Rust interface to [Fizzy](https://github.com/wasmx/fizzy), a fast, deterministic, and pedantic WebAssembly interpreter.
//!
//! # Examples
//!
//! This is a generic example for parsing and instantiating a module, and executing a simple function with inputs and an output.
//!
//! ```
//! extern crate fizzy;
//!
//! fn main() {
//!     // This wasm binary exports a single sum(u32, u32) -> u32 function.
//!     let wasm = [
//!         0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x07, 0x01, 0x60, 0x02, 0x7f,
//!         0x7f, 0x01, 0x7f, 0x03, 0x02, 0x01, 0x00, 0x07, 0x07, 0x01, 0x03, 0x73, 0x75, 0x6d,
//!         0x00, 0x00, 0x0a, 0x09, 0x01, 0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6a, 0x0b,
//!     ];
//!     let module = fizzy::parse(&wasm).expect("parsing failed");
//!     let mut instance = module.instantiate().expect("instantiation failed");
//!     let result = instance
//!         .execute(
//!             "sum",
//!             &[fizzy::TypedValue::U32(42), fizzy::TypedValue::U32(24)],
//!         )
//!         .expect("execution failed");
//!     let result = result
//!         .expect("return value expected")
//!         .as_u32()
//!         .expect("u32 expected as a return type");
//!     assert_eq!(result, 66);
//! }
//! ```

mod constnonnull;
mod sys;

use std::ffi::{CStr, CString};
use std::ptr::NonNull;

use crate::constnonnull::ConstNonNull;

/// The various kinds of errors, which can be returned by any of the interfaces.
#[derive(Eq, PartialEq, Debug, Clone)]
pub enum Error {
    MalformedModule(String),
    InvalidModule(String),
    InstantiationFailed(String),
    MemoryAllocationFailed(String),
    Other(String),
    FunctionNotFound,
    ArgumentCountMismatch,
    ArgumentTypeMismatch,
    NoMemoryAvailable,
    InvalidMemoryOffsetOrSize,
    Trapped,
}

impl From<String> for Error {
    fn from(error: String) -> Self {
        Error::Other(error)
    }
}

impl From<&str> for Error {
    fn from(error: &str) -> Self {
        Error::Other(error.to_string())
    }
}

/// A safe container for handling the low-level FizzyError struct.
struct FizzyErrorBox(Box<sys::FizzyError>);

impl FizzyErrorBox {
    /// Create a safe, boxed, and zero initialised container.
    fn new() -> Self {
        FizzyErrorBox(Box::new(sys::FizzyError {
            code: 0,
            message: [0i8; 256],
        }))
    }

    /// Return a pointer passable to low-level functions (validate, parse, instantiate).
    ///
    /// # Safety
    /// The returned pointer is still onwed by this struct, and thus not valid after this struct goes out of scope.
    unsafe fn as_mut_ptr(&mut self) -> *mut sys::FizzyError {
        &mut *self.0
    }

    /// Return the underlying error code.
    fn code(&self) -> u32 {
        self.0.code
    }

    /// Return an owned String copy of the underlying message.
    fn message(&self) -> String {
        unsafe {
            CStr::from_ptr(self.0.message.as_ptr())
                .to_string_lossy()
                .into_owned()
        }
    }

    /// Return a translated error object.
    fn error(&self) -> Option<Error> {
        match self.code() {
            sys::FizzyErrorCode_FizzySuccess => None,
            sys::FizzyErrorCode_FizzyErrorMalformedModule => {
                Some(Error::MalformedModule(self.message()))
            }
            sys::FizzyErrorCode_FizzyErrorInvalidModule => {
                Some(Error::InvalidModule(self.message()))
            }
            sys::FizzyErrorCode_FizzyErrorInstantiationFailed => {
                Some(Error::InstantiationFailed(self.message()))
            }
            sys::FizzyErrorCode_FizzyErrorMemoryAllocationFailed => {
                Some(Error::MemoryAllocationFailed(self.message()))
            }
            sys::FizzyErrorCode_FizzyErrorOther => Some(Error::Other(self.message())),
            _ => panic!(),
        }
    }
}

/// Parse and validate the input according to WebAssembly 1.0 rules. Returns true if the supplied input is valid.
pub fn validate<T: AsRef<[u8]>>(input: T) -> Result<(), Error> {
    let mut err = FizzyErrorBox::new();
    let ret = unsafe {
        sys::fizzy_validate(
            input.as_ref().as_ptr(),
            input.as_ref().len(),
            err.as_mut_ptr(),
        )
    };
    if ret {
        debug_assert!(err.code() == 0);
        Ok(())
    } else {
        debug_assert!(err.code() != 0);
        Err(err.error().unwrap())
    }
}

/// A parsed and validated WebAssembly 1.0 module.
pub struct Module(ConstNonNull<sys::FizzyModule>);

impl Drop for Module {
    fn drop(&mut self) {
        unsafe { sys::fizzy_free_module(self.0.as_ptr()) }
    }
}

impl Clone for Module {
    fn clone(&self) -> Self {
        let ptr = unsafe { sys::fizzy_clone_module(self.0.as_ptr()) };
        // TODO: this can be zero in case of memory allocation error, should this be gracefully handled?
        assert!(!ptr.is_null());
        Module(unsafe { ConstNonNull::new_unchecked(ptr) })
    }
}

/// Parse and validate the input according to WebAssembly 1.0 rules.
pub fn parse<T: AsRef<[u8]>>(input: &T) -> Result<Module, Error> {
    let mut err = FizzyErrorBox::new();
    let ptr = unsafe {
        sys::fizzy_parse(
            input.as_ref().as_ptr(),
            input.as_ref().len(),
            err.as_mut_ptr(),
        )
    };
    if ptr.is_null() {
        debug_assert!(err.code() != 0);
        Err(err.error().unwrap())
    } else {
        debug_assert!(err.code() == 0);
        Ok(Module(unsafe { ConstNonNull::new_unchecked(ptr) }))
    }
}

/// An instance of a module.
pub struct Instance(NonNull<sys::FizzyInstance>);

impl Drop for Instance {
    fn drop(&mut self) {
        unsafe { sys::fizzy_free_instance(self.0.as_ptr()) }
    }
}

impl Module {
    /// Create an instance of a module.
    // TODO: support imported functions
    pub fn instantiate(self) -> Result<Instance, Error> {
        let mut err = FizzyErrorBox::new();
        let ptr = unsafe {
            sys::fizzy_instantiate(
                self.0.as_ptr(),
                std::ptr::null(),
                0,
                std::ptr::null(),
                std::ptr::null(),
                std::ptr::null(),
                0,
                sys::FizzyMemoryPagesLimitDefault,
                err.as_mut_ptr(),
            )
        };
        // Forget Module (and avoid calling drop) because it has been consumed by instantiate (even if it failed).
        core::mem::forget(self);
        if ptr.is_null() {
            debug_assert!(err.code() != 0);
            Err(err.error().unwrap())
        } else {
            debug_assert!(err.code() == 0);
            Ok(Instance(unsafe { NonNull::new_unchecked(ptr) }))
        }
    }
}

/// A WebAssembly value of i32/i64/f32/f64.
pub type Value = sys::FizzyValue;

impl Value {
    pub fn as_i32(&self) -> i32 {
        unsafe { self.i32 as i32 }
    }
    pub fn as_u32(&self) -> u32 {
        unsafe { self.i32 }
    }
    pub fn as_i64(&self) -> i64 {
        unsafe { self.i64 as i64 }
    }
    pub fn as_u64(&self) -> u64 {
        unsafe { self.i64 }
    }
    pub fn as_f32(&self) -> f32 {
        unsafe { self.f32 }
    }
    pub fn as_f64(&self) -> f64 {
        unsafe { self.f64 }
    }
}

impl From<i32> for Value {
    fn from(v: i32) -> Self {
        Value { i32: v as u32 }
    }
}

impl From<u32> for Value {
    fn from(v: u32) -> Self {
        Value { i32: v }
    }
}

impl From<i64> for Value {
    fn from(v: i64) -> Self {
        Value { i64: v as u64 }
    }
}

impl From<u64> for Value {
    fn from(v: u64) -> Self {
        Value { i64: v }
    }
}
impl From<f32> for Value {
    fn from(v: f32) -> Self {
        Value { f32: v }
    }
}

impl From<f64> for Value {
    fn from(v: f64) -> Self {
        Value { f64: v }
    }
}

/// A WebAssembly value i32/i64/f32/f64 with its type specified.
pub enum TypedValue {
    U32(u32),
    U64(u64),
    F32(f32),
    F64(f64),
}

impl TypedValue {
    fn get_type(&self) -> sys::FizzyValueType {
        match self {
            TypedValue::U32(_) => sys::FizzyValueTypeI32,
            TypedValue::U64(_) => sys::FizzyValueTypeI64,
            TypedValue::F32(_) => sys::FizzyValueTypeF32,
            TypedValue::F64(_) => sys::FizzyValueTypeF64,
        }
    }

    pub fn as_i32(&self) -> Option<i32> {
        match self {
            TypedValue::U32(v) => Some(*v as i32),
            _ => None,
        }
    }
    pub fn as_u32(&self) -> Option<u32> {
        match self {
            TypedValue::U32(v) => Some(*v),
            _ => None,
        }
    }
    pub fn as_i64(&self) -> Option<i64> {
        match self {
            TypedValue::U64(v) => Some(*v as i64),
            _ => None,
        }
    }
    pub fn as_u64(&self) -> Option<u64> {
        match self {
            TypedValue::U64(v) => Some(*v),
            _ => None,
        }
    }
    pub fn as_f32(&self) -> Option<f32> {
        match self {
            TypedValue::F32(v) => Some(*v),
            _ => None,
        }
    }
    pub fn as_f64(&self) -> Option<f64> {
        match self {
            TypedValue::F64(v) => Some(*v),
            _ => None,
        }
    }
}

impl From<&TypedValue> for sys::FizzyValue {
    fn from(v: &TypedValue) -> sys::FizzyValue {
        match v {
            TypedValue::U32(v) => sys::FizzyValue { i32: *v },
            TypedValue::U64(v) => sys::FizzyValue { i64: *v },
            TypedValue::F32(v) => sys::FizzyValue { f32: *v },
            TypedValue::F64(v) => sys::FizzyValue { f64: *v },
        }
    }
}

/// The result of an execution.
pub struct ExecutionResult(sys::FizzyExecutionResult);

impl ExecutionResult {
    /// True if execution has resulted in a trap.
    pub fn trapped(&self) -> bool {
        self.0.trapped
    }

    /// The optional return value. Only a single return value is allowed in WebAssembly 1.0.
    pub fn value(&self) -> Option<Value> {
        if self.0.has_value {
            assert!(!self.0.trapped);
            Some(self.0.value)
        } else {
            None
        }
    }

    pub fn typed_value(&self, expected_type: sys::FizzyValueType) -> Option<TypedValue> {
        assert!(!self.0.trapped);
        if expected_type != sys::FizzyValueTypeVoid {
            assert!(self.0.has_value);
            Some(match expected_type {
                sys::FizzyValueTypeI32 => TypedValue::U32(unsafe { self.0.value.i32 }),
                sys::FizzyValueTypeI64 => TypedValue::U64(unsafe { self.0.value.i64 }),
                sys::FizzyValueTypeF32 => TypedValue::F32(unsafe { self.0.value.f32 }),
                sys::FizzyValueTypeF64 => TypedValue::F64(unsafe { self.0.value.f64 }),
                _ => panic!(),
            })
        } else {
            assert!(!self.0.has_value);
            None
        }
    }
}

impl Instance {
    /// Ensure the range is valid according to the currently available memory size.
    fn checked_memory_range(
        memory_data: *mut u8,
        memory_size: usize,
        offset: u32,
        size: usize,
    ) -> Result<core::ops::Range<usize>, Error> {
        // This is safe given usize::BITS >= u32::BITS, see https://doc.rust-lang.org/std/primitive.usize.html.
        let offset = offset as usize;
        if memory_data.is_null() {
            return Err(Error::NoMemoryAvailable);
        }
        if offset.checked_add(size).is_none() || (offset + size) > memory_size {
            return Err(Error::InvalidMemoryOffsetOrSize);
        }
        Ok(offset..offset + size)
    }

    /// Obtain a read-only slice of underlying memory.
    ///
    /// # Safety
    /// These slices turn invalid if the memory is resized (i.e. via the WebAssembly `memory.grow` instruction)
    pub unsafe fn checked_memory_slice(&self, offset: u32, size: usize) -> Result<&[u8], Error> {
        let memory_data = sys::fizzy_get_instance_memory_data(self.0.as_ptr());
        let memory_size = sys::fizzy_get_instance_memory_size(self.0.as_ptr());
        let range = Instance::checked_memory_range(memory_data, memory_size, offset, size)?;
        // Slices allow empty length, but data must be a valid pointer.
        debug_assert!(!memory_data.is_null());
        let memory = std::slice::from_raw_parts(memory_data, memory_size);
        Ok(&memory[range])
    }

    /// Obtain a mutable slice of underlying memory.
    ///
    /// # Safety
    /// These slices turn invalid if the memory is resized (i.e. via the WebAssembly `memory.grow` instruction)
    pub unsafe fn checked_memory_slice_mut(
        &mut self,
        offset: u32,
        size: usize,
    ) -> Result<&mut [u8], Error> {
        let memory_data = sys::fizzy_get_instance_memory_data(self.0.as_ptr());
        let memory_size = sys::fizzy_get_instance_memory_size(self.0.as_ptr());
        let range = Instance::checked_memory_range(memory_data, memory_size, offset, size)?;
        // Slices allow empty length, but data must be a valid pointer.
        debug_assert!(!memory_data.is_null());
        let memory = std::slice::from_raw_parts_mut(memory_data, memory_size);
        Ok(&mut memory[range])
    }

    /// Returns the current memory size, in bytes.
    pub fn memory_size(&self) -> usize {
        unsafe { sys::fizzy_get_instance_memory_size(self.0.as_ptr()) }
    }

    /// Copies memory from `offset` to `target`, for the length of `target.len()`.
    pub fn memory_get(&self, offset: u32, target: &mut [u8]) -> Result<(), Error> {
        let slice = unsafe { self.checked_memory_slice(offset, target.len())? };
        target.copy_from_slice(slice);
        Ok(())
    }

    /// Copies memory from `source` to `offset`, for the length of `source.len()`.
    pub fn memory_set(&mut self, offset: u32, source: &[u8]) -> Result<(), Error> {
        let slice = unsafe { self.checked_memory_slice_mut(offset, source.len())? };
        slice.copy_from_slice(source);
        Ok(())
    }

    /// Get a read-only pointer to the module.
    unsafe fn get_module(&self) -> *const sys::FizzyModule {
        sys::fizzy_get_instance_module(self.0.as_ptr())
    }

    /// Find index of exported function by name.
    pub fn find_exported_function_index(&self, name: &str) -> Option<u32> {
        let module = unsafe { self.get_module() };
        let name = CString::new(name).expect("CString::new failed");
        let mut func_idx: u32 = 0;
        let found = unsafe {
            sys::fizzy_find_exported_function_index(module, name.as_ptr(), &mut func_idx)
        };
        if found {
            Some(func_idx)
        } else {
            None
        }
    }

    /// Unsafe execution of a given function index `func_idx` with the given values `args`.
    ///
    /// An invalid index, invalid inputs, or invalid depth can cause undefined behaviour.
    ///
    /// # Safety
    /// This function expects a valid `func_idx` and appropriate number of `args`.
    pub unsafe fn unsafe_execute(&mut self, func_idx: u32, args: &[Value]) -> ExecutionResult {
        ExecutionResult(sys::fizzy_execute(
            self.0.as_ptr(),
            func_idx,
            args.as_ptr(),
            std::ptr::null_mut(),
        ))
    }

    /// Find function type for a given index. Must be a valid index otherwise behaviour is undefined.
    unsafe fn get_function_type(&self, func_idx: u32) -> sys::FizzyFunctionType {
        let module = self.get_module();
        sys::fizzy_get_function_type(module, func_idx)
    }

    /// Execute a given function of `name` with the given values `args`.
    ///
    /// An error is returned if the function can not be found, inappropriate number of arguments are passed,
    /// or the supplied types are mismatching.
    pub fn execute(
        &mut self,
        name: &str,
        args: &[TypedValue],
    ) -> Result<Option<TypedValue>, Error> {
        let func_idx = self
            .find_exported_function_index(name)
            .ok_or(Error::FunctionNotFound)?;

        let func_type = unsafe { self.get_function_type(func_idx) };
        if func_type.inputs_size != args.len() {
            return Err(Error::ArgumentCountMismatch);
        }

        // Validate input types.
        let supplied_types: Vec<sys::FizzyValueType> = args.iter().map(|v| v.get_type()).collect();
        let expected_types =
            unsafe { std::slice::from_raw_parts(func_type.inputs, func_type.inputs_size) };
        if expected_types != supplied_types {
            return Err(Error::ArgumentTypeMismatch);
        }

        // Translate to untyped raw values.
        let args: Vec<Value> = args.iter().map(|v| v.into()).collect();

        let ret = unsafe { self.unsafe_execute(func_idx, &args) };
        if ret.trapped() {
            Err(Error::Trapped)
        } else {
            Ok(ret.typed_value(func_type.output))
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn error_box() {
        let mut err = FizzyErrorBox::new();
        assert_ne!(unsafe { err.as_mut_ptr() }, std::ptr::null_mut());
        assert_eq!(err.code(), 0);
        assert_eq!(err.message(), "");
        assert!(err.error().is_none());
    }

    #[test]
    fn value_conversions() {
        // NOTE: since the underlying type is a union, a conversion or access to other members is undefined

        let v: Value = i32::MIN.into();
        assert_eq!(v.as_i32(), i32::MIN);
        let v: Value = i32::MAX.into();
        assert_eq!(v.as_i32(), i32::MAX);

        let v: Value = u32::MIN.into();
        assert_eq!(v.as_u32(), u32::MIN);
        let v: Value = u32::MAX.into();
        assert_eq!(v.as_u32(), u32::MAX);

        let v: Value = i64::MIN.into();
        assert_eq!(v.as_i64(), i64::MIN);
        let v: Value = i64::MAX.into();
        assert_eq!(v.as_i64(), i64::MAX);

        let v: Value = u64::MIN.into();
        assert_eq!(v.as_u64(), u64::MIN);
        let v: Value = u64::MAX.into();
        assert_eq!(v.as_u64(), u64::MAX);

        let v: Value = f32::MIN.into();
        assert_eq!(v.as_f32(), f32::MIN);
        let v: Value = f32::MAX.into();
        assert_eq!(v.as_f32(), f32::MAX);

        let v: Value = f64::MIN.into();
        assert_eq!(v.as_f64(), f64::MIN);
        let v: Value = f64::MAX.into();
        assert_eq!(v.as_f64(), f64::MAX);
    }

    #[test]
    fn typed_value_conversion() {
        let v = TypedValue::U32(u32::MIN);
        assert_eq!(v.as_u32().unwrap(), u32::MIN);
        assert_eq!(v.as_i32().unwrap(), 0);
        assert!(v.as_u64().is_none());
        assert!(v.as_i64().is_none());
        assert!(v.as_f32().is_none());
        assert!(v.as_f64().is_none());
        let v = TypedValue::U32(u32::MAX);
        assert_eq!(v.as_u32().unwrap(), u32::MAX);
        assert_eq!(v.as_i32().unwrap(), -1);
        assert!(v.as_u64().is_none());
        assert!(v.as_i64().is_none());
        assert!(v.as_f32().is_none());
        assert!(v.as_f64().is_none());

        let v = TypedValue::U64(u64::MIN);
        assert_eq!(v.as_u64().unwrap(), u64::MIN);
        assert_eq!(v.as_i64().unwrap(), 0);
        assert!(v.as_u32().is_none());
        assert!(v.as_i32().is_none());
        assert!(v.as_f32().is_none());
        assert!(v.as_f64().is_none());
        let v = TypedValue::U64(u64::MAX);
        assert_eq!(v.as_u64().unwrap(), u64::MAX);
        assert_eq!(v.as_i64().unwrap(), -1);
        assert!(v.as_u32().is_none());
        assert!(v.as_i32().is_none());
        assert!(v.as_f32().is_none());
        assert!(v.as_f64().is_none());

        let v = TypedValue::F32(f32::MIN);
        assert_eq!(v.as_f32().unwrap(), f32::MIN);
        let v = TypedValue::F32(f32::MAX);
        assert_eq!(v.as_f32().unwrap(), f32::MAX);
        assert!(v.as_u32().is_none());
        assert!(v.as_i32().is_none());
        assert!(v.as_u64().is_none());
        assert!(v.as_i64().is_none());

        let v = TypedValue::F64(f64::MIN);
        assert_eq!(v.as_f64().unwrap(), f64::MIN);
        let v = TypedValue::F64(f64::MAX);
        assert_eq!(v.as_f64().unwrap(), f64::MAX);
        assert!(v.as_u32().is_none());
        assert!(v.as_i32().is_none());
        assert!(v.as_u64().is_none());
        assert!(v.as_i64().is_none());
    }

    #[test]
    fn validate_wasm() {
        // Empty
        assert_eq!(
            validate(&[]).err().unwrap(),
            Error::MalformedModule("invalid wasm module prefix".to_string())
        );
        // Too short
        assert_eq!(
            validate(&[0x00]).err().unwrap(),
            Error::MalformedModule("invalid wasm module prefix".to_string())
        );
        // Valid
        assert!(validate(&[0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00]).is_ok());
        // Invalid version
        assert_eq!(
            validate(&[0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x01])
                .err()
                .unwrap(),
            Error::MalformedModule("invalid wasm module prefix".to_string())
        );
    }

    #[test]
    fn parse_wasm() {
        assert!(parse(&[0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00]).is_ok());
        assert_eq!(
            parse(&[0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x01])
                .err()
                .unwrap(),
            Error::MalformedModule("invalid wasm module prefix".to_string())
        );
    }

    #[test]
    fn instantiate_wasm() {
        let module = parse(&[0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00]);
        assert!(module.is_ok());
        let instance = module.unwrap().instantiate();
        assert!(instance.is_ok());
    }

    #[test]
    fn instantiate_wasm_missing_import() {
        /* wat2wasm
        (module
          (memory (import "mod" "m") 1)
        )
        */
        let input = hex::decode("0061736d01000000020a01036d6f64016d020001").unwrap();
        let module = parse(&input);
        assert!(module.is_ok());
        let instance = module.unwrap().instantiate();
        assert_eq!(
            instance.err().unwrap(),
            Error::InstantiationFailed(
                "module defines an imported memory but none was provided".to_string()
            )
        );
    }

    #[test]
    fn clone_module() {
        let module = parse(&[0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00]);
        assert!(module.is_ok());
        let module = module.unwrap();
        let module2 = module.clone();
        let instance = module.instantiate();
        assert!(instance.is_ok());
        let instance = module2.instantiate();
        assert!(instance.is_ok());
    }

    #[test]
    fn find_exported_function_index() {
        /* wat2wasm
        (module
          (func $f (export "foo") (result i32) (i32.const 42))
          (global (export "g1") i32 (i32.const 0))
          (table (export "tab") 0 anyfunc)
          (memory (export "mem") 1 2)
        )
        */
        let input = hex::decode(
        "0061736d010000000105016000017f030201000404017000000504010101020606017f0041000b07180403666f6f00000267310300037461620100036d656d02000a06010400412a0b").unwrap();

        let module = parse(&input);
        assert!(module.is_ok());
        let instance = module.unwrap().instantiate();
        assert!(instance.is_ok());
        let mut instance = instance.unwrap();

        let func_idx = instance.find_exported_function_index(&"foo");
        assert!(func_idx.is_some());
        assert_eq!(func_idx.unwrap(), 0);

        assert!(instance.find_exported_function_index(&"bar").is_none());
        assert!(instance.find_exported_function_index(&"g1").is_none());
        assert!(instance.find_exported_function_index(&"tab").is_none());
        assert!(instance.find_exported_function_index(&"mem").is_none());
    }

    #[test]
    fn unsafe_execute_wasm() {
        /* wat2wasm
          (func)
          (func (result i32) i32.const 42)
          (func (param i32 i32) (result i32)
            (i32.div_u (local.get 0) (local.get 1))
          )
          (func unreachable)
        */
        let input = hex::decode("0061736d01000000010e036000006000017f60027f7f017f030504000102000a150402000b0400412a0b0700200020016e0b0300000b").unwrap();
        let module = parse(&input);
        assert!(module.is_ok());
        let instance = module.unwrap().instantiate();
        assert!(instance.is_ok());
        let mut instance = instance.unwrap();

        let result = unsafe { instance.unsafe_execute(0, &[]) };
        assert!(!result.trapped());
        assert!(!result.value().is_some());

        let result = unsafe { instance.unsafe_execute(1, &[]) };
        assert!(!result.trapped());
        assert!(result.value().is_some());
        assert_eq!(result.value().unwrap().as_i32(), 42);

        // Explicit type specification
        let result =
            unsafe { instance.unsafe_execute(2, &[(42 as i32).into(), (2 as i32).into()]) };
        assert!(!result.trapped());
        assert!(result.value().is_some());
        assert_eq!(result.value().unwrap().as_i32(), 21);

        // Implicit i64 types (even though the code expects i32)
        let result = unsafe { instance.unsafe_execute(2, &[42.into(), 2.into()]) };
        assert!(!result.trapped());
        assert!(result.value().is_some());
        assert_eq!(result.value().unwrap().as_i32(), 21);

        let result = unsafe { instance.unsafe_execute(3, &[]) };
        assert!(result.trapped());
        assert!(!result.value().is_some());
    }

    #[test]
    fn execute_wasm() {
        /* wat2wasm
        (module
          (func (export "foo") (result i32) (i32.const 42))
          (func (export "bar") (param i32) (param i64) (result i32) (local.get 0) (i32.wrap_i64 (local.get 1)) (i32.add))
          (func (export "pi32") (param f32) (result f32) (local.get 0) (f32.const 3.14) (f32.div))
          (func (export "pi64") (param f64) (result f64) (local.get 0) (f64.const 3.14) (f64.div))
          (global (export "g1") i32 (i32.const 0))
          (table (export "tab") 0 anyfunc)
          (memory (export "mem") 1 2)
        )
        */
        let input = hex::decode(
        "0061736d010000000115046000017f60027f7e017f60017d017d60017c017c030504000102030404017000000504010101020606017f0041000b072c0703666f6f000003626172000104706933320002047069363400030267310300037461620100036d656d02000a29040400412a0b080020002001a76a0b0a00200043c3f54840950b0e002000441f85eb51b81e0940a30b").unwrap();

        let module = parse(&input);
        assert!(module.is_ok());
        let instance = module.unwrap().instantiate();
        assert!(instance.is_ok());
        let mut instance = instance.unwrap();

        // Successful execution.
        let result = instance.execute("foo", &[]);
        assert!(result.is_ok());
        assert_eq!(result.unwrap().unwrap().as_u32().unwrap(), 42);

        // Successful execution with arguments.
        let result = instance.execute("bar", &[TypedValue::U32(42), TypedValue::U64(24)]);
        assert!(result.is_ok());
        assert_eq!(result.unwrap().unwrap().as_u32().unwrap(), 66);

        // Successful execution with 32-bit float argument.
        let result = instance.execute("pi32", &[TypedValue::F32(0.5)]);
        assert!(result.is_ok());
        assert_eq!(result.unwrap().unwrap().as_f32().unwrap(), 0.15923566);

        // Successful execution with 64-bit float argument.
        let result = instance.execute("pi64", &[TypedValue::F64(0.5)]);
        assert!(result.is_ok());
        assert_eq!(
            result.unwrap().unwrap().as_f64().unwrap(),
            0.1592356687898089
        );

        // Non-function export.
        let result = instance.execute("g1", &[]);
        assert_eq!(result.err().unwrap(), Error::FunctionNotFound);

        // Export not found.
        let result = instance.execute("baz", &[]);
        assert_eq!(result.err().unwrap(), Error::FunctionNotFound);

        // Passing more arguments than required.
        let result = instance.execute("foo", &[TypedValue::U32(42)]);
        assert_eq!(result.err().unwrap(), Error::ArgumentCountMismatch);

        // Passing less arguments than required.
        let result = instance.execute("bar", &[]);
        assert_eq!(result.err().unwrap(), Error::ArgumentCountMismatch);

        // Passing mismatched types.
        let result = instance.execute("bar", &[TypedValue::F32(1.0), TypedValue::F64(2.0)]);
        assert_eq!(result.err().unwrap(), Error::ArgumentTypeMismatch);
    }

    #[test]
    fn no_memory() {
        /* wat2wasm
        (module)
        */
        let input = hex::decode("0061736d01000000").unwrap();

        let module = parse(&input);
        assert!(module.is_ok());
        let instance = module.unwrap().instantiate();
        assert!(instance.is_ok());
        let mut instance = instance.unwrap();

        assert_eq!(instance.memory_size(), 0);

        // If there is no memory, do not allow any slice.
        unsafe {
            assert_eq!(
                instance.checked_memory_slice(0, 0).err().unwrap(),
                Error::NoMemoryAvailable
            );
            assert_eq!(
                instance.checked_memory_slice_mut(0, 0).err().unwrap(),
                Error::NoMemoryAvailable
            );
            assert_eq!(
                instance.checked_memory_slice(0, 65536).err().unwrap(),
                Error::NoMemoryAvailable
            );
            assert_eq!(
                instance.checked_memory_slice_mut(0, 65536).err().unwrap(),
                Error::NoMemoryAvailable
            );
            assert_eq!(
                instance.checked_memory_slice(65535, 1).err().unwrap(),
                Error::NoMemoryAvailable
            );
            assert_eq!(
                instance.checked_memory_slice_mut(65535, 1).err().unwrap(),
                Error::NoMemoryAvailable
            );
            assert_eq!(
                instance.checked_memory_slice(65535, 2).err().unwrap(),
                Error::NoMemoryAvailable
            );
            assert_eq!(
                instance.checked_memory_slice_mut(65535, 2).err().unwrap(),
                Error::NoMemoryAvailable
            );
            assert_eq!(
                instance.checked_memory_slice(65536, 0).err().unwrap(),
                Error::NoMemoryAvailable
            );
            assert_eq!(
                instance.checked_memory_slice_mut(65536, 0).err().unwrap(),
                Error::NoMemoryAvailable
            );
            assert_eq!(
                instance.checked_memory_slice(65536, 1).err().unwrap(),
                Error::NoMemoryAvailable
            );
            assert_eq!(
                instance.checked_memory_slice_mut(65536, 1).err().unwrap(),
                Error::NoMemoryAvailable
            );
        }

        // Set memory via safe helper.
        assert_eq!(
            instance.memory_set(0, &[]).err().unwrap(),
            Error::NoMemoryAvailable
        );
        assert_eq!(
            instance.memory_set(0, &[0x11, 0x22]).err().unwrap(),
            Error::NoMemoryAvailable
        );
        // Get memory via safe helper.
        let mut dst: Vec<u8> = Vec::new();
        dst.resize(65536, 0);
        // Reading empty slice.
        assert_eq!(
            instance.memory_get(0, &mut dst[0..0]).err().unwrap(),
            Error::NoMemoryAvailable
        );
        // Reading 65536 bytes.
        assert_eq!(
            instance.memory_get(0, &mut dst).err().unwrap(),
            Error::NoMemoryAvailable
        );
    }

    #[test]
    fn empty_memory() {
        /* wat2wasm
        (module
          ;; Memory is allowed, but no memory is allocated at start.
          (memory 0)
        )
        */
        let input = hex::decode("0061736d010000000503010000").unwrap();

        let module = parse(&input);
        assert!(module.is_ok());
        let instance = module.unwrap().instantiate();
        assert!(instance.is_ok());
        let mut instance = instance.unwrap();

        assert_eq!(instance.memory_size(), 0);

        // If there is no memory, do not allow any slice.
        unsafe {
            assert!(instance.checked_memory_slice(0, 0).is_ok());
            assert!(instance.checked_memory_slice_mut(0, 0).is_ok());
            assert_eq!(
                instance.checked_memory_slice(0, 65536).err().unwrap(),
                Error::InvalidMemoryOffsetOrSize
            );
            assert_eq!(
                instance.checked_memory_slice_mut(0, 65536).err().unwrap(),
                Error::InvalidMemoryOffsetOrSize
            );
            assert_eq!(
                instance.checked_memory_slice(65535, 1).err().unwrap(),
                Error::InvalidMemoryOffsetOrSize
            );
            assert_eq!(
                instance.checked_memory_slice_mut(65535, 1).err().unwrap(),
                Error::InvalidMemoryOffsetOrSize
            );
            assert_eq!(
                instance.checked_memory_slice(65535, 2).err().unwrap(),
                Error::InvalidMemoryOffsetOrSize
            );
            assert_eq!(
                instance.checked_memory_slice_mut(65535, 2).err().unwrap(),
                Error::InvalidMemoryOffsetOrSize
            );
            assert_eq!(
                instance.checked_memory_slice(65536, 0).err().unwrap(),
                Error::InvalidMemoryOffsetOrSize
            );
            assert_eq!(
                instance.checked_memory_slice_mut(65536, 0).err().unwrap(),
                Error::InvalidMemoryOffsetOrSize
            );
            assert_eq!(
                instance.checked_memory_slice(65536, 1).err().unwrap(),
                Error::InvalidMemoryOffsetOrSize
            );
            assert_eq!(
                instance.checked_memory_slice_mut(65536, 1).err().unwrap(),
                Error::InvalidMemoryOffsetOrSize
            );
        }

        // Set memory via safe helper.
        assert!(instance.memory_set(0, &[]).is_ok());
        assert_eq!(
            instance.memory_set(0, &[0x11, 0x22]).err().unwrap(),
            Error::InvalidMemoryOffsetOrSize
        );
        // Get memory via safe helper.
        let mut dst: Vec<u8> = Vec::new();
        dst.resize(65536, 0);
        // Reading empty slice.
        assert!(instance.memory_get(0, &mut dst[0..0]).is_ok());
        // Reading 65536 bytes.
        assert_eq!(
            instance.memory_get(0, &mut dst).err().unwrap(),
            Error::InvalidMemoryOffsetOrSize
        );
    }

    #[test]
    fn memory() {
        /* wat2wasm
        (module
          (func (export "grow") (param i32) (result i32) (memory.grow (local.get 0)))
          (func (export "peek") (param i32) (result i32) (i32.load (local.get 0)))
          (func (export "poke") (param i32) (param i32) (i32.store (local.get 0) (local.get 1)))
          (memory (export "mem") 1 2)
        )
        */
        let input = hex::decode("0061736d01000000010b0260017f017f60027f7f00030403000001050401010102071c040467726f770000047065656b000104706f6b650002036d656d02000a1a030600200040000b070020002802000b0900200020013602000b").unwrap();
        let module = parse(&input);
        assert!(module.is_ok());
        let instance = module.unwrap().instantiate();
        assert!(instance.is_ok());
        let mut instance = instance.unwrap();

        assert_eq!(instance.memory_size(), 65536);
        unsafe {
            // Allow empty slices.
            assert!(instance.checked_memory_slice(0, 0).is_ok());
            assert!(instance.checked_memory_slice_mut(0, 0).is_ok());
            // Entire memory.
            assert!(instance.checked_memory_slice(0, 65536).is_ok());
            assert!(instance.checked_memory_slice_mut(0, 65536).is_ok());
            // Allow empty slices.
            assert!(instance.checked_memory_slice(65535, 0).is_ok());
            assert!(instance.checked_memory_slice_mut(65535, 0).is_ok());
            assert!(instance.checked_memory_slice(65536, 0).is_ok());
            assert!(instance.checked_memory_slice_mut(65536, 0).is_ok());
            // Single byte.
            assert!(instance.checked_memory_slice(65535, 1).is_ok());
            assert!(instance.checked_memory_slice_mut(65535, 1).is_ok());
            // Reading over.
            assert_eq!(
                instance.checked_memory_slice(65535, 2).err().unwrap(),
                Error::InvalidMemoryOffsetOrSize
            );
            assert_eq!(
                instance.checked_memory_slice_mut(65535, 2).err().unwrap(),
                Error::InvalidMemoryOffsetOrSize
            );
            assert_eq!(
                instance.checked_memory_slice(65536, 1).err().unwrap(),
                Error::InvalidMemoryOffsetOrSize
            );
            assert_eq!(
                instance.checked_memory_slice_mut(65536, 1).err().unwrap(),
                Error::InvalidMemoryOffsetOrSize
            );
            // Offset overflow.
            assert_eq!(
                instance.checked_memory_slice(65537, 0).err().unwrap(),
                Error::InvalidMemoryOffsetOrSize
            );
            assert_eq!(
                instance.checked_memory_slice_mut(65537, 0).err().unwrap(),
                Error::InvalidMemoryOffsetOrSize
            );
        }

        // Grow with a single page.
        let result = instance
            .execute("grow", &[TypedValue::U32(1)])
            .expect("successful execution");
        assert_eq!(
            result
                .expect("expected value")
                .as_u32()
                .expect("expected u32 result"),
            1
        );
        // Expect new total memory size.
        assert_eq!(instance.memory_size(), 65536 * 2);

        // Set memory via slices.
        unsafe {
            let mem = instance
                .checked_memory_slice_mut(0, 65536)
                .expect("valid mutable slice");
            assert_eq!(mem[0], 0);
            assert_eq!(mem[1], 0);
            assert_eq!(mem[2], 0);
            assert_eq!(mem[3], 0);
            mem[0] = 42;
        }
        unsafe {
            // Check that const slice matches up.
            let mem = instance.checked_memory_slice(0, 5).expect("valid slice");
            assert_eq!(mem[0], 42);
            assert_eq!(mem[1], 0);
            assert_eq!(mem[2], 0);
            assert_eq!(mem[3], 0);
        }
        let result = instance
            .execute("peek", &[TypedValue::U32(0)])
            .expect("successful execution");
        assert_eq!(
            result
                .expect("expected value")
                .as_u32()
                .expect("expected u32 result"),
            42
        );

        // Remember, by now we have grown memory to two pages.

        // Set memory via safe helper.
        assert!(instance.memory_set(0, &[]).is_ok());
        assert!(instance.memory_set(65536 + 65535, &[]).is_ok());
        assert!(instance.memory_set(65536 + 65536, &[]).is_ok());
        assert_eq!(
            instance.memory_set(65536 + 65537, &[]).err().unwrap(),
            Error::InvalidMemoryOffsetOrSize
        );
        assert!(instance.memory_set(0, &[0x11, 0x22, 0x33, 0x44]).is_ok());
        assert!(instance
            .memory_set(65536 + 65532, &[0x11, 0x22, 0x33, 0x44])
            .is_ok());
        assert_eq!(
            instance
                .memory_set(65536 + 65533, &[0x11, 0x22, 0x33, 0x44])
                .err()
                .unwrap(),
            Error::InvalidMemoryOffsetOrSize
        );
        assert_eq!(
            instance
                .memory_set(65536 + 65534, &[0x11, 0x22, 0x33, 0x44])
                .err()
                .unwrap(),
            Error::InvalidMemoryOffsetOrSize
        );
        assert_eq!(
            instance
                .memory_set(65536 + 65535, &[0x11, 0x22, 0x33, 0x44])
                .err()
                .unwrap(),
            Error::InvalidMemoryOffsetOrSize
        );
        assert_eq!(
            instance
                .memory_set(65536 + 65536, &[0x11, 0x22, 0x33, 0x44])
                .err()
                .unwrap(),
            Error::InvalidMemoryOffsetOrSize
        );
        assert_eq!(
            instance
                .memory_set(65536 + 65537, &[0x11, 0x22, 0x33, 0x44])
                .err()
                .unwrap(),
            Error::InvalidMemoryOffsetOrSize
        );

        let result = instance
            .execute("peek", &[TypedValue::U32(0)])
            .expect("successful execution");
        assert_eq!(
            result
                .expect("expected value")
                .as_u32()
                .expect("expected u32 result"),
            0x44332211
        );

        // Change memory via wasm.
        let result = instance
            .execute("poke", &[TypedValue::U32(0), TypedValue::U32(0x88776655)])
            .expect("successful execution");

        // Read memory via safe helper.
        let mut dst: Vec<u8> = Vec::new();
        dst.resize(65536, 0);
        // Reading 65536 bytes.
        assert!(instance.memory_get(0, &mut dst).is_ok());
        // Only checking the first 4.
        assert_eq!(dst[0..4], [0x55, 0x66, 0x77, 0x88]);

        // Read into empty slice.
        assert!(instance.memory_get(0, &mut dst[0..0]).is_ok());
        assert!(instance.memory_get(65536 + 65535, &mut dst[0..0]).is_ok());
        assert!(instance.memory_get(65536 + 65536, &mut dst[0..0]).is_ok());
        assert_eq!(
            instance
                .memory_get(65536 + 65537, &mut dst[0..0])
                .err()
                .unwrap(),
            Error::InvalidMemoryOffsetOrSize
        );

        // Read into short slice.
        assert!(instance.memory_get(0, &mut dst[0..4]).is_ok());
        assert!(instance.memory_get(65536 + 65532, &mut dst[0..4]).is_ok());
        assert_eq!(
            instance
                .memory_get(65536 + 65533, &mut dst[0..4])
                .err()
                .unwrap(),
            Error::InvalidMemoryOffsetOrSize
        );
        assert_eq!(
            instance
                .memory_get(65536 + 65534, &mut dst[0..4])
                .err()
                .unwrap(),
            Error::InvalidMemoryOffsetOrSize
        );
        assert_eq!(
            instance
                .memory_get(65536 + 65535, &mut dst[0..4])
                .err()
                .unwrap(),
            Error::InvalidMemoryOffsetOrSize
        );
        assert_eq!(
            instance
                .memory_get(65536 + 65536, &mut dst[0..4])
                .err()
                .unwrap(),
            Error::InvalidMemoryOffsetOrSize
        );
        assert_eq!(
            instance
                .memory_get(65536 + 65537, &mut dst[0..4])
                .err()
                .unwrap(),
            Error::InvalidMemoryOffsetOrSize
        );
    }

    #[test]
    fn execute_with_missing_import() {
        /* wat2wasm
        (module
          (func $adler32 (import "env" "adler32") (param i32 i32) (result i32))
          (memory (export "memory") 1)
          (data (i32.const 0) "abcd")
          (func (export "test") (result i32)
            i32.const 0
            i32.const 4
            call $adler32
          )
        )
        */
        let input = hex::decode(
        "0061736d01000000010b0260027f7f017f6000017f020f0103656e760761646c657233320000030201010503010001071102066d656d6f72790200047465737400010a0a0108004100410410000b0b0a010041000b0461626364").unwrap();

        let module = parse(&input);
        assert!(module.is_ok());
        let instance = module.unwrap().instantiate();
        assert!(instance.is_err());
        assert_eq!(
            instance.err().unwrap(),
            Error::InstantiationFailed(
                "module requires 1 imported functions, 0 provided".to_string()
            )
        );
    }

    #[test]
    fn execute_with_trap() {
        /* wat2wasm
        (module
          (func (export "test")
            unreachable
          )
        )
        */
        let input =
            hex::decode("0061736d0100000001040160000003020100070801047465737400000a05010300000b")
                .unwrap();

        let module = parse(&input);
        assert!(module.is_ok());
        let instance = module.unwrap().instantiate();
        assert!(instance.is_ok());
        let result = instance.unwrap().execute("test", &[]);
        assert!(result.is_err());
        assert_eq!(result.err().unwrap(), Error::Trapped);
    }
}
