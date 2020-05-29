(module
  (func $imported (import "env" "imported") (param i32) (result i32))
  (func (export "main") (param i32) (result i32)
    local.get 0
    call $imported
  )
)
