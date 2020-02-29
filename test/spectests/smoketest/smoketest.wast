;; unnamed module
(module
  (func (export "foo") (param i32) (drop (local.get 0)))
)

;; invoke command translates into "action" command in json
(invoke "foo" (i32.const 1))

;; named module
(module $Mod
  (func (export "foo.i32") (result i32) (i32.const 2))
  (func (export "foo.i64") (result i64) (i64.const 4))
  (func (export "trap") (unreachable))
)

(register "Mod" $Mod)

;; invoke function from named module
;; expected i32 value
(assert_return (invoke $Mod "foo.i32") (i32.const 2))

;; expected i64 value
(assert_return (invoke $Mod "foo.i64") (i64.const 4))

(assert_trap (invoke "trap") "unreachable instruction")

(assert_invalid (module (func $type-unary-operand-empty (drop))) "stack underflow")

(assert_malformed (module binary "") "unexpected end")

