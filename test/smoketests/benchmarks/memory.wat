(module
  (memory 1 1)
  (func (export "inc") (param i32)
    local.get 0
    local.get 0
    i32.load
    i32.const 1
    i32.add
    i32.store
  )
)
