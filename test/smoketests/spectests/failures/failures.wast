;; Process with wast2json --no-check

;; no instantiated module, skipped
(assert_return (invoke "foo.i32") (i32.const 2))

;; invalid
(module (func (drop)))

;; registering two modules with the same name
(module)
(register "mod-empty")
(module)
(register "mod-empty")

;; unlinkable
(module (import "Mod-unknown" "foo" (func)))
;; unlinkable - data segment dosesn't fit
(module (memory 0) (data (i32.const 0) "a"))

;; malformed expected, invalid given
(assert_malformed (module (func (drop))) "error")
;; invalid expected, malformed given
(assert_invalid (module binary "") "error")
;; invalid expected, valid given
(assert_invalid (module (func (param i32) (drop (local.get 0)))) "error")
;; unlinkable expected, valid given
(assert_unlinkable (module (func (param i32) (drop (local.get 0)))) "error")
;; unlinkable expected, invalid given
(assert_unlinkable (module binary "") "error")

;; unlinkable expected, invalid given
(assert_unlinkable (module (func (drop))) "error")

;; uninstantiable expected, unlinkable given
(assert_trap (module (memory 0) (data (i32.const 0) "a")) "error")
(assert_trap (module (import "Mod-unknown" "foo" (func))) "error")
;; unlinkable expected, uninstantiable given
(assert_unlinkable (module (func $main (unreachable)) (start $main)) "error")

;; invalid result
(module
    (func (export "foo.i32") (result i32) (i32.const 1))
    (func (export "foo.i64") (result i64) (i64.const 1))
    (func (export "foo.f32") (result f32) (f32.const 1.234))
    (func (export "foo.f64") (result f64) (f64.const 1.234))
    (func (export "trap") (result i32) (unreachable))
)
(assert_return (invoke "foo.i32") (i32.const 2))
(assert_return (invoke "foo.i64") (i64.const 2))
(assert_return (invoke "foo.f32") (f32.const 2.456))
(assert_return (invoke "foo.f64") (f64.const 2.456))
(assert_return (invoke "trap") (i32.const 2))
(assert_return (invoke "foo.i32"))
(assert_trap (invoke "foo.i32") "error")

;; function not found, skipped
(assert_return (invoke "foo.unknown") (i32.const 2))
;; global not found
(assert_return (get "glob") (i32.const 55))
