;; unnamed module
(module
  (func (export "foo") (param i32) (drop (local.get 0)))
)

;; invoke command translates into "action" command in json
(invoke "foo" (i32.const 1))

(register "Mod0")

;; named module
(module $Mod1
  (func (export "foo.i32") (result i32) (i32.const 2))
  (func (export "foo.i64") (result i64) (i64.const 4))
  (func (export "trap") (unreachable))
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

(assert_invalid (module (func $type-unary-operand-empty (drop))) "stack underflow")

(assert_malformed (module binary "") "unexpected end")

(module $Mod3
  (import "Mod0" "foo" (func (param i32)))
  (import "Mod1" "foo.i32" (func (result i32)))
  (import "Mod1" "glob" (global i32))
  (import "Mod1" "tab" (table 10 funcref))
  (import "Mod1" "mem" (memory 1))
)
