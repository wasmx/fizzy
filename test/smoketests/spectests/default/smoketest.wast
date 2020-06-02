;; unnamed module
(module
  (func (export "foo") (param i32) (drop (local.get 0)))
  (func (export "bar") (param i64) (drop (local.get 0)))
)

;; invoke command translates into "action" command in json
(invoke "foo" (i32.const 1))
(invoke "bar" (i64.const 1))

(register "Mod0")

;; named module
(module $Mod1
  (func (export "foo.i32") (result i32) (i32.const 2))
  (func (export "foo.i64") (result i64) (i64.const 4))
  (func (export "trap") (unreachable))
  (func $loop (export "loop") (call $loop))
  (global (export "glob") i32 (i32.const 55))
  (table (export "tab") 10 funcref)
  (memory (export "mem") 1)
)

(register "Mod1" $Mod1)

;; invoke function from named module
;; expected i32 value
(assert_return (invoke $Mod1 "foo.i32") (i32.const 2))

;; expected i64 value
(assert_return (invoke $Mod1 "foo.i64") (i64.const 4))

;; invoke without module name (last module)
(assert_return (invoke "foo.i64") (i64.const 4))

;; get global
(assert_return (get $Mod1 "glob") (i32.const 55))

(assert_trap (invoke $Mod1 "trap") "unreachable instruction")

(assert_exhaustion (invoke $Mod1 "loop") "call stack exhausted")

(assert_invalid (module (func $type-unary-operand-empty (drop))) "stack underflow")

(assert_malformed (module binary "") "unexpected end")

(module $Mod3
  (import "Mod0" "foo" (func (param i32)))
  (import "Mod1" "foo.i32" (func (result i32)))
  (import "Mod1" "glob" (global i32))
  (import "Mod1" "tab" (table 10 funcref))
  (import "Mod1" "mem" (memory 1))
)

(assert_unlinkable
  (module (import "Mod-unknown" "foo" (func)))
  "unknown module"
)
(assert_unlinkable
  (module (import "Mod0" "unknown" (func)))
  "unknown import"
)
(assert_unlinkable
  (module (import "Mod0" "unknown" (table 1 funcref)))
  "unknown import"
)
(assert_unlinkable
  (module (import "Mod0" "unknown" (memory 1)))
  "unknown import"
)
(assert_unlinkable
  (module (import "Mod0" "unknown" (global i32)))
  "unknown import"
)
(assert_unlinkable
  (module (memory 0) (data (i32.const 0) "a"))
  "data segment does not fit"
)

(assert_trap
  (module (func $main (unreachable)) (start $main))
  "unreachable"
)

;; cases that will be skipped

;; floating point module
(module
  (func (export "foo.f32") (result f32) (f32.const 1.23))
  (func (export "foo.f64") (result f64) (f64.const 4.56))
  (func (export "param.f64") (param f64) (drop (local.get 0)))
  (global (export "glob.f32") f32 (f32.const 5.5))
)

(assert_return (invoke "foo.f32") (f32.const 1.23))
(assert_return (invoke "foo.f64") (f64.const 4.56))
(invoke "param.f64" (f64.const 1))
(assert_return (get "glob.f32") (f32.const 5.5))

(register "Mod-unknown" $Mod-unknown)

;; module_type=text
(assert_malformed (module quote "") "error")
(assert_unlinkable (module quote "") "error")
