ECS = require 'libecs'

-- {{{ test utilities

function assert_error(f, ...)
   local code, _ = pcall(f, ...)
   if code then
      error('should have raised an error', 2)
   end
end

-- }}}

-- {{{ entities

do
   local ecs = ECS({}, true)
   local e1 = ecs:add()
   local e2 = ecs:add()
   local e3 = ecs:add('mypool')

   local count = 0
   for e in ecs:entities() do
      assert(e == e1 or e == e2 or e == e3)
      count = count + 1
   end
   assert(count == 3)

   count = 0
   for e in ecs:entities{ pool = 'mypool' } do
      assert(e ~= e1 and e ~= e2 and e == e3)
      count = count + 1
   end
   assert(count == 1)

   ecs:remove(e1)

   count = 0
   for e in ecs:entities() do
      assert(e ~= e1 and (e == e2 or e == e3))
      count = count + 1
   end
   assert(count == 2)
end

-- }}}

-- {{{ add components

function add_components(dev_mode)
   old_assert_error = assert_error
   if not dev_mode then
      assert_error = function(f) f() end
   end

   local ecs = ECS({
      position = {
         x = 'number',
         y = 'number?'
      },
      direction = {
         dir = 'string'
      },
      private = 'free'
   }, dev_mode)
   
   -- set component
   local e1 = ecs:add()
   e1.position = { x = 34, y = 10 }
   assert(e1.position.y == 10)
   assert_error(function() e1.my_component = {} end)

   -- set component values
   e1.position.y = 18
   assert(e1.position.y == 18)
   assert_error(function() e1.position.z = 24 end)
   assert_error(function() e1.position.x = '24' end)
   assert_error(function() e1.position.x = nil end)
   e1.position.y = nil

   ecs.locked = true
   assert_error(function() e1.position.x = 24 end)
   ecs.locked = false

   ecs.locked = true
   assert_error(function() e1.position = { x = 38, y = 10 } end)
   ecs.locked = false

   assert_error(function() e1.position = { y = 10 } end)
   assert_error(function() e1.position = { y = 10, z = 8 } end)

   ecs.locked = true
   e1.private = { hello = 'world' }
   ecs.locked = false

   assert_error = old_assert_error
end
add_components(true)

-- }}}

-- {{{ development mode

add_components(false)

-- }}}

-- TODO components with special types (table, etc)

-- {{{ iterate components

do
   local ecs = ECS({
      position = {
         x = 'number',
         y = 'number?'
      },
      direction = {
         dir = 'string'
      }
   }, true)
   
   -- set component
   local e1 = ecs:add()
   e1.position = { x = 34, y = 10 }
   e1.direction = { dir = 'N' }

   local e2 = ecs:add()
   e2.position = { x = 12 }

   local count = 0
   for e in ecs:entities{ components = { 'position' } } do
      assert(e == e1 or e == e2)
      count = count + 1
   end
   assert(count == 2)

   count = 0
   for e in ecs:entities{ components = { 'position', 'direction' } } do
      assert(e == e1)
      count = count + 1
   end
   assert(count == 1)
end

-- }}}

-- {{{ events

do
   local ecs = ECS({}, true)
   ecs:add_event('inc_x', { value = 1 })
   ecs:add_event('inc_y')
   ecs:add_event('inc_y')

   local count = 0
   for e in ecs:events('inc_x') do
      count = count + 1
      assert(e.value == 1)
   end
   assert(count == 1)

   count = 0
   for e in ecs:events('inc_y') do count = count + 1 end
   assert(count == 2)

   ecs:clear_event_queue()

   count = 0
   for e in ecs:events('inc_y') do count = count + 1 end
   assert(count == 0)
end

-- }}}

-- {{{ systems
do
   local ecs = ECS({
      comp = { value = "number" }
   }, true)
   local e1 = ecs:add()
   e1.comp = { value = 1 }
   
   function adder(t)
      t.value = t.value + 1
   end

   ecs:start_frame()

   -- run singlethreaded
   local t = { value = 0 }
   ecs:run_st('adder 1', adder, t)
   assert(t.value == 1)

   -- check timer
   assert(ecs:timer()['adder 1'])

   -- singlethread non-mutable
   function change_stuff()
      e1.comp.value = 2
   end
   assert_error(function() ecs:run_st('change_stuff', change_stuff) end)
   assert(e1.comp.value == 1)

   -- multithread
   function add_wait(t)
      for i = 1,80 do
         t.value = t.value + 1
         love.timer.sleep(0.001)
      end
   end
   local t1 = { value = 0 }
   local t2 = { value = 0 }

   ECS.multithreaded = false

   ecs:reset_timer()
   ecs:start_frame()
   ecs:run_mt('multithread', {
      { 'wait1', add_wait, t1 },
      { 'wait2', add_wait, t2 },
   })
   ecs:join()
   assert(t1.value > 0)
   assert(t2.value > 0)

   local timer = ecs:timer()
   assert(timer.wait1 > 0)
   assert(timer.wait2 > 0)

   -- mutable
   ecs:run_mutable('change_stuff', change_stuff)
   assert(e1.comp.value == 2)
end

-- }}}

-- TODO print components, values

print('Tests ok!')
